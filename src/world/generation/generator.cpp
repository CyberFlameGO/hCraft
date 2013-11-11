/* 
 * hCraft - A custom Minecraft server.
 * Copyright (C) 2012-2013	Jacob Zhitomirsky (BizarreCake)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "world/generation/generator.hpp"
#include "world/world.hpp"
#include "world/chunk.hpp"
#include "player/player.hpp"
#include "system/server.hpp"
#include <functional>
#include <chrono>


namespace hCraft {
	
	chunk_generator::chunk_generator ()
	{
		this->th = nullptr;
		this->_running = false;
	}
	
	chunk_generator::~chunk_generator ()
	{
		this->stop ();
	}
	
	
	
	/* 
	 * Starts the internal thread and begins accepting generation requests.
	 */
	void
	chunk_generator::start ()
	{
		if (this->_running)
			return;
		
		this->_running = true;
		this->th = new std::thread (
			std::bind (std::mem_fn (&hCraft::chunk_generator::main_loop), this));
	}
	
	/* 
	 * Stops the generation thread and cleans up resources.
	 */
	void
	chunk_generator::stop ()
	{
		if (!this->_running)
			return;
		
		this->_running = false;
		if (this->th->joinable ())
			this->th->join ();
		
		for (generator_queue *q : this->queues)
			delete q;
		this->queues.clear ();
		this->index_map.clear ();
	}
	
	
	
	static generator_queue*
	_pick_queue (std::vector<generator_queue *>& queues)
	{
		if (queues.empty ())
			return nullptr;
		
		int max = 0;
		for (int i = 1; i < queues.size (); ++i)
			if ((queues[i]->counter > queues[max]->counter) && !queues[i]->requests.empty ())
				max = i;
		
		return queues[max];
	}
	
	/* 
	 * Where everything happens.
	 */
	void
	chunk_generator::main_loop ()
	{
		bool should_rest = false;
		int counter = 0;
		int req_counter = 0;
		
		while (this->_running)
			{
				if (should_rest)
					std::this_thread::sleep_for (std::chrono::milliseconds (4));
				else if (counter % 250 == 0)
					std::this_thread::sleep_for (std::chrono::milliseconds (20));
				should_rest = false;
				++ counter;
				
				if (!this->queues.empty ())
					{
						std::lock_guard<std::mutex> guard {this->request_mutex};
						
						generator_queue *q = _pick_queue (this->queues);
						q->counter = 0;
						if (!q->requests.empty ())
							{
								// pop request
								gen_request req = q->requests.front ();
								q->requests.pop ();
								++ req_counter;
								
								world *w = req.w;
								int flags = req.flags;
						
								player *pl = w->get_server ().player_by_id (req.pid);
								if (!pl) continue;
						
								if (!(flags & GFL_NOABORT) && (pl->get_world () != w || !pl->can_see_chunk (req.cx, req.cz)))
									{
										if (!(flags & GFL_NODELIVER))
											pl->deliver_chunk (w, req.cx, req.cz, nullptr, GFL_ABORTED, req.extra);
										continue;
									}
						
								if ((flags & GFL_NODELIVER) && (w->get_chunk (req.cx, req.cz) != nullptr))
									continue;
						
								// generate chunk
								chunk *ch = w->load_chunk (req.cx, req.cz);
								if (!ch) continue; // shouldn't happen :X
						
								// deliver
								if (!(flags & GFL_NODELIVER))
									pl->deliver_chunk (w, req.cx, req.cz, ch, GFL_NONE, req.extra);
								
							//--
								// increment counters
								for (generator_queue *q : this->queues)
									if (!q->requests.empty ())
										++ q->counter;
							}
					}
				
				should_rest = true;
			}
	}
	
	
	
	/* 
	 * Requests the chunk located at the given coordinates to be generated.
	 * The specified player is then informed when it's ready.
	 */
	void
	chunk_generator::request (world *w, int cx, int cz, int pid, int flags, int extra)
	{
		std::lock_guard<std::mutex> guard {this->request_mutex};
		
		generator_queue *q = nullptr;
		auto itr = this->index_map.find (pid);
		if (itr == this->index_map.end ())
			{
				q = new generator_queue ();
				q->pid = pid;
				q->counter = 0;
				
				this->index_map[pid] = this->queues.size ();
				this->queues.push_back (q);
			}
		else
			q = this->queues[itr->second];
		
		q->requests.push ({pid, w, cx, cz, flags, extra});
	}
	
	
	
	/* 
	 * Cancels all chunk requests for the given world.
	 */
	void
	chunk_generator::cancel_requests (world *w)
	{
		std::lock_guard<std::mutex> guard {this->request_mutex};
		
		for (generator_queue *q : this->queues)
			{
				std::queue<gen_request> valid_reqs;
				while (!q->requests.empty ())
					{
						gen_request req = q->requests.front ();
						q->requests.pop ();
				
						if (req.w != w)
							valid_reqs.push (req);
					}
		
				q->requests = valid_reqs;
			}
	}
}

