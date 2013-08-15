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

#include "physics/physics.hpp"
#include "physics/physics.hpp"
#include "world.hpp"
#include "utils.hpp"
#include "server.hpp"
#include "stringutils.hpp"
#include "entities/entity.hpp"
#include "player.hpp"
#include <functional>
#include <cstring>

#include <iostream> // DEBUG


namespace hCraft {
	
	physics_manager::physics_manager (server &srv)
		: srv (srv)
		{ }
	
	
	
	physics_update::physics_update (int wid, int x, int y, int z, int data, int tick,
		std::chrono::steady_clock::time_point nt, physics_block_callback cb)
		: params (), nt (nt)
	{
		this->type = PU_BLOCK;
		
		this->wid = wid;
		this->data.blk.x = x;
		this->data.blk.y = y;
		this->data.blk.z = z;
		this->data.blk.cb = cb;
		this->data.blk.data = data;
		this->tick = tick;
		this->elapsed = 0;
	}
	
	physics_update::physics_update (int wid, int eid, bool persistent,
		int tick, std::chrono::steady_clock::time_point nt)
		: params (), nt (nt)
	{
		this->type = PU_ENTITY;
		
		this->wid = wid;
		this->data.ent.eid = eid;
		this->data.ent.persistent = persistent;
		this->tick = tick;
		this->elapsed = 0;
	}
		
		
		
	physics_params::physics_params ()
	{
		for (int i = 0; i < 8; ++i)
			this->actions[i].type = PA_NONE;
	}
	
	/* 
	 * Constructs a physics_params object from the given string.
	 * Returns true on success, and false in case of an error.
	 */
	bool
	physics_params::build (const std::string& str, physics_params& out, int def_expire)
	{
		out = physics_params (); // clear it out
		
		int index = 0, val;
		
		std::istringstream ss {str};
		std::string word, numstr;
		while (!ss.eof () && ss.good ())
			{
				ss >> word;
				if (ss.eof () || !ss.good ())
					return false;
				ss >> numstr;
				if (!sutils::is_int (numstr))
					return false;
				
				if (index == 7)
					return false;
				
				val = sutils::to_int (numstr);
				
				out.actions[index].expire = def_expire;
				out.actions[index].val = val;
				
				if (word.compare ("dissipate") == 0)
					out.actions[index].type = PA_DISSIPATE;
				else if (word.compare ("drop") == 0)
					out.actions[index].type = PA_DROP;
				else if (word.compare ("finite") == 0)
					out.actions[index].type = PA_FINITE;
				else
					return false;
				
				++ index;
			}
		
		out.actions[index++].type = PA_NONE;
		
		return true;
	}
	
	
	
	ph_mem_subchunk::ph_mem_subchunk ()
	{
		std::memset (this->blocks, 0, 4096 * sizeof (unsigned short));
	}
	
	
	
	/* 
	 * Constructs and starts the worker thread.
	 */
	physics_worker::physics_worker (physics_manager &man)
		: paused (false), ticks (0), man (man),
			rnd (utils::ns_since_epoch ()), _running (true),
		
			// and finally, the thread:
			th (std::bind (std::mem_fn (&hCraft::physics_worker::main_loop), this))
		{ }
	
	/* 
	 * Destructor - stops the worker thread.
	 */
	physics_worker::~physics_worker ()
	{
		this->_running = false;
		if (this->th.joinable ())
			this->th.join ();
	}
	
	
	physics_manager::~physics_manager ()
	{
		this->stop ();
	}
	
	void
	physics_manager::stop ()
	{
		this->workers.clear ();
		this->updates.clear ();
	}
	
	
	
	static bool
	handle_param_dissipate (world *w, physics_update& u, physics_action& act, std::minstd_rand& rnd)
	{
		std::uniform_int_distribution<> dis (0, 100);
		if (dis (rnd) <= act.val)
			{
				w->queue_update (u.data.blk.x, u.data.blk.y, u.data.blk.z, BT_AIR);
				return false;
			}
		
		return true;
	}
	
	static bool
	handle_param_drop (world *w, physics_update& u, physics_action& act)
	{
		int x = u.data.blk.x, y = u.data.blk.y, z = u.data.blk.z;
		
		int d = (act.val / (50 * ((u.tick <= 0) ? 1 : u.tick)));
		if (d <= 0) d = 1;
		
		if (u.elapsed % d == 0)
			{
				if ((y > 0) && (w->get_id (x, y - 1, z) == BT_AIR))
					{
						block_data bd = w->get_block (x, y, z);
						w->queue_update (x, y - 1, z, bd.id, bd.meta);
						w->queue_update (x, y, z, BT_AIR);
						
						-- u.data.blk.y;
						return true;
					}
				else
					return false;
			}
		
		return true;
	}
	
	// based on MCZall's finite mechanics.
	static bool
	handle_param_finite (world *w, physics_update& u, physics_action& act, std::minstd_rand& rnd)
	{
		int x = u.data.blk.x, y = u.data.blk.y, z = u.data.blk.z;
		
		block_data bd = w->get_block (x, y, z);
		if (bd.id == BT_AIR)
			return false;
		
		if (w->get_id (x, y - 1, z) == BT_AIR)
			{
				w->queue_update (x, y - 1, z, bd.id, bd.meta);
				w->queue_update (x, y, z, BT_AIR);
				
				-- u.data.blk.y;
			}
		else
			{
				int ind_list[25];
				for (int i = 0; i < 25; ++i)
					ind_list[i] = i;
				
				// shuffle the array
				for (int i = 24; i >= 0; --i)
					{
						std::uniform_int_distribution<> dis (0, i);
						int j = dis (rnd);
						int t = ind_list[j];
						ind_list[j] = ind_list[i];
						ind_list[i] = t;
					}
				
				struct xz_pos { int x; int z; };
				xz_pos pos_list[25];
				{
					int i = 0;
					for (int xx = (x - 2); xx <= (x + 2); ++xx)
						for (int zz = (z - 2); zz <= (z + 2); ++zz)
							{
								pos_list[i].x = xx;
								pos_list[i].z = zz;
								++ i;
							}
				}
				
				for (int ii = 0; ii < 25; ++ii)
					{
						int i = ind_list[ii];
						xz_pos pos = pos_list[i];
						if (w->get_id (pos.x, y - 1, pos.z) == BT_AIR &&
								w->get_id (pos.x, y, pos.z) == BT_AIR)
							{
								if (pos.x < x) pos.x = std::floor ((pos.x + x) / 2.0);
								else pos.x = std::ceil ((pos.x + x) / 2.0);
								if (pos.z < z) pos.z = std::floor ((pos.z + z) / 2.0);
								else pos.z = std::ceil ((pos.z + z) / 2.0);
								
								if (pos.x != x || pos.z != z)
									{
										if (w->get_id (pos.x, y, pos.z) == BT_AIR)
											{
												w->queue_update (pos.x, y, pos.z, bd.id, bd.meta);
												w->queue_update (x, y, z, BT_AIR);
										
												u.data.blk.x = pos.x;
												u.data.blk.z = pos.z;
												break;
											}
									}
							}
					}
			}
		
		return true;
	}
	
	static bool
	handle_params (world *w, physics_update& u, physics_manager &man, std::minstd_rand& rnd)
	{
		int found = 0;
		
		for (int i = 0; i < 8; ++i)
			{
				physics_action& act = u.params.actions[i];
				if (act.type == PA_NONE)
					continue;
				
				switch (act.type)
					{
					case PA_DISSIPATE:
						if (!handle_param_dissipate (w, u, act, rnd))
							act.type = PA_NONE;
						break;
					
					case PA_DROP:
						if (!handle_param_drop (w, u, act))
							act.type = PA_NONE;
						break;
					
					case PA_FINITE:
						if (!handle_param_finite (w, u, act, rnd))
							act.type = PA_NONE;
						break;
					
					default:
						act.type = PA_NONE;
						break;
					}
				
				if (act.expire == 0)
					act.type = PA_NONE;
				else if ((act.expire != -1) && (act.expire > 0))
					-- act.expire;
				
				if (act.type != PA_NONE)
					++ found;
			}
		
		if (found > 0)
			{
				physics_update nu = u;
				nu.nt = std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * nu.tick);
				++ nu.elapsed;
				man.updates.push (nu);
			}
		
		return true;
	}
	
	

	/* 
	 * Where everything happens.
	 */
	void
	physics_worker::main_loop ()
	{
		physics_update u {};
		const static int updates_per_tick = 8000;
		int i, fcount;
		
		std::minstd_rand rnd ((utils::ns_since_epoch ()));
		
		while (this->_running)
			{
				std::this_thread::sleep_for (std::chrono::milliseconds (50));
				++ this->ticks;
				if (paused)
					continue;
				
				fcount = 0; // failure counter
				for (i = 0; i < updates_per_tick; ++i)
					{
						if (!this->_running || paused || this->man.updates.empty ())
							break;
						if (!this->man.updates.try_pop (u))
							{
								++ fcount;
								if (fcount % 15 == 0)
									std::this_thread::sleep_for (std::chrono::milliseconds (2));
								if (fcount == 60)
									break;
								continue;
							}
						
						world *w = this->man.srv.world_by_id (u.wid);
						if (!w) continue;
						
						if (u.tick < 0) continue;
						if (u.nt > std::chrono::steady_clock::now ())
							{
								this->man.updates.push (u);
								continue;
							}
						
						// parameters
						if (!handle_params (w, u, this->man, this->rnd))
							continue;
						
						if (u.type == PU_BLOCK)
							{
								auto blk = u.data.blk;
								this->man.remove_block (w, blk.x, blk.y, blk.z);
								
								// does this block have a custom callback attached?
								if (blk.cb)
									{
										blk.cb (*w, blk.x, blk.y, blk.z, blk.data, rnd);
									}
								else
									{
										// nope, use the one associated with its ID
										physics_block *pb = w->get_physics_at (blk.x, blk.y, blk.z);
										if (pb)
											pb->tick (*w, blk.x, blk.y, blk.z, blk.data, nullptr, rnd);
									}
							}
						else if (u.type == PU_ENTITY)
							{
								auto ent = u.data.ent;
								entity *e = w->get_server ().entity_by_id (ent.eid);
								if (!e) continue;
								
								if (e->get_type () == ET_PLAYER)
									{
										player *pl = dynamic_cast<player *> (e);
										if (pl->get_world () != w)
											continue;
									}
								
								if (!e->tick (*w) && ent.persistent)
									{
										// requeue
										physics_update nu = u;
										nu.nt = std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * nu.tick);
										man.updates.push (nu);
									}
							}
					}
			}
	}
	
	
	
	bool
	physics_manager::block_exists_nolock (world *w, int x, int y, int z)
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
		return (sub->blocks[((y & 0xF) << 8) | ((z & 0xF) << 4) | (x & 0xF)] > 0);
	}
	
	void
	physics_manager::add_block_nolock (world *w, int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		
		std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>&
			mem_chunks = this->block_mem[w];
		ph_mem_chunk& ch = mem_chunks[{x >> 4, z >> 4}];
		ph_mem_subchunk* sub = ch.subs[y >> 4];
		if (sub == nullptr)
			sub = ch.subs[y >> 4] = new ph_mem_subchunk ();
		
		unsigned int index = ((y & 0xF) << 8) | ((z & 0xF) << 4) | (x & 0xF);
		if (sub->blocks[index] < 0xFFFF)
			++ sub->blocks[index];
		else
			std::cout << "!!!" << std::endl;
	}
	
		
	bool
	physics_manager::block_exists (world *w, int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		return this->block_exists_nolock (w, x, y, z);
	}
	
	void
	physics_manager::add_block (world *w, int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		this->add_block_nolock (w, x, y, z);
	}
	
	void
	physics_manager::remove_block (world *w, int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		std::lock_guard<std::mutex> guard {this->lock};
		
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
		
		unsigned int index = ((y & 0xF) << 8) | ((z & 0xF) << 4) | (x & 0xF);
		if (sub->blocks[index] > 0)
			-- sub->blocks[index];
	}
	
	
	
//-----------
	
	/* 
	 * Changes the number of worker threads to utilize.
	 */
	void
	physics_manager::set_thread_count (unsigned int count)
	{
		if (count > 20) count = 20;
		std::lock_guard<std::mutex> guard {this->lock};
		
		if (count == this->workers.size ())
			return; // nothing to do
		
		if (count > this->workers.size ())
			{
				while (this->workers.size () < count)
					this->workers.emplace_back (new physics_worker (*this));
				return;
			}
		
		if (count == 0)
			{
				this->workers.clear ();
				return;
			}
		
		this->workers.resize (count);
	}
	
	
	
	/* 
	 * Queues an update to be processed by one of the workers:
	 */
	void
	physics_manager::queue_physics (world *w, int x, int y, int z,
		int data, int tick_delay, physics_params *params,
		physics_block_callback cb)
	{
		//if (tick_delay == 0) tick_delay = 1;
		//-- tick_delay;
		
		std::lock_guard<std::mutex> guard {this->lock};
		this->add_block_nolock (w, x, y, z);
		
		physics_update u (w->id, x, y, z, data, tick_delay,
			std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * ((tick_delay < 0) ? 0 : tick_delay)),
			cb);
		if (params)
			for (int i = 0; i < 8; ++i)
				{
					u.params.actions[i] = params->actions[i];
					if (params->actions[i].type == PA_NONE)
						break;
				}
		
		this->updates.push (u);
	}
	
	/* 
	 * Queues an update only if one with the same xyz coordinates does not
	 * already exist.
	 */
	void
	physics_manager::queue_physics_once (world *w, int x, int y, int z,
		int data, int tick_delay, physics_params *params,
		physics_block_callback cb)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		if (this->block_exists_nolock (w, x, y, z))
			return;
		
		//if (tick_delay == 0) tick_delay = 1;
		//-- tick_delay;
		
		this->add_block_nolock (w, x, y, z);
		physics_update u (w->id, x, y, z, data, tick_delay,
			std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * ((tick_delay < 0) ? 0 : tick_delay)),
			cb);
		if (params)
			for (int i = 0; i < 8; ++i)
				{
					u.params.actions[i] = params->actions[i];
					if (params->actions[i].type == PA_NONE)
						break;
				}
		
		this->updates.push (u);
	}
	
	
	/* 
	 * Queues an entity update.
	 */
	void
	physics_manager::queue_physics (world *w, int eid, bool persistent,
		int tick_delay, physics_params *params)
	{
		if (tick_delay == 0) tick_delay = 1;
		-- tick_delay;
		
		std::lock_guard<std::mutex> guard {this->lock};
		
		physics_update u (w->id, eid, persistent, tick_delay,
			std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * ((tick_delay < 0) ? 0 : tick_delay)));
		if (params)
			for (int i = 0; i < 8; ++i)
				{
					u.params.actions[i] = params->actions[i];
					if (params->actions[i].type == PA_NONE)
						break;
				}
		
		this->updates.push (u);
	}
}

