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
#include "playerlist.hpp"
#include "player.hpp"
#include "packet.hpp"
#include "logger.hpp"
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <cctype>
#include <algorithm>

// physics:
#include "physics/sand.hpp"


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
	world::world (const char *name, logger &log, world_generator *gen,
		world_provider *provider)
		: log (log), lm (log, this)
	{
		assert (world::is_valid_name (name));
		std::strcpy (this->name, name);
		
		this->gen = gen;
		this->width = 0;
		this->depth = 0;
		
		this->prov = provider;
		this->edge_chunk = nullptr;
		
		this->players = new playerlist ();
		this->th_running = false;
		this->auto_lighting = true;
		
		// physics blocks
		{
#define REGISTER_BLOCK(B)  \
	{ B *a = new B ();        \
		this->phblocks[a->id ()].reset (a); }
			REGISTER_BLOCK (physics::sand)
		}
		this->ph_on = true;
		this->ph_paused = false;
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
		while (c = *ptr++)
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
		const static int ph_update_cap    = 200;
		
		int update_count;
		
		bool nolight_msg = false;
		int total_update_count = 0;
		
		while (this->th_running)
			{
				{
					std::lock_guard<std::mutex> guard {this->update_lock};
					
					/* 
					 * Block updates.
					 */
					update_count = 0;
					while (!this->updates.empty () && (update_count++ < block_update_cap))
						{
							block_update &update = this->updates.front ();
							
							block_data old_bd = this->get_block (update.x, update.y, update.z);
							if (old_bd.id == update.id && old_bd.meta == update.meta)
								{
									// nothing modified
									this->updates.pop_front ();
									continue;
								}
							
							block_info *old_inf = block_info::from_id (old_bd.id);
							block_info *new_inf = block_info::from_id (update.id);
							
							physics_block *ph = nullptr; {
								auto itr = this->phblocks.find (update.id);
								if (itr != this->phblocks.end ())
									ph = itr->second.get ();
							}
							
							if (((this->width > 0) && ((update.x >= this->width) || (update.x < 0))) ||
								((this->depth > 0) && ((update.z >= this->depth) || (update.z < 0))) ||
								((update.y < 0) || (update.y > 255)))
								{
									this->updates.pop_front ();
									continue;
								}
							
							if ((this->get_id (update.x, update.y, update.z) == update.id) &&
									(this->get_meta (update.x, update.y, update.z) == update.meta))
								{
									this->updates.pop_front ();
									continue;
								}
							
							chunk *ch = this->get_chunk_at (update.x, update.z);
							this->set_id_and_meta (update.x, update.y, update.z,
								update.id, update.meta);
							if (new_inf->opaque != old_inf->opaque)
								ch->recalc_heightmap (update.x & 0xF, update.z & 0xF);
							
							this->get_players ().send_to_all (packet::make_block_change (
								update.x, update.y, update.z, ph ? ph->vanilla_id () : update.id,
								update.meta), update.pl);
							
							if (ch)
								{
									if (auto_lighting)
										{
											this->lm.enqueue (update.x, update.y, update.z);
										}
									
									// physics
									if (ph)
										this->queue_physics_nolock (update.x, update.y, update.z);
									
									// check neighbouring blocks
									{
										physics_block *nph;
										
										int xx, yy, zz;
										for (xx = (update.x - 1); xx <= (update.x + 1); ++xx)
											for (yy = (update.y - 1); yy <= (update.y + 1); ++yy)
												for (zz = (update.z - 1); zz <= (update.z + 1); ++zz)
													{
														if ((yy < 0) || (yy > 255))
															continue;
														
														nph = this->get_physics_at (xx, yy, zz);
														if (nph)
															nph->on_neighbour_modified (*this, xx, yy, zz,
																update.x, update.y, update.z);
													}
									}
								}
							
							this->updates.pop_front ();
						}
					
					/* 
					 * Lighting updates.
					 */
					int handled = this->lm.update (light_update_cap);
					
					/* 
					 * Physics.
					 */
					if (this->ph_on && !this->ph_paused)
						{
							update_count = 0;
							while (!phupdates.empty () && (update_count++ < ph_update_cap))
								{
									physics_update u = this->phupdates.front ();
									this->phupdates.pop_front ();
							
									unsigned short id = this->get_id (u.x, u.y, u.z);
									physics_block *ph; {
										auto itr = this->phblocks.find (id);
										if (itr == this->phblocks.end ())
											continue;
										ph = itr->second.get ();
									}
							
									if ((u.last_tick + std::chrono::milliseconds (ph->tick_rate ()))
										> std::chrono::system_clock::now ())
										{
											this->phupdates.push_back (u);
											continue;
										}
							
									ph->tick (*this, u.x, u.y, u.z, u.extra);						
								}
						}
				}
				
				std::this_thread::sleep_for (std::chrono::milliseconds (1));
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
				this->edge_chunk->recalc_heightmap ();
				this->edge_chunk->relight ();
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
				this->edge_chunk->recalc_heightmap ();
				this->edge_chunk->relight ();
			}
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
	 * Loads up a grid of radius x radius chunks around the given point
	 * (specified in chunk coordinates).
	 */
	void
	world::load_grid (chunk_pos cpos, int radius)
	{
		int r_half = radius >> 1;
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
	world::prepare_spawn (int radius)
	{
		this->load_grid (chunk_pos (0, 0), radius);
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
			chunk *n = get_chunk_nolock (x, z - 1);
			if (n)
				{
					ch->north = n;
					n->south = ch;
				}
			else
				ch->north = nullptr;
			
			// south (+z)
			chunk *s = get_chunk_nolock (x, z + 1);
			if (s)
				{
					ch->south = s;
					s->north = ch;
				}
			else
				ch->south = nullptr;
			
			// west (-x)
			chunk *w = get_chunk_nolock (x - 1, z);
			if (w)
				{
					ch->west = w;
					w->east = ch;
				}
			else
				ch->west = nullptr;
			
			// east (+x)
			chunk *e = get_chunk_nolock (x + 1, z);
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
		if (((this->width > 0) && (((x * 16) >= this->width) || (x < 0))) ||
				((this->depth > 0) && (((z * 16) >= this->depth) || (z < 0))))
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
		if (((this->width > 0) && (((x * 16) >= this->width) || (x < 0))) ||
				((this->depth > 0) && (((z * 16) >= this->depth) || (z < 0))))
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
	
	/* 
	 * Same as get_chunk (), but if the chunk does not exist, it will be either
	 * loaded from a file (if such a file exists), or completely generated from
	 * scratch.
	 */
	chunk*
	world::load_chunk (int x, int z)
	{
		chunk *ch = this->get_chunk (x, z);
		if (ch) return ch;
		
		ch = new chunk ();
		
		// try to load from disk
		this->prov->open (*this);
		if (this->prov->load (*this, ch, x, z))
			{
				this->prov->close ();
				this->put_chunk (x, z, ch);
				ch->recalc_heightmap ();
				ch->relight ();
				return ch;
			}
		
		this->prov->close ();
		this->put_chunk (x, z, ch);
		this->gen->generate (*this, ch, x, z);
		ch->recalc_heightmap ();
		ch->relight ();
		return ch;
	}
	
	
	
//----
	/* 
	 * Block interaction: 
	 */
	
	void
	world::set_id (int x, int y, int z, unsigned short id)
	{
		chunk *ch = this->load_chunk (x >> 4, z >> 4);
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
	world::set_id_and_meta (int x, int y, int z, unsigned short id, unsigned char meta)
	{
		chunk *ch = this->load_chunk (x >> 4, z >> 4);
		ch->set_id_and_meta (x & 0xF, y, z & 0xF, id, meta);
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
		unsigned short id = this->get_id (x, y, z);
		return (this->phblocks.find (id) != this->phblocks.end ());
	}
	
	physics_block*
	world::get_physics_at (int x, int y, int z)
	{
		unsigned short id = this->get_id (x, y, z);
		auto itr = this->phblocks.find (id);
		return ((itr == this->phblocks.end ())
			? nullptr
			: itr->second.get ());
	}
	
	unsigned short
	world::get_final_id_nolock (int x, int y, int z)
	{
		unsigned short id = this->get_id (x, y, z);
		for (auto itr = std::begin (this->updates); itr != std::end (this->updates); ++itr)
			{
				block_update &u = *itr;
				if (u.x == x && u.y == y && u.z == z)
					{
						id = u.id;
						break;
					}
			}
		
		return id;
	}
	
	
	
//----
	
	/* 
	 * Enqueues an update that should be made to a block in this world
	 * and sent to nearby players.
	 */
	void
	world::queue_update (int x, int y, int z, unsigned short id,
		unsigned char meta, player *pl)
	{
		std::lock_guard<std::mutex> guard {this->update_lock};
		this->updates.emplace_back (x, y, z, id, meta, 0, pl);
	}
	
	void
	world::queue_update_nolock (int x, int y, int z, unsigned short id,
		unsigned char meta, int extra, player *pl)
	{
		this->updates.emplace_back (x, y, z, id, meta, extra, pl);
	}
	
	void
	world::queue_physics (int x, int y, int z, int extra)
	{
		if (!this->ph_on) return;
		
		std::lock_guard<std::mutex> guard {this->update_lock};
		this->phupdates.emplace_back (x, y, z, extra, std::chrono::system_clock::now ());
	}
	
	void
	world::queue_physics_nolock (int x, int y, int z, int extra)
	{
		if (!this->ph_on) return;
		this->phupdates.emplace_back (x, y, z, extra, std::chrono::system_clock::now ());
	}
	
	void
	world::queue_physics_once_nolock (int x, int y, int z, int extra)
	{
		if (!this->ph_on) return;
		if (std::find_if (std::begin (this->phupdates), std::end (this->phupdates),
			[x, y, z] (const physics_update& u) -> bool
				{
					return (u.x == x && u.y == y && u.z == z);
				}) != std::end (this->phupdates))
			return;
		this->phupdates.emplace_back (x, y, z, extra, std::chrono::system_clock::now ());
	}
	
	
	void
	world::start_physics ()
	{
		//if (this->ph_on) return;
		this->ph_on = true;
		this->ph_paused = false;
	}
	
	void
	world::stop_physics ()
	{
		if (!this->ph_on) return;
		this->ph_on = false;
		this->ph_paused = false;
		
		std::lock_guard<std::mutex> guard {this->update_lock};
		this->phupdates.clear ();
	}
	
	void
	world::pause_physics ()
	{
		if (!this->ph_on || this->ph_paused) return;
		this->ph_paused = true;
	}
}

