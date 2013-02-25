/* 
 * hCraft - A custom Minecraft server.
 * Copyright (C) 2012	Jacob Zhitomirsky
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

#include "block_physics.hpp"
#include "physics/physics.hpp"
#include "world.hpp"
#include <functional>
#include <chrono>

#include <iostream> // DEBUG


namespace hCraft {
	
	/* 
	 * Constructs and starts the worker thread.
	 */
	block_physics_worker::block_physics_worker ()
		: paused (false), ticks (0),
			updates (), update_lock (), _running (true),
		
			// and finally, the thread:
			th (std::bind (std::mem_fn (&hCraft::block_physics_worker::main_loop), this))
		{ }
	
	/* 
	 * Destructor - stops the worker thread.
	 */
	block_physics_worker::~block_physics_worker ()
	{
		this->_running = false;
		if (this->th.joinable ())
			this->th.join ();
	}
	
	
	/* 
	 * Where everything happens.
	 */
	void
	block_physics_worker::main_loop ()
	{
#define MAX_UPDATES_PER_ITER 1000
		while (this->_running)
			{
				std::this_thread::sleep_for (std::chrono::milliseconds (50));
				++ this->ticks;
				if (paused || this->updates.empty ())
					continue;
				
				int handled = 0;
				while (handled++ < MAX_UPDATES_PER_ITER)
					{
						physics_update u;
						{
							std::lock_guard<std::mutex> guard ((this->update_lock));
							if (this->updates.empty ())
								break;
							u = this->updates.front ();
							this->updates.pop_front ();
							
							this->remove_block (u.w, u.x, u.y, u.z);
						}
					
						world *wr = u.w;
						physics_block *pb = wr->get_physics_at (u.x, u.y, u.z);
						if (!pb) continue;
						
						if ((u.lt + pb->tick_rate ()) > this->ticks)
							{
								// NOTE: The source of slowness when over a thousand blocks are queued.
								std::lock_guard<std::mutex> guard {this->update_lock};
								this->updates.push_back (u);
								continue;
							}
						
						pb->tick (*wr, u.x, u.y, u.z, u.extra, nullptr);
					}
			}
	}
	
	
	
	bool
	block_physics_worker::update_exists (world *w, int x, int y, int z)
	{
		if (y < 0 || y > 255) return false;
		auto w_itr = this->block_mem.find (w);
		if (w_itr == this->block_mem.end ())
			return false;
		std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>&
			mem_chunks = w_itr->second;
		
		auto ch_itr = mem_chunks.find ({x >> 4, z >> 4});
		if (ch_itr == mem_chunks.end ())
			return false;
		ph_mem_chunk& ch = ch_itr->second;
		ph_mem_subchunk* sub = ch.subs[y >> 4];
		if (sub == nullptr)
			return false;
		return sub->bits.test (((y & 0xF) << 8) | ((z & 0xF) << 8) | (x & 0xF));
	}
	
	void
	block_physics_worker::add_block (world *w, int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>&
			mem_chunks = this->block_mem[w];
		
		ph_mem_chunk& ch = mem_chunks[{x >> 4, z >> 4}];
		ph_mem_subchunk* sub = ch.subs[y >> 4];
		if (sub == nullptr)
			sub = ch.subs[y >> 4] = new ph_mem_subchunk ();
		sub->bits.set (((y & 0xF) << 8) | ((z & 0xF) << 8) | (x & 0xF));
	}
	
	void
	block_physics_worker::remove_block (world *w, int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		auto w_itr = this->block_mem.find (w);
		if (w_itr == this->block_mem.end ())
			return;
		std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>&
			mem_chunks = w_itr->second;
		
		auto ch_itr = mem_chunks.find ({x >> 4, z >> 4});
		if (ch_itr == mem_chunks.end ())
			return;
		ph_mem_chunk& ch = ch_itr->second;
		ph_mem_subchunk* sub = ch.subs[y >> 4];
		if (sub == nullptr)
			return;
		sub->bits.reset (((y & 0xF) << 8) | ((z & 0xF) << 8) | (x & 0xF));
	}
	
	
	
//-----------
	
	/* 
	 * Changes the number of worker threads to utilize.
	 */
	void
	block_physics_manager::set_thread_count (unsigned int count)
	{
		if (count > 20) count = 20;
		std::lock_guard<std::mutex> guard {this->lock};
		
		if (count == this->workers.size ())
			return; // nothing to do
		
		if (count > this->workers.size ())
			{
				while (this->workers.size () < count)
					this->workers.emplace_back (new block_physics_worker ());
				return;
			}
		
		if (count == 0)
			{
				this->workers.clear ();
				return;
			}
		
		int to_remove = this->workers.size () - count;
		int total_updates = 0;
		for (size_t i = count; i < this->workers.size (); ++i)
			{
				block_physics_worker *worker = this->workers[i].get ();
				worker->paused = true;
				std::lock_guard<std::mutex> guard {worker->update_lock};
				total_updates += worker->updates.size ();
			}
		int updates_per_worker = total_updates / count;
		
		for (size_t i = 0; i < (this->workers.size () - to_remove); ++i)
			{
				block_physics_worker *worker = this->workers[i].get ();
				for (size_t j = count; j < this->workers.size (); ++j)
					{
						block_physics_worker *removee = this->workers[j].get ();
						for (int k = 0; k < updates_per_worker; ++k)
							{
								worker->updates.push_back (removee->updates.front ());
								removee->updates.pop_front ();
							}
					}
			}
		
		this->workers.resize (count);
	}
	
	
	
	static block_physics_worker*
	min_worker (std::vector<std::shared_ptr<block_physics_worker> >& workers)
	{
		if (workers.empty ()) return nullptr;
		
		size_t min = 0;
		for (size_t i = 1; i < workers.size (); ++i)
			if ((workers[i].get ()->update_count ()) < (workers[min].get ()->update_count ()))
					min = i;
		return workers[min].get ();
	}
	
	void
	block_physics_manager::queue_physics (world *w, int x, int y, int z, int extra)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		block_physics_worker *worker = min_worker (this->workers);
		if (worker)
			{
				std::lock_guard<std::mutex> inner_guard {worker->update_lock};
				worker->updates.emplace_back (w, x, y, z, extra, worker->ticks);
				worker->add_block (w, x, y, z);
			}
	}

	void
	block_physics_manager::queue_physics_nolock (world *w, int x, int y, int z, int extra)
	{
		block_physics_worker *worker = min_worker (this->workers);
		if (worker)
			worker->updates.emplace_back (w, x, y, z, extra, worker->ticks);
	}
	
	// Queues an update only if one with the same xyz coordinates does not
	// already exist.
	void
	block_physics_manager::queue_physics_once (world *w, int x, int y, int z, int extra)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		for (size_t i = 1; i < workers.size (); ++i)
			if ((workers[i].get ()->update_exists (w, x, y, z)))
				return;
		
		block_physics_worker *worker = min_worker (this->workers);
		if (worker)
			{
				std::lock_guard<std::mutex> inner_guard {worker->update_lock};
				worker->updates.emplace_back (w, x, y, z, extra, worker->ticks);
			}
	}
}

