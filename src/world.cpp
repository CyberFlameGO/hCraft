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

#include "world.hpp"
#include "server.hpp"
#include "playerlist.hpp"
#include "player.hpp"
#include "packet.hpp"
#include "logger.hpp"
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <cctype>
#include <algorithm>

#include <iostream> // DEBUG


namespace hCraft {
	
	static unsigned long long
	chunk_key (int x, int z)
		{ return ((unsigned long long)((unsigned int)z) << 32)
			| (unsigned long long)((unsigned int)x); }
	
	static void
	chunk_coords (unsigned long long key, int* x, int* z)
		{ *x = key & 0xFFFFFFFFU; *z = key >> 32; }
	
	
	
	/* 
	 * Constructs a new empty world.
	 */
	world::world (server &srv, const char *name, logger &log, world_generator *gen,
		world_provider *provider)
		: srv (srv), log (log), lm (log, this), estage (this)
	{
		assert (world::is_valid_name (name));
		std::strcpy (this->name, name);
		
		this->gen = gen;
		this->width = 0;
		this->depth = 0;
		
		this->prov = provider;
		this->edge_chunk = nullptr;
		this->last_chunk = {0, 0, nullptr};
		
		this->players = new playerlist ();
		this->th_running = false;
		this->auto_lighting = true;
		this->ticks = 0;
		
		this->ph_state = PHY_ON;
		//this->physics.set_thread_count (0);
	}
	
	/* 
	 * Class destructor.
	 */
	world::~world ()
	{
		this->stop ();
		delete this->players;
		
		delete this->gen;
		if (this->edge_chunk)
			delete this->edge_chunk;
		
		if (this->prov)
			delete this->prov;
		
		{
			std::lock_guard<std::mutex> guard {this->chunk_lock};
			for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
				{
					chunk *ch = itr->second;
					delete ch;
				}
			this->chunks.clear ();
		}
	}
	
	
	
	/* 
	 * Checks whether the specified string can be used to name a world.
	 */
	bool
	world::is_valid_name (const char *wname)
	{
		const char *ptr = wname;
		int len = 0, c;
		while ((c = (int)(*ptr++)))
			{
				if (len++ == 32)
					return false;
				
				if (!(std::isalnum (c) || (c == '_' || c == '-' || c == '.')))
					return false;
			}
		
		return true;
	}
	
	
	
	/* 
	 * Starts the world's "physics"-handling thread.
	 */
	void
	world::start ()
	{
		if (this->th_running)
			return;
		
		this->th_running = true;
		this->th.reset (new std::thread (
			std::bind (std::mem_fn (&hCraft::world::worker), this)));
	}
	
	/* 
	 * Stops the world's thread.
	 */
	void
	world::stop ()
	{
		if (!this->th_running)
			return;
		
		this->stop_physics ();
		
		this->th_running = false;
		if (this->th->joinable ())
			this->th->join ();
		this->th.reset ();
	}
	
	
	
	/* 
	 * The function ran by the world's thread.
	 */
	void
	world::worker ()
	{
		const static int block_update_cap = 10000; // per tick
		const static int light_update_cap = 10000; // per tick
		
		int update_count;
		dense_edit_stage pl_tr;
		
		this->ticks = 0;
		while (this->th_running)
			{
				++ this->ticks;
				{
					std::lock_guard<std::mutex> guard {this->update_lock};
					
					/* 
					 * Block updates.
					 */
					if (!this->updates.empty ())
						{
							std::lock_guard<std::mutex> lm_guard {this->lm.get_lock ()};
							
							std::vector<player *> pl_vc;
							this->get_players ().populate (pl_vc);
							std::cout << "[" << pl_vc.size () << "p]" << std::endl;
							
							update_count = 0;
							while (!this->updates.empty () && (update_count++ < block_update_cap))
								{
									block_update &u = this->updates.front ();
									
									block_data old_bd = this->get_block (u.x, u.y, u.z);
									if (old_bd.id == u.id && old_bd.meta == u.meta)
										{
											// nothing modified
											this->updates.pop_front ();
											continue;
										}
									
									block_info *old_inf = block_info::from_id (old_bd.id);
									block_info *new_inf = block_info::from_id (u.id);
							
									physics_block *ph = physics_block::from_id (u.id);
									
									if (((this->width > 0) && ((u.x >= this->width) || (u.x < 0))) ||
										((this->depth > 0) && ((u.z >= this->depth) || (u.z < 0))) ||
										((u.y < 0) || (u.y > 255)))
										{
											this->updates.pop_front ();
											continue;
										}
									
									if ((this->get_id (u.x, u.y, u.z) == u.id) &&
											(this->get_meta (u.x, u.y, u.z) == u.meta))
										{
											this->updates.pop_front ();
											continue;
										}
									
									unsigned short old_id = this->get_id (u.x, u.y, u.z);
									unsigned char old_meta = this->get_meta (u.x, u.y, u.z);
									this->set_block (u.x, u.y, u.z, u.id, u.meta);
							
									chunk *ch = this->get_chunk_at (u.x, u.z);
									if (new_inf->opaque != old_inf->opaque)
										ch->recalc_heightmap (u.x & 0xF, u.z & 0xF);
									
									// update players
									pl_tr.set (u.x, u.y, u.z, ph ? ph->vanilla_id () : u.id, u.meta);
									
									if (ch)
										{
											if (auto_lighting)
												{
													this->lm.enqueue_nolock (u.x, u.y, u.z);
												}
											
											// physics
											if (u.physics && ph)
												{
													if (old_id != u.id || old_meta != u.meta)
														{
															physics_block *old_ph = physics_block::from_id (old_id);
															if (old_ph)
																old_ph->on_modified (*this, u.x, u.y, u.z);
														}
													
													this->queue_physics (u.x, u.y, u.z, u.extra, u.ptr,
														ph->tick_rate ());
												}
											
											// check neighbouring blocks
											{
												physics_block *nph;
											
												int xx, yy, zz;
												for (xx = (u.x - 1); xx <= (u.x + 1); ++xx)
													for (yy = (u.y - 1); yy <= (u.y + 1); ++yy)
														for (zz = (u.z - 1); zz <= (u.z + 1); ++zz)
															{
																if (xx == u.x && yy == u.y && zz == u.z)
																	continue;
																if ((yy < 0) || (yy > 255))
																	continue;
															
																nph = this->get_physics_at (xx, yy, zz);
																if (nph && nph->affected_by_neighbours ())
																	{
																		nph->on_neighbour_modified (*this, xx, yy, zz,
																			u.x, u.y, u.z);
																	}
															}
											}
										}
									
									this->updates.pop_front ();
								}
							
							// send updates to players
							pl_tr.preview (pl_vc);
							pl_tr.clear ();
						}
					
				} // release of update lock
				
				/* 
				 * Lighting updates.
				 */
				this->lm.update (light_update_cap);
				
				std::this_thread::sleep_for (std::chrono::milliseconds (5));
			}
	}
	
	
	
	void
	world::set_width (int width)
	{
		if (width % 16 != 0)
			width += width % 16;
		this->width = width;
		
		if (this->width > 0 && !this->edge_chunk)
			{
				this->edge_chunk = new chunk ();
				this->gen->generate_edge (*this, this->edge_chunk);
				this->edge_chunk->generated = true;
				this->edge_chunk->recalc_heightmap ();
				this->lm.relight_chunk (this->edge_chunk);
			}
	}
	
	void
	world::set_depth (int depth)
	{
		if (depth % 16 != 0)
			depth += depth % 16;
		this->depth = depth;
		
		if (this->depth > 0 && !this->edge_chunk)
			{
				this->edge_chunk = new chunk ();
				this->gen->generate_edge (*this, this->edge_chunk);
				this->edge_chunk->generated = true;
				this->edge_chunk->recalc_heightmap ();
				this->lm.relight_chunk (this->edge_chunk);
			}
	}
	
	
	
	void
	world::get_information (world_information& inf)
	{
		inf.width = this->width;
		inf.depth = this->depth;
		inf.spawn_pos = this->spawn_pos;
		inf.generator = this->gen ? this->gen->name () : "";
		inf.seed = this->gen ? this->gen->seed () : 0;
		inf.chunk_count = 0;
	}
	
	/* 
	 * Saves all modified chunks to disk.
	 */
	void
	world::save_all ()
	{
		if (this->prov == nullptr)
			return;
		
		std::lock_guard<std::mutex> guard {this->chunk_lock};
		
		if (this->chunks.empty ())
			{
				this->prov->save_empty (*this);
				return;
			}
		
		this->prov->open (*this);
		
		// meta
		world_information inf = this->prov->info ();
		this->get_information (inf);
		this->prov->save_info (*this, inf);
		
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			{
				chunk *ch = itr->second;
				if (ch->modified)
					{
						int x, z;
						chunk_coords (itr->first, &x, &z);
						this->prov->save (*this, ch, x, z);
						ch->modified = false;
					}
			}
		this->prov->close ();
	}
	
	/* 
	 * Saves metadata to disk (width, depth, spawn pos, etc...).
	 */
	void
	world::save_meta ()
	{
		if (this->prov == nullptr)
			return;
		
		// we're not modifying any chunks, but we'll still take ahold of this lock...
		std::lock_guard<std::mutex> guard {this->chunk_lock};
		
		this->prov->open (*this);
		
		world_information inf = this->prov->info ();
		this->get_information (inf);
		this->prov->save_info (*this, inf);
		
		this->prov->close ();
	}
	
	
	
	/* 
	 * Loads up a grid of diameter x diameter chunks around the given point
	 * (specified in chunk coordinates).
	 */
	void
	world::load_grid (chunk_pos cpos, int diameter)
	{
		int r_half = diameter >> 1;
		int cx, cz;
		
		for (cx = (cpos.x - r_half); cx <= (cpos.x + r_half); ++cx)
			for (cz = (cpos.z - r_half); cz <= (cpos.z + r_half); ++cz)
				{
					this->load_chunk (cx, cz);
				}
	}
	
	/* 
	 * Calls load_grid around () {x: 0, z: 0}, and attempts to find a suitable
	 * spawn position. 
	 */
	void
	world::prepare_spawn (int diameter, bool calc_spawn_point)
	{
		this->load_grid (chunk_pos (this->spawn_pos), diameter);
		
		if (calc_spawn_point)
			{
				block_pos best {0, 0, 0};
		
				int cx, cz, x, z;
				short h;
		
				for (cx = 0; cx <= 2; ++cx)
					for (cz = 0; cz <= 2; ++cz)
						{
							chunk *ch = this->load_chunk (cx, cz);
							for (x = 0; x < 16; ++x)
								for (z = 0; z < 16; ++z)
									{
										h = ch->get_height (x, z);
										if (ch->get_id (x, h - 1, z) != 0 && ((h + 1) > best.y))
											best.set ((cx * 16) + x, h + 1, (cz * 16) + z);
									}
						}
		
				this->spawn_pos = best;
			}
	}
	
	
	
	/* 
	 * Inserts the specified chunk into this world at the given coordinates.
	 */
	void
	world::put_chunk (int x, int z, chunk *ch)
	{
		unsigned long long key = chunk_key (x, z);
		
		std::lock_guard<std::mutex> guard {this->chunk_lock};
		auto itr = this->chunks.find (key);
		if (itr != this->chunks.end ())
			{
				chunk *prev = itr->second;
				if (prev == ch) return;
				delete prev;
				this->chunks.erase (itr);
			}
		
		// set links
		{
			// north (-z)
			chunk *n = chunk_in_bounds (x, z - 1) ? get_chunk_nolock (x, z - 1) : nullptr;
			if (n)
				{
					ch->north = n;
					n->south = ch;
					
				}
			else
				ch->north = nullptr;
			
			// south (+z)
			chunk *s = chunk_in_bounds (x, z + 1) ? get_chunk_nolock (x, z + 1) : nullptr;
			if (s)
				{
					ch->south = s;
					s->north = ch;
				}
			else
				ch->south = nullptr;
			
			// west (-x)
			chunk *w = chunk_in_bounds (x - 1, z) ? get_chunk_nolock (x - 1, z) : nullptr;
			if (w)
				{
					ch->west = w;
					w->east = ch;
				}
			else
				ch->west = nullptr;
			
			// east (+x)
			chunk *e = chunk_in_bounds (x + 1, z) ? get_chunk_nolock (x + 1, z) : nullptr;
			if (e)
				{
					ch->east = e;
					e->west = ch;
				}
			else
				ch->east = nullptr;
		}
		
		this->chunks[key] = ch;
	}
	
	
	chunk*
	world::get_chunk_nolock (int x, int z)
	{
		if (!this->chunk_in_bounds (x, z))
			return this->edge_chunk;
		
		auto itr = this->chunks.find ((unsigned long long)chunk_key (x, z));
		if (itr != this->chunks.end ())
			return itr->second;
		
		return nullptr;
	}
	
	/* 
	 * Searches the chunk world for a chunk located at the specified coordinates.
	 */
	chunk*
	world::get_chunk (int x, int z)
	{
		if (!this->chunk_in_bounds (x, z))
			return this->edge_chunk;
		
		unsigned long long key = chunk_key (x, z);
		
		std::lock_guard<std::mutex> guard {this->chunk_lock};
		auto itr = this->chunks.find (key);
		if (itr != this->chunks.end ())
			return itr->second;
		
		return nullptr;
	}
	
	/* 
	 * Returns the chunk located at the given block coordinates.
	 */
	chunk*
	world::get_chunk_at (int bx, int bz)
	{
		return this->get_chunk (bx >> 4, bz >> 4);
	}
	
	chunk*
	world::load_chunk_at (int bx, int bz)
	{
		return this->load_chunk (bx >> 4, bz >> 4);
	}
	
	
	/* 
	 * Same as get_chunk (), but if the chunk does not exist, it will be either
	 * loaded from a file (if such a file exists), or completely generated from
	 * scratch.
	 */
	chunk*
	world::load_chunk (int x, int z)
	{
		chunk *ch = this->get_chunk (x, z);
		if (ch && ch->generated) return ch;
		else if (!ch)
			{
				ch = new chunk ();
		
				// try to load from disk
				this->prov->open (*this);
				if (this->prov->load (*this, ch, x, z))
					{
						if (ch->generated)
							{
								ch->recalc_heightmap ();
								this->prov->close ();
								this->put_chunk (x, z, ch);
								return ch;
							}
					}
				
				this->prov->close ();
				this->put_chunk (x, z, ch);
			}
		
		this->gen->generate (*this, ch, x, z);
		ch->generated = true;
		ch->recalc_heightmap ();
		this->lm.relight_chunk (ch);
		return ch;
	}
	
	
	/* 
	 * Checks whether a block exists at the given coordinates.
	 */
	
	bool
	world::in_bounds (int x, int y, int z)
	{
		return (((this->width > 0) ? ((x < this->width) && (x >= 0)) : true) 	// x
				&& ((this->depth > 0) ? ((z < this->depth) && (z >= 0)) : true)  	// z
				&& ((y >= 0) && (y <= 255)));     																// y 
	}
	
	bool
	world::chunk_in_bounds (int cx, int cz)
	{
		int cw = this->width >> 4;
		int cd = this->depth >> 4;
		return (((cw > 0) ? ((cx < cw) && (cx >= 0)) : true)	// x
				&& ((cd > 0) ? ((cz < cd) && (cz >= 0)) : true));	// z
	}
	
	
	
	/* 
	 * Spawns the specified entity into the world.
	 */
	void
	world::spawn_entity (entity *e)
	{
		// add to entity list
		{
			std::lock_guard<std::mutex> lock ((this->entity_lock));
			auto itr = this->entities.find (e);
			if (itr != this->entities.end ())
				return; // id clash
			
			this->entities.insert (e);
		}
		
		chunk *ch = this->load_chunk_at ((int)e->pos.x, (int)e->pos.z);
		if (!ch) return; // shouldn't happen
		
		ch->add_entity (e);
		e->spawn_time = std::chrono::steady_clock::now ();
		
		// physics
		this->srv.global_physics.queue_physics (this, e);
		
		// spawn entity to players
		chunk_pos cpos = e->pos;
		int cx, cz;
		for (cx = (cpos.x - player::chunk_radius ());
				 cx <= (cpos.x + player::chunk_radius ()); ++cx)
			{
				for (cz = (cpos.z - player::chunk_radius ());
						 cz <= (cpos.z + player::chunk_radius ()); ++cz)
					{
						chunk *och = this->get_chunk (cx, cz);
						if (!och) continue;
						
						entity *e_this = e;
						och->all_entities ([e_this] (entity *e)
							{
								if (e->get_type () == ET_PLAYER)
									{
										player *pl = dynamic_cast<player *> (e);
										e_this->spawn_to (pl);
									}
							});
					}
			}
	}
	
	
	std::unordered_set<entity *>::iterator
	world::despawn_entity_nolock (std::unordered_set<entity *>::iterator itr)
	{
		entity *e = *itr;
		
		chunk *ch = this->get_chunk_at ((int)e->pos.x, (int)e->pos.z);
		if (ch)
			{
				ch->remove_entity (e);
			}
		
		// despawn from players
		chunk_pos cpos = e->pos;
		int cx, cz;
		for (cx = (cpos.x - player::chunk_radius ());
				 cx <= (cpos.x + player::chunk_radius ()); ++cx)
			{
				for (cz = (cpos.z - player::chunk_radius ());
						 cz <= (cpos.z + player::chunk_radius ()); ++cz)
					{
						chunk *och = this->get_chunk (cx, cz);
						if (!och) continue;
						
						entity *e_this = e;
						och->all_entities ([e_this] (entity *e)
							{
								if (e->get_type () == ET_PLAYER)
									{
										player *pl = dynamic_cast<player *> (e);
										e_this->despawn_from (pl);
									}
							});
					}
			}
		delete e;
		
		auto ret_itr = this->entities.erase (itr);
		return ret_itr;
	}
	
	/* 
	 * Removes the specified etnity from this world.
	 */
	void
	world::despawn_entity (entity *e)
	{
		std::lock_guard<std::mutex> lock ((this->entity_lock));
		auto itr = this->entities.find (e);
		if (itr == this->entities.end ())
			return;
		this->despawn_entity_nolock (itr);
	}
	
	/* 
	 * Calls the given function on all entities in the world.
	 */
	void
	world::all_entities (std::function<void (entity *e)> f)
	{
		std::lock_guard<std::mutex> lock ((this->entity_lock));
		for (auto itr = this->entities.begin (); itr != this->entities.end (); ++itr)
			f (*itr);
	}
		
	
	
//----
	/* 
	 * Block interaction: 
	 */
	
	void
	world::set_id (int x, int y, int z, unsigned short id)
	{
		chunk *ch = nullptr;
		if (this->last_chunk.ch)
			{
				int dx = this->last_chunk.x - (x >> 4);
				int dz = this->last_chunk.z - (z >> 4);
				if (dx == 0)
					{
						switch (dz)
							{
								case  0: ch = this->last_chunk.ch; break;
								case  1: ch = ch->south; break;
								case -1: ch = ch->north; break;
							}
					}
				else if (dz == 0)
					{
						switch (dx)
							{
								case  0: ch = this->last_chunk.ch; break;
								case  1: ch = ch->east; break;
								case -1: ch = ch->west; break;
							}
					}
				
				if (!ch)
					ch = this->load_chunk (x >> 4, z >> 4);
			}
		else
			ch = this->load_chunk (x >> 4, z >> 4);
		ch->set_id (x & 0xF, y, z & 0xF, id);
	}
	
	unsigned short
	world::get_id (int x, int y, int z)
	{
		chunk *ch = this->get_chunk (x >> 4, z >> 4);
		if (!ch)
			return 0;
		return ch->get_id (x & 0xF, y, z & 0xF);
	}
	
	
	void
	world::set_meta (int x, int y, int z, unsigned char val)
	{
		chunk *ch = this->load_chunk (x >> 4, z >> 4);
		ch->set_meta (x & 0xF, y, z & 0xF, val);
	}
	
	unsigned char
	world::get_meta (int x, int y, int z)
	{
		chunk *ch = this->get_chunk (x >> 4, z >> 4);
		if (!ch)
			return 0;
		return ch->get_meta (x & 0xF, y, z & 0xF);
	}
	
	
	void
	world::set_block_light (int x, int y, int z, unsigned char val)
	{
		chunk *ch = this->load_chunk (x >> 4, z >> 4);
		ch->set_block_light (x & 0xF, y, z & 0xF, val);
	}
	
	unsigned char
	world::get_block_light (int x, int y, int z)
	{
		chunk *ch = this->get_chunk (x >> 4, z >> 4);
		if (!ch)
			return 0;
		return ch->get_block_light (x & 0xF, y, z & 0xF);
	}
	
	 
	void
	world::set_sky_light (int x, int y, int z, unsigned char val)
	{
		chunk *ch = this->load_chunk (x >> 4, z >> 4);
		ch->set_sky_light (x & 0xF, y, z & 0xF, val);
	}
	
	unsigned char
	world::get_sky_light (int x, int y, int z)
	{
		chunk *ch = this->get_chunk (x >> 4, z >> 4);
		if (!ch)
			return 0xF;
		return ch->get_sky_light (x & 0xF, y, z & 0xF);
	}
	
	
	void
	world::set_block (int x, int y, int z, unsigned short id, unsigned char meta, unsigned char ex)
	{
		chunk *ch = this->load_chunk (x >> 4, z >> 4);
		ch->set_block (x & 0xF, y, z & 0xF, id, meta, ex);
	}
	
	
	void
	world::set_extra (int x, int y, int z, unsigned char ex)
	{
		chunk *ch = this->load_chunk (x >> 4, z >> 4);
		ch->set_extra (x & 0xF, y, z & 0xF, ex);
	}
	
	unsigned char
	world::get_extra (int x, int y, int z)
	{
		chunk *ch = this->get_chunk (x >> 4, z >> 4);
		if (!ch)
			return 0;
		return ch->get_extra (x & 0xF, y, z & 0xF);
	}
	
	
	block_data
	world::get_block (int x, int y, int z)
	{
		chunk *ch = this->get_chunk (x >> 4, z >> 4);
		if (!ch)
			return block_data ();
		return ch->get_block (x & 0xF, y, z & 0xF);
	}
	
	
	bool
	world::has_physics_at (int x, int y, int z)
	{
		return (this->get_physics_at (x, y, z) != nullptr);
	}
	
	physics_block*
	world::get_physics_at (int x, int y, int z)
	{
		return physics_block::from_id (this->get_id (x, y, z));
	}
	
	
	
	/* 
	 * Instead of fetching the block from the underlying chunk, and attempt
	 * to query the edit stage is made first.
	 */
	blocki
	world::get_final_block (int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->estage_lock};
		return this->estage.get (x, y, z);
	}
	
	
	
//----
	
	/* 
	 * Enqueues an update that should be made to a block in this world
	 * and sent to nearby players.
	 */
	void
	world::queue_update (int x, int y, int z, unsigned short id,
		unsigned char meta, int extra, void *ptr, player *pl, bool physics)
	{
		if (!this->in_bounds (x, y, z)) return;
		std::lock_guard<std::mutex> guard {this->update_lock};
		this->updates.emplace_back (x, y, z, id, meta, extra, ptr, pl, physics);
		
		std::lock_guard<std::mutex> estage_guard {this->estage_lock};
		this->estage.set (x, y, z, id, meta);
	}
	
	void
	world::queue_physics (int x, int y, int z, int extra, void *ptr,
		int tick_delay, physics_params *params, physics_block_callback cb)
	{
		if (!this->in_bounds (x, y, z)) return;
		if (this->ph_state == PHY_OFF) return;
		
		if (this->physics.get_thread_count () == 0)
			this->srv.global_physics.queue_physics (this, x, y, z, extra, tick_delay, params, cb);
		else
			this->physics.queue_physics (this, x, y, z, extra, tick_delay, params, cb);
	}
	
	void
	world::queue_physics_once (int x, int y, int z, int extra, void *ptr,
		int tick_delay, physics_params *params, physics_block_callback cb)
	{
		if (!this->in_bounds (x, y, z)) return;
		if (this->ph_state == PHY_OFF) return;
		
		if (this->physics.get_thread_count () == 0)
			this->srv.global_physics.queue_physics_once (this, x, y, z, extra, tick_delay, params, cb);
		else
			this->physics.queue_physics_once (this, x, y, z, extra, tick_delay, params, cb);
	}
	
	
	void
	world::start_physics ()
	{
		this->ph_state = PHY_ON;
	}
	
	void
	world::stop_physics ()
	{
		if (this->ph_state == PHY_OFF) return;
		this->ph_state = PHY_OFF;
		
		// TODO
	}
	
	void
	world::pause_physics ()
	{
		if (this->ph_state == PHY_PAUSED) return;
		this->ph_state = PHY_PAUSED;
	}
}

