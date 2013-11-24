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

#include "world/world.hpp"
#include "system/server.hpp"
#include "player/player_list.hpp"
#include "player/player.hpp"
#include "system/packet.hpp"
#include "system/logger.hpp"
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
	
	
	
	static void
	_init_sql_tables (world *w, server &srv)
	{
		soci::session sql (srv.sql_pool ());
		
		// block history table
		sql.once <<
			"CREATE TABLE IF NOT EXISTS `block_history_" << w->get_name () << "` ("
			"`x` INT, `y` INT, `z` INT, " // pos
			"`old_id` SMALLINT UNSIGNED, `old_meta` TINYINT UNSIGNED, `old_ex` TINYINT UNSIGNED, " // old value
			"`new_id` SMALLINT UNSIGNED, `new_meta` TINYINT UNSIGNED, `new_ex` TINYINT UNSIGNED, " // new value
			"`pid` INTEGER UNSIGNED, `time` BIGINT UNSIGNED)";
		{
			// create index
			int c;
			sql << "SELECT Count(1) IndexIsThere FROM INFORMATION_SCHEMA.STATISTICS "
				"WHERE table_schema=DATABASE() AND table_name='block_history_" << w->get_name ()
				<< "' AND index_name='bh_index_" << w->get_name () << "'", soci::into (c);
			if (c == 0)
				sql.once <<
					"CREATE INDEX `bh_index_" << w->get_name () << "` ON `block_history_"
					<< w->get_name () << "` (`x`, `y`, `z`)";
		}
	}
	
	/* 
	 * Constructs a new empty world.
	 */
	world::world (world_type typ, server &srv, const char *name, logger &log,
		world_generator *gen, world_provider *provider)
		: srv (srv), log (log), physics (srv), lm (log, this), blhi (*this), estage (this)
	{
		assert (world::is_valid_name (name));
		std::strcpy (this->name, name);
		
		this->typ = typ;
		
		this->gen = gen;
		this->width = 0;
		this->depth = 0;
		
		this->prov = provider;
		this->edge_chunk = nullptr;
		this->last_chunk = {0, 0, nullptr};
		
		this->players = new player_list ();
		this->th_running = false;
		this->auto_lighting = true;
		this->ticks = this->wtime = 0;
		this->wtime_frozen = false;
		this->use_def_inv = false;
		this->def_gm = GT_SURVIVAL;
		
		this->ph_state = PHY_OFF;
		//this->physics.set_thread_count (0);
		
		_init_sql_tables (this, srv);
	}
	
	/* 
	 * Class destructor.
	 */
	world::~world ()
	{
		this->srv.deregister_world (this);
		this->srv.cgen.cancel_requests (this);
		
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
		}
		
		{
			std::lock_guard<std::mutex> guard {this->bad_chunk_lock};
			for (auto tch : this->bad_chunks)
				delete tch.ch;
		}
		
		{
			std::lock_guard<std::mutex> guard {this->portal_lock};
			for (portal *ptl : this->portals)
				delete ptl;
		}
	}
	
	
	
	world*
	world::load_world (server &srv, const char *name)
	{
		srv.get_logger () () << " - Loading world \"" << name << "\"" << std::endl;
		
		std::string prov_name = world_provider::determine ("data/worlds", name);
		if (prov_name.empty ())
			{
				srv.get_logger () (LT_ERROR) << " - Failed to load world: world does not exist" << std::endl;
				return nullptr; // we don't throw
			}
		
		world_provider *prov = world_provider::create (prov_name.c_str (),
			"data/worlds", name);
		if (!prov)
			{
				srv.get_logger () (LT_ERROR) << " - Failed to load world: invalid provider \"" << prov_name << "\"" << std::endl;
				throw world_load_error ("Invalid provider");
			}
		
		const world_information& winf = prov->info ();
		world_generator *gen = world_generator::create (winf.generator.c_str (), winf.seed);
		if (!gen)
			{
				srv.get_logger () (LT_ERROR) << " - Failed to load world: invalid generator \"" << winf.generator << "\"" << std::endl;
				delete prov;
				throw world_load_error ("Invalid generator");
			}
		
		world_type wtyp;
		if (winf.world_type.compare ("NORMAL") == 0)
			wtyp = WT_NORMAL;
		else if (winf.world_type.compare ("LIGHT") == 0)
			wtyp = WT_LIGHT;
		else
			{
				srv.get_logger () (LT_ERROR) << " - Failed to load world: world type" << std::endl;
				delete gen;
				delete prov;
				throw world_load_error ("Corrupt world");
			}
		
		world *wr = new world (wtyp, srv, name, srv.get_logger (), gen, prov);
		wr->set_size (winf.width, winf.depth);
		wr->set_spawn (winf.spawn_pos);
		wr->def_gm = (winf.def_gm == "CREATIVE") ? GT_CREATIVE : GT_SURVIVAL;
		wr->def_inv = winf.def_inv;
		wr->use_def_inv = winf.use_def_inv;
		wr->wtime = winf.time;
		wr->wtime_frozen = winf.time_frozen;
		
		wr->prov->open (*wr);
		wr->prov->load_portals (*wr, wr->portals);
		wr->prov->load_security (*wr, wr->security ());
		{
			// zones
			std::vector<zone *> vzones;
			wr->prov->load_zones (*wr, vzones);
			for (zone *zn : vzones)
				wr->zman.add (zn);
		}
		wr->prov->close ();
		
		wr->prepare_spawn (10, false);
		
		return wr;
	}
	
	
	
	/* 
	 * Reloads the world from the specified path.
	 */
	void
	world::reload_world (const char *name)
	{
		{
			// acquire all locks
			std::lock_guard<std::mutex> ch_guard {this->chunk_lock};
			std::lock_guard<std::mutex> gen_guard {this->gen_lock};
			std::lock_guard<std::mutex> guard {this->bad_chunk_lock};
			std::lock_guard<std::mutex> ent_guard {this->entity_lock};
			std::lock_guard<std::mutex> ptl_guard {this->portal_lock};
			
			this->srv.cgen.cancel_requests (this);
			
			std::string prov_name = world_provider::determine ("data/worlds", name);
			if (prov_name.empty ())
				{
					srv.get_logger () (LT_ERROR) << " - Failed to reload world: world does not exist" << std::endl;
					throw world_load_error ("World does not exist");
				}
			
			world_provider *prov = world_provider::create (prov_name.c_str (),
				"data/worlds", name);
			if (!prov)
				{
					srv.get_logger () (LT_ERROR) << " - Failed to reload world: invalid provider \"" << prov_name << "\"" << std::endl;
					throw world_load_error ("Invalid provider");
				}
			
			const world_information& winf = prov->info ();
			world_generator *gen = world_generator::create (winf.generator.c_str (), winf.seed);
			if (!gen)
				{
					srv.get_logger () (LT_ERROR) << " - Failed to reload world: invalid generator \"" << winf.generator << "\"" << std::endl;
					delete prov;
					throw world_load_error ("Invalid generator");
				}
			
			world_type wtyp;
			if (winf.world_type.compare ("NORMAL") == 0)
				wtyp = WT_NORMAL;
			else if (winf.world_type.compare ("LIGHT") == 0)
				wtyp = WT_LIGHT;
			else
				{
					srv.get_logger () (LT_ERROR) << " - Failed to reload world: world type" << std::endl;
					delete gen;
					delete prov;
					throw world_load_error ("Corrupt world");
				}
			
			/* 
			 * Release resources
			 */
			delete this->gen;
			if (this->prov)
				delete this->prov;
			
			for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
				{
					chunk *ch = itr->second;
					
					int cx, cz;
					chunk_coords (itr->first, &cx, &cz);
					
					this->bad_chunks.push_back ({cx, cz, ch});
				}
			this->chunks.clear ();
			
			for (portal *ptl : this->portals)
				delete ptl;
			this->portals.clear ();
			
			// reassign
			this->typ = wtyp;
			this->gen = gen;
			this->prov = prov;
		
			this->set_size (winf.width, winf.depth);
			this->set_spawn (winf.spawn_pos);
			this->def_gm = (winf.def_gm == "CREATIVE") ? GT_CREATIVE : GT_SURVIVAL;
			this->def_inv = winf.def_inv;
			this->use_def_inv = winf.use_def_inv;
			this->wtime = winf.time;
			this->wtime_frozen = winf.time_frozen;
			
			this->prov->open (*this);
			this->prov->load_portals (*this, this->portals);
			this->prov->load_security (*this, this->security ());
			{
				// zones
				std::vector<zone *> vzones;
				this->prov->load_zones (*this, vzones);
				for (zone *zn : vzones)
					this->zman.add (zn);
			}
			this->prov->close ();
		}
		
		this->prepare_spawn (10, false);
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
	
	std::string
	world::get_colored_name ()
	{
		enum term_type
			{
				TT_GROUP,
				TT_ATLEAST_GROUP,
				TT_PLAYER,
				TT_PERM
			};
		
		group *min_grp = nullptr;
		bool player_world = false;
		bool perm_world   = false;
		
		const std::string& astr = this->wsec.get_build_perms ();
		const char *str = astr.c_str ();
		const char *ptr = str;
		while (*ptr)
			{
				term_type typ = TT_GROUP;
				std::string word;
				int neg = 0;
				
				while (*ptr && (*ptr != '|'))
					{
						int c = *ptr;
						switch (c)
							{
								case '!':
									++ neg;
									if (neg > 1)
										return this->get_name ();
									break;
								
								case '^':
									if (typ == TT_PLAYER)
										return this->get_name ();
									typ = TT_PLAYER;
									break;
								
								case '*':
									if (typ == TT_PERM)
										return this->get_name ();
									typ = TT_PERM;
									break;
								
								case '>':
									if (typ == TT_ATLEAST_GROUP)
										return this->get_name ();
									typ = TT_ATLEAST_GROUP;
									break;
								
								default:
									word.push_back (c);
							}
						++ ptr;
					}
				
				if (*ptr == '|')
					++ ptr;
				
				bool bad = false;
				switch (typ)
					{
						case TT_GROUP:
						case TT_ATLEAST_GROUP:
							{
								group *grp = this->srv.get_groups ().find (word.c_str ());
								if (!grp)
									bad = true;
								if (!min_grp || (grp->power < min_grp->power))
									min_grp = grp;
							}
							break;
						
						case TT_PLAYER:
							if (!neg)
								player_world = true;
							break;
						
						case TT_PERM:
							if (!neg)
								perm_world = true;
							break;
					}
				
				if (bad)
					return std::string (this->get_name ());
			}
		
		std::string colname;
		
		if (min_grp)
			{
				colname.append ("§");
				colname.push_back (min_grp->color);
			}
		else if (player_world)
			{
				colname.append ("§7^§e");
			}
		else if (perm_world)
			{
				colname.append ("§c*§f");
			}
		else
			{
				group *grp = this->srv.get_groups ().lowest ();
				if (grp)
					{
						colname.append ("§");
						colname.push_back (grp->color);
					}
			}
		
		colname.append (this->get_name ());
		return colname;
	}
	
	
	
	void
	world::set_generator (world_generator *gen)
	{
		std::lock_guard<std::mutex> guard {this->gen_lock};
		
		if (gen == this->gen)
			return;
		
		this->srv.cgen.cancel_requests (this);
		delete this->gen;
		this->gen = gen;
	}
	
	
	
	/* 
	 * Starts the world's "physics"-handling thread.
	 */
	void
	world::start ()
	{
		if (this->typ == WT_LIGHT)
			return;
		if (this->th_running)
			return;
		
		this->start_physics ();
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
	
	
	
//===========
	/* 
	 * Doors.
	 */
	
#define DOOR_TICK_RATE 4

	static void _door_tick (world &w, int x, int y, int z, int extra, std::minstd_rand& rnd);
	
	static bool
	_try_door_lock (world &w, int x, int y, int z, block_data bd)
	{
		if (bd.ex != BE_DOOR) return false;
		
		w.queue_update (x, y, z, BT_AIR);
		w.queue_physics (x, y, z, bd.id | (bd.meta << 12), nullptr, DOOR_TICK_RATE, nullptr, _door_tick);
		
		return true;
	}
	
	void
	_door_tick (world &w, int x, int y, int z, int data, std::minstd_rand& rnd)
	{
		int id = data & 0xFFF;
		int meta = (data >> 12) & 0xF;
		int elapsed = data >> 16;
		
		if (elapsed == 0)
			{
				_try_door_lock (w, x - 1, y, z, w.get_block (x - 1, y, z));
				_try_door_lock (w, x + 1, y, z, w.get_block (x + 1, y, z));
				_try_door_lock (w, x, y, z - 1, w.get_block (x, y, z - 1));
				_try_door_lock (w, x, y, z + 1, w.get_block (x, y, z + 1));
				if (y > 0)   _try_door_lock (w, x, y - 1, z, w.get_block (x, y - 1, z));
				if (y < 255) _try_door_lock (w, x, y + 1, z, w.get_block (x, y + 1, z));
			}
		
		++ elapsed;
		if (elapsed == (5 * 4)) // 4 seconds
			w.queue_update (x, y, z, id, meta, BE_DOOR);
		else
			w.queue_physics (x, y, z, id | (meta << 12) | (elapsed << 16),
				nullptr, DOOR_TICK_RATE, nullptr, _door_tick);
	}
	
	static bool
	_try_door_nolock (world &w, int x, int y, int z, block_data bd)
	{
		if (bd.ex != BE_DOOR) return false;
		
		w.queue_update_nolock (x, y, z, BT_AIR);
		w.queue_physics (x, y, z, bd.id | (bd.meta << 12), nullptr, DOOR_TICK_RATE, nullptr, _door_tick);
		
		return true;
	}
	
//===========
	
	
	
	/* 
	 * The function ran by the world's thread.
	 */
	void
	world::worker ()
	{
		const static int block_update_cap = 10000; // per tick
		const static int light_update_cap = 10000; // per tick
		
		int update_count;
		dense_edit_stage pl_tr {this};
		
		this->ticks = 0;
		while (this->th_running)
			{
				++ this->ticks;
				{
					std::lock_guard<std::mutex> guard {this->update_lock};
					
					/* 
					 * Dispose of unused chunks.
					 */
					{
						std::lock_guard<std::mutex> guard {this->bad_chunk_lock};
						for (auto itr = this->bad_chunks.begin (); itr != this->bad_chunks.end (); )
							{
								auto tch = *itr;
								
								// make sure there aren't any players near this chunk
								bool used = false;
								this->get_players ().all (
									[&used, tch] (player *pl)
										{
											if (used) return;
											
											if (pl->can_see_chunk (tch.cx, tch.cz))
												used = true;
										});
								
								if (!used)
									{
										delete tch.ch;
										itr = this->bad_chunks.erase (itr);
									}
								else
									++ itr;
							}
					}
					
					
					/* 
					 * Block updates.
					 */
					if (!this->updates.empty ())
						{
							std::lock_guard<std::mutex> lm_guard {this->lm.get_lock ()};
							
							std::vector<player *> pl_vc;
							this->get_players ().populate (pl_vc);
							
							update_count = 0;
							while (!this->updates.empty () && (update_count++ < block_update_cap))
								{
									block_update &u = this->updates.front ();
									
									if (((this->width > 0) && ((u.x >= this->width) || (u.x < 0))) ||
										((this->depth > 0) && ((u.z >= this->depth) || (u.z < 0))) ||
										((u.y < 0) || (u.y > 255)))
										{
											this->updates.pop_front ();
											continue;
										}
									
									block_data old_bd = this->get_block (u.x, u.y, u.z);
									if (old_bd.id == u.id && old_bd.meta == u.meta && old_bd.ex == u.extra)
										{
											// nothing modified
											this->updates.pop_front ();
											continue;
										}
									
									// doors
									if (u.id == BT_AIR && u.pl)
										{
											if (_try_door_nolock (*this, u.x, u.y, u.z, old_bd))
												{
													this->updates.pop_front ();
													continue;
												}
										}
									
									block_info *old_inf = block_info::from_id (old_bd.id);
									block_info *new_inf = block_info::from_id (u.id);
							
									physics_block *ph = physics_block::from_id (u.id);

									
									unsigned short old_id = this->get_id (u.x, u.y, u.z);
									unsigned char old_meta = this->get_meta (u.x, u.y, u.z);
									physics_block *old_ph = physics_block::from_id (old_id);
									if (old_ph && (!old_ph->breakable () && (u.id == 0)))
										{
											old_ph->on_break_attempt (*this, u.x, u.y, u.z);
											(u.pl)->send (packets::play::make_block_change (u.x, u.y, u.z, old_id, old_meta));
											this->updates.pop_front ();
											continue;
										}
									
									if ((old_id == u.id) && (old_meta == u.meta))
										{
											this->updates.pop_front ();
											continue;
										}
									
									this->set_block (u.x, u.y, u.z, u.id, u.meta, u.extra);
									if (u.pl)
										{
											// block history
											this->blhi.insert (u.x, u.y, u.z, old_bd, {u.id, u.meta, (unsigned char)u.extra}, u.pl);
											
											// block undo
											(u.pl)->bundo->insert ({u.x, u.y, u.z, old_bd.id, old_bd.meta, old_bd.ex,
												u.id, u.meta, (unsigned char)u.extra, std::time (nullptr)});
										}
							
									chunk *ch = this->get_chunk_at (u.x, u.z);
									if (new_inf->opaque != old_inf->opaque)
										ch->recalc_heightmap (u.x & 0xF, u.z & 0xF);
									
									// update players
									pl_tr.set (u.x, u.y, u.z,
										ph ? ph->vanilla_block ().id : u.id, 
										ph ? ph->vanilla_block ().meta : u.meta);
									
									if (ch)
										{
											if (auto_lighting)
												{
													this->lm.enqueue_nolock (u.x, u.y, u.z);
												}
											
											// physics
											if (old_id != u.id || old_meta != u.meta)
												{
													if (old_ph)
														old_ph->on_modified (*this, u.x, u.y, u.z);
												}
											if (ph && u.physics)
												{
													this->queue_physics (u.x, u.y, u.z, u.data, u.ptr,
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
				if (!this->wtime_frozen && ((this->ticks % 10) == 0))
					++ this->wtime;
			}
	}
	
	
	
	void
	world::set_width (int width)
	{
		if (width % 16 != 0)
			width += 16 - (width % 16);
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
			depth += 16 - (depth % 16);
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
		inf.world_type = (this->typ == WT_LIGHT) ? "LIGHT" : "NORMAL";
		inf.def_gm = (this->def_gm == GT_SURVIVAL) ? "SURVIVAL" : "CREATIVE";
		inf.def_inv = this->def_inv;
		inf.use_def_inv = this->use_def_inv;
		inf.time = this->wtime;
		inf.time_frozen = this->wtime_frozen;
	}
	
	
	
	/* 
	 * Saves all modified chunks to disk.
	 */
	void
	world::save_all ()
	{
		if (this->prov == nullptr)
			return;
		
		std::lock_guard<std::mutex> ch_guard {this->chunk_lock};
		std::lock_guard<std::mutex> gen_guard {this->gen_lock};
		std::lock_guard<std::mutex> ptl_guard {this->portal_lock};
		std::lock_guard<std::mutex> ent_guard {this->entity_lock};
		
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
		
		// security
		this->prov->save_security (*this, this->security ());
		
		// portals
		this->prov->save_portals (*this, this->portals);
		
		// zones
		this->prov->save_zones (*this, this->zman.get_all ());
		
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
		this->put_chunk_nolock (x, z, ch, true);
	}
	
	void
	world::put_chunk_nolock (int x, int z, chunk *ch, bool lock)
	{
		unsigned long long key = chunk_key (x, z);
		
		std::unique_lock<std::mutex> guard {this->chunk_lock, std::defer_lock};
		if (lock)
			guard.lock ();
		
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
	
	
	
	/* 
	 * Searches the chunk world for a chunk located at the specified coordinates.
	 */
	chunk*
	world::get_chunk (int x, int z)
	{
		return this->get_chunk_nolock (x, z, true);
	}
	
	chunk*
	world::get_chunk_nolock (int x, int z, bool lock)
	{
		if (!this->chunk_in_bounds (x, z))
			return this->edge_chunk;
		
		unsigned long long key = chunk_key (x, z);
		
		std::unique_lock<std::mutex> guard {this->chunk_lock, std::defer_lock};
		if (lock)
			guard.lock ();
		
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
		return this->load_chunk_nolock (x, z, true);
	}
	
	chunk*
	world::load_chunk_nolock (int x, int z, bool lock)
	{
		std::unique_lock<std::mutex> ch_guard {this->chunk_lock, std::defer_lock};
		if (lock)
			ch_guard.lock ();
		
		chunk *ch = this->get_chunk_nolock (x, z);
		if (ch && ch->generated) return ch;
		else if (!ch)
			{
				ch = new chunk ();
				
				// try to load from disk
				{
					std::unique_lock<std::mutex> gen_guard {this->gen_lock, std::defer_lock};
					if (lock)
						gen_guard.lock ();
					
					this->prov->open (*this);
					if (this->prov->load (*this, ch, x, z))
						{
							if (ch->generated)
								{
									ch->recalc_heightmap ();
									this->prov->close ();
									this->put_chunk_nolock (x, z, ch);
									return ch;
								}
						}
					this->prov->close ();
				}
				
				this->put_chunk_nolock (x, z, ch);
			}
		
		if (lock)
			ch_guard.unlock ();
		
		this->gen->generate (*this, ch, x, z);
		ch->generated = true;
		ch->recalc_heightmap ();
		this->lm.relight_chunk (ch);
		return ch;
	}
	
	
	/* 
	 * Unloads and saves (if save = true) the chunk located at the specified
	 * coordinates.
	 */
	void 
	world::remove_chunk (int x, int z, bool save)
	{
		if (!this->chunk_in_bounds (x, z))
			return;
		
		unsigned long long key = chunk_key (x, z);
		
		std::lock_guard<std::mutex> guard {this->chunk_lock};
		auto itr = this->chunks.find (key);
		if (itr != this->chunks.end ())
			{
				chunk *ch = itr->second;
				
				if (save)
					{
						this->prov->open (*this);
						this->prov->save (*this, ch, x, z);
						this->prov->close ();
					}
				
				this->chunks.erase (itr);
				
				{
					std::lock_guard<std::mutex> guard {this->bad_chunk_lock};
					this->bad_chunks.push_back ({x, z, ch});
				}
			}
	}
	
	/* 
	 * Removes all chunks from the world and optionally saves them to disk.
	 */
	void
	world::clear_chunks (bool save, bool del)
	{
		std::lock_guard<std::mutex> guard {this->chunk_lock};
		
		if (save)
			this->prov->open (*this);
		else if (del)
			{
				std::remove (this->get_path ());
			}
		
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			{
				chunk *ch = itr->second;
				int x, z;
				chunk_coords (itr->first, &x, &z);
						
				if (save)
					{
						this->prov->save (*this, ch, x, z);
					}
				
				{
					std::lock_guard<std::mutex> guard {this->bad_chunk_lock};
					this->bad_chunks.push_back ({x, z, ch});
				}
			}
		this->chunks.clear ();
		
		if (save)
			this->prov->close ();
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
		this->srv.global_physics.queue_physics (this, e->get_eid ());
		
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
		unsigned char meta, int extra, int data, void *ptr, player *pl, bool physics)
	{
		std::lock_guard<std::mutex> guard {this->update_lock};
		this->queue_update_nolock (x, y, z, id, meta, extra,data, ptr, pl, physics);
	}
	
	void
	world::queue_update_nolock (int x, int y, int z, unsigned short id,
		unsigned char meta, int extra, int data, void *ptr, player *pl, bool physics)
	{
		if (!this->in_bounds (x, y, z)) return;
		if (this->typ == WT_LIGHT)
			{
				block_data old_bd = this->get_block (x, y, z);
				this->set_block (x, y, z, id, meta);
				this->lm.enqueue (x, y, z);
				
				// block history
				this->blhi.insert (x, y, z, old_bd, {id, meta, (unsigned char)extra}, pl);
				
				// block undo
				pl->bundo->insert ({x, y, z, old_bd.id, old_bd.meta, old_bd.ex,
					id, meta, (unsigned char)extra, std::time (nullptr)});
				
				// update players
				this->get_players ().all (
					[x, y, z, id, meta] (player *pl)
						{
							pl->send (packets::play::make_block_change (x, y, z, id, meta));
						});
				return;
			}
		
		this->updates.emplace_back (x, y, z, id, meta, extra, data, ptr, pl, physics);
		
		std::lock_guard<std::mutex> estage_guard {this->estage_lock};
		this->estage.set (x, y, z, id, meta, extra);
	}
	
	void
	world::queue_physics (int x, int y, int z, int extra, void *ptr,
		int tick_delay, physics_params *params, physics_block_callback cb)
	{
		if (!this->in_bounds (x, y, z)) return;
		if (this->ph_state == PHY_OFF) return;
		if (this->typ == WT_LIGHT) return;
		
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
		if (this->typ == WT_LIGHT) return;
		
		if (this->physics.get_thread_count () == 0)
			this->srv.global_physics.queue_physics_once (this, x, y, z, extra, tick_delay, params, cb);
		else
			this->physics.queue_physics_once (this, x, y, z, extra, tick_delay, params, cb);
	}
	
	
	void
	world::start_physics ()
	{
		if (this->typ == WT_LIGHT) return;
		this->ph_state = PHY_ON;
	}
	
	void
	world::stop_physics ()
	{
		if (this->ph_state == PHY_OFF) return;
		this->ph_state = PHY_OFF;
		
		if (this->physics.get_thread_count () != 0)
			this->physics.stop ();
	}
	
	void
	world::pause_physics ()
	{
		if (this->typ == WT_LIGHT) return;
		if (this->ph_state == PHY_PAUSED) return;
		this->ph_state = PHY_PAUSED;
	}
	
	
	
//-----
	
	/* 
	 * Checks whether the specified player can modify the block located at the
	 * given coordinates.
	 */
	bool
	world::can_build_at (int x, int y, int z, player *pl)
	{
		std::vector<zone *> zones;
		this->zman.find (x, y, z, zones);
		
		for (zone *zn : zones)
			if (!zn->get_security ().can_build (pl))
				return false;
		
		return true;
	}
	
	
	
//-----
	
	/* 
	 * Finds and returns the portal located at the given block coordinates,
	 * or null, if one is not found.
	 */
	portal* 
	world::get_portal (int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->portal_lock};
		
		for (portal *p : this->portals)
			{
				if (p->in_range (x, y, z))
					{
						for (block_pos bp : p->affected)
							if (bp.x == x && bp.y == y && bp.z == z)
								return p;
					}
			}
		
		return nullptr;
	}
	
	/* 
	 * Adds the specified portal to the world's portal list
	 */
	void
	world::add_portal (portal *ptl)
	{
		std::lock_guard<std::mutex> guard {this->portal_lock};
		this->portals.push_back (ptl);
	}
}

