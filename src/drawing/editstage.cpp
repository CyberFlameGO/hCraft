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

#include "drawing/editstage.hpp"
#include "world/world.hpp"
#include "world/chunk.hpp"
#include "player/player.hpp"
#include "player/player_list.hpp"
#include "physics/blocks/physics_block.hpp"
#include <cstring>
#include <mutex>

#include <iostream> // DEBUG


namespace hCraft {
	
	void
	edit_stage::set_world (world *w, bool reset)
	{
		this->w = w;
		if (reset)
			this->clear ();
	}
	
	
	void
	edit_stage::preview_to (player *pl, bool update_sbs)
	{
		std::vector<player *> vec (1, pl);
		this->preview (vec, update_sbs);
	}
	
	void
	edit_stage::preview_chunk_to (player *pl, int cx, int cz, bool update_sbs)
	{
		std::vector<player *> vec (1, pl);
		this->preview_chunk (vec, cx, cz, update_sbs);
	}
		
	void
	edit_stage::restore_to (player *pl, bool update_sbs)
	{
		std::vector<player *> vec (1, pl);
		this->restore (vec, update_sbs);
	}
	
	
	
//------------------------------------------------------------------------------
	
	des_microchunk::des_microchunk ()
	{
		std::memset (this->data, 0xFF, 512 * sizeof (unsigned short));
	}
	
	
	
	des_subchunk::des_subchunk ()
	{
		for (int i = 0; i < 8; ++i)
			this->micro[i] = nullptr;
	}
	
	des_subchunk::~des_subchunk ()
	{
		for (int i = 0; i < 8; ++i)
			delete this->micro[i];
	}
	
	
	
	des_chunk::des_chunk ()
	{
		for (int i = 0; i < 16; ++i)
			this->subs[i] = nullptr;
		this->mod_count = 0;
	}
	
	des_chunk::~des_chunk ()
	{
		for (int i = 0; i < 16; ++i)
			delete this->subs[i];
	}
	
	
	
//----
	
	dense_edit_stage::dense_edit_stage (world *w)
		: edit_stage (w)
		{ }
	
	
	
	/* 
	 * Block modification \ retreival:
	 */
	
	void
	dense_edit_stage::set (int x, int y, int z, unsigned short id, unsigned char meta, unsigned char ex)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		des_chunk &ch = this->chunks[{cx, cz}];
		
		int sy = y >> 4;
		des_subchunk *sub = ch.subs[sy];
		if (!sub)
			sub = ch.subs[sy] = new des_subchunk ();
		
		int bx = x & 0xF;
		int by = y & 0xF;
		int bz = z & 0xF;
		int m_index = ((by >> 3) << 2) | ((bz >> 3) << 1) | ((bx >> 3));
		des_microchunk *micro = sub->micro[m_index];
		if (!micro)
			micro = sub->micro[m_index] = new des_microchunk ();
		
		int b_index = ((y & 0x7) << 6) | ((z & 0x7) << 3) | ((x & 0x7));
		
		// update mod count
		if ((micro->data[b_index] >> 4) != ES_NONE) {
			if (id == ES_NONE)
				-- ch.mod_count;
		} else {
			 if (id != ES_NONE)
			 	++ ch.mod_count;
		}
			
		micro->data[b_index] = (id << 4) | (meta & 0xF);
		micro->ex[b_index]    = ex;
	}
	
	blocki
	dense_edit_stage::get (int x, int y, int z)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		
		des_chunk *ch;
		auto itr = this->chunks.find ({cx, cz});
		if (itr == this->chunks.end ())
			{
				block_data bd = this->w->get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		else
			ch = &itr->second;
		
		int sy = y >> 4;
		des_subchunk *sub = ch->subs[sy];
		if (!sub)
			{
				block_data bd = this->w->get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		int bx = x & 0xF;
		int by = y & 0xF;
		int bz = z & 0xF;
		int m_index = ((by >> 3) << 2) | ((bz >> 3) << 1) | ((bx >> 3));
		des_microchunk *micro = sub->micro[m_index];
		if (!micro)
			{
				block_data bd = this->w->get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		int b_index = ((y & 0x7) << 6) | ((z & 0x7) << 3) | ((x & 0x7));
		unsigned short val = micro->data[b_index];
		unsigned short id  = val >> 4;
		unsigned char  ex  = micro->ex[b_index];
		if (id == ES_NONE || id == ES_REM)
			{
				block_data bd = this->w->get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		return {id, (unsigned char)(val & 0xF), ex};
	}
	
	void
	dense_edit_stage::reset (int x, int y, int z)
	{
		this->set (x, y, z, ES_NONE, 0xF);
	}
	
	int
	dense_edit_stage::mod_count_at (int cx, int cz)
	{
		auto itr = this->chunks.find ({cx, cz});
		if (itr == this->chunks.end ())
			return 0;
		else
			return itr->second.mod_count;
	}
	
	
	
	void
	dense_edit_stage::send_to_players (std::vector<player *>& _players,
	  int cx, int cz, des_chunk& ch, bool restore, bool update_sbs)
	{
		std::vector<player *> players (_players);
		
		// don't send to players that are too far away
		for (auto itr = players.begin (); itr != players.end (); )
			{
				player *pl = *itr;
				
				if ((pl->get_world () == this->w) && pl->can_see_chunk (cx, cz))
					++ itr;
				else
					itr = players.erase (itr);
			}
		
		if (players.empty ())
			return;
		
		block_data bd;
		unsigned short id;
		unsigned char meta;
		
		int bx, by, bz;
		
		std::vector<block_change_record> records;
		for (int sy = 0; sy < 16; ++sy)
			{
				des_subchunk *sub = ch.subs[sy];
				if (!sub) continue;
				
				for (int mi = 0; mi < 8; ++mi)
					{
						des_microchunk *micro = sub->micro[mi];
						if (!micro) continue;
						
						for (int bi = 0; bi < 512; ++bi)
							{
								id = micro->data[bi] >> 4;
								if (id != ES_NONE)
									{
										bx = ((mi & 1) << 3) | (bi & 0x7);
										by = (sy << 4) | (((mi >> 2) & 1) << 3) | ((bi >> 6) & 0x7);
										bz = (((mi >> 1) & 1) << 3) | ((bi >> 3) & 0x7);
										
										if (restore || (id == ES_REM))
											{
												bd = this->w->get_block ((cx << 4) | bx, by, (cz << 4) | bz);
												id = bd.id;
												meta = bd.meta;
											}
										else
											meta = micro->data[bi] & 0xF;
										
										records.push_back ({(unsigned char)bx, (unsigned char)by, (unsigned char)bz, id, meta});
									}
							}
					}
			}
		
		for (player *pl : players)
			if (pl->get_world () == this->w && pl->can_see_chunk (cx, cz))
				pl->send (packets::play::make_multi_block_change (cx, cz, records, pl));
	}
	
	
	
	/* 
	 * Sends all modified blocks to the specified player(s).
	 */
	void
	dense_edit_stage::preview (std::vector<player *>& players, bool update_sbs)
	{
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			this->send_to_players (players, itr->first.x, itr->first.z, itr->second,
				false, update_sbs);
	}
	
	
	/* 
	 * Same as preview (), but only sends a single chunk.
	 */
	void
	dense_edit_stage::preview_chunk (std::vector<player *>& players, int cx, int cz,
		bool update_sbs)
	{
		auto itr = this->chunks.find ({cx, cz});
		if (itr != this->chunks.end ())
			this->send_to_players (players, cx, cz, itr->second, false, update_sbs);
	}
	
	
	/* 
	 * Restores back all block modifications sent by preview().
	 */
	void
	dense_edit_stage::restore (std::vector<player *>& players, bool update_sbs)
	{
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			this->send_to_players (players, itr->first.x, itr->first.z, itr->second,
				true, update_sbs);
	}
	
	
	
	/* 
	 * Commits all block modifications to the underlying world.
	 * The edit stage is then cleared.
	 */
	void
	dense_edit_stage::commit (bool physics)
	{
		static const int chunk_cap = 3000;
		
		if (this->chunks.empty ())
			return;
		
		block_pos bound_min = { 0x7FFFFFFF,  0x7FFFFFFF, 0x7FFFFFFF};
		block_pos bound_max = {-0x7FFFFFFF, -0x7FFFFFFF,-0x7FFFFFFF};
		
		std::vector<player *> affected_players;
		auto& chunks_ref = this->chunks;
		world *w = this->w;
		this->w->get_players ().all (
			[&affected_players, &chunks_ref, w] (player *pl)
				{
					bool found_common_chunk = false;
					for (auto itr = chunks_ref.begin (); itr != chunks_ref.end (); ++itr)
						{
							if ((pl->get_world () == w) && pl->can_see_chunk (itr->first.x, itr->first.z))
								{
									found_common_chunk = true;
									break;
								}
						}
					
					if (found_common_chunk)
						affected_players.push_back (pl);
				});
		
		std::lock_guard<std::mutex> u_guard ((this->w->update_lock));
		std::lock_guard<std::mutex> es_guard ((this->w->estage_lock));
		std::lock_guard<std::mutex> lm_guard ((this->w->lm.get_lock ()));
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			{
				int cx = itr->first.x;
				int cz = itr->first.z;
				
				chunk *wch = this->w->load_chunk (cx, cz);
				if (wch == this->w->get_edge_chunk ())
					continue;
				des_chunk &ch = itr->second;
				
				std::vector<block_change_record> records;
				bool add_records = (ch.mod_count < chunk_cap);
				
				std::bitset<256> column_changed;
				
				unsigned short id;
				unsigned char meta;
				unsigned ex;
				int rx, ry, rz;
				
				for (int sy = 0; sy < 16; ++sy)
					{
						int yy = sy << 4;
						des_subchunk *sub = ch.subs[sy];
						if (!sub)
							continue;
						
						for (int mi = 0; mi < 8; ++mi)
							{
								des_microchunk *micro = sub->micro[mi];
								if (!micro)
									continue;
								
								int mx = (mi & 1) << 3;
								int my = ((mi >> 2) & 1) << 3; 
								int mz = ((mi >> 1) & 1) << 3;
								for (int x = 0; x < 8; ++x)
									for (int z = 0; z < 8; ++z)
										for (int y = 0; y < 8; ++y)
											{
												unsigned int index = (y << 6) | (z << 3) | x;
										
												id = micro->data[index] >> 4;
												if (id != ES_NONE)
													{
														rx = mx | x;
														ry = my | y;
														rz = mz | z;
														column_changed.set ((rz << 4) | rx);
														
														int wx = (cx << 4) | rx;
														int wy = yy | ry;
														int wz = (cz << 4) | rz;
														
														meta = micro->data[index] & 0xF;
														ex   = micro->ex[index];
														if (id == ES_REM)
															{
																block_data bd = this->w->get_block (wx, wy, wz);
																id = bd.id;
																meta = bd.meta;
															}
														
														if (add_records)
															{
																block_change_record rec;
																rec.x = rx;
																rec.z = rz;
																rec.y = yy + ry;
																rec.id = id;
																rec.meta = meta;
																records.push_back (rec);
															}
												
														
														this->w->estage.set (wx, wy, wz, ES_NONE, 0xF, 0);
														
														// update boundaries
														if (wx < bound_min.x) bound_min.x = wx;
														if (wx > bound_max.x) bound_max.x = wx;
														if (wy < bound_min.y) bound_min.y = wy;
														if (wy > bound_max.y) bound_max.y = wy;
														if (wz < bound_min.z) bound_min.z = wz;
														if (wz > bound_max.z) bound_max.z = wz;
										
														wch->set_block (rx, wy, rz, id, meta, ex);
														
														//if (this->w->auto_lighting)
														// NOTE: we already acquired the lighting manager's lock,
														//       so this is perfectly safe.
														this->w->queue_lighting_nolock (wx, wy, wz);
														
														if (physics)
															{
																physics_block *ph = physics_block::from_id (id);
																if (ph)
																	this->w->queue_physics (wx, wy, wz, 0, nullptr, ph->tick_rate ());
															}
													}
											}
							}
					}
				
				// adjust heightmap
				for (int x = 0; x < 16; ++x)
					for (int z = 0; z < 16; ++z) 
						{
							if (column_changed.test ((z << 4) | x))
								wch->recalc_heightmap (x, z);
						}

				if (ch.mod_count >= chunk_cap)
					{
						for (player *pl : affected_players)
							{
								if ((pl->get_world () == this->w) &&pl->can_see_chunk (cx, cz))
									pl->send (packets::play::make_chunk (cx, cz, wch));
							}
					}
				else if (ch.mod_count > 200)
					{
						packet *mbcp = packets::play::make_multi_block_change (cx, cz, records);
						packet *cp   = packets::play::make_chunk (cx, cz, wch);
		
						// send the smaller between the two
						if (mbcp->size < cp->size)
							{
								delete cp;
								for (player *pl : affected_players)
									{
										if (pl->get_world () == this->w)
											pl->send (new packet (*mbcp));
									}
								delete mbcp;
							}
						else
							{
								delete mbcp;
								for (player *pl : affected_players)
									{
										if ((pl->get_world () == this->w) && pl->can_see_chunk (cx, cz))
											pl->send (new packet (*cp));
									}
								delete cp;
							}
					}
				else
					{
						packet *pack = packets::play::make_multi_block_change (cx, cz, records);
						for (player *pl : affected_players)
							{
								if (pl->get_world () == this->w)
									pl->send (new packet (*pack));
							}
						delete pack;
					}
			}
		
		// update player selections
		for (player *pl : affected_players)
			{
				if (pl->get_world () != this->w)
					continue;
				
				for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
					{
						world_selection *sel = itr->second;
						if (sel->visible ())
							{
								// make sure both bounding boxes overlap
								block_pos min = sel->min (), max = sel->max ();
								if (!(bound_max.x < min.x || bound_min.x > max.x ||
											bound_max.y < min.y || bound_min.y > max.y ||
											bound_max.z < min.z || bound_min.z > max.z))
									{
										sel->hide (pl);
										sel->show (pl);
									}
							}
					}
				pl->sb_commit ();
			}
	}
	
	/* 
	 * Does not notify players, nor does this activate physics blocks.
	 */
	void
	dense_edit_stage::commit_chunk (chunk *wch, int cx, int cz)
	{
		auto itr = this->chunks.find ({cx, cz});
		if (itr == this->chunks.end ())
			return;
		des_chunk& ch = itr->second;
				
		unsigned short id;
		unsigned char meta;
		unsigned ex;
		int rx, ry, rz;
		
		for (int sy = 0; sy < 16; ++sy)
			{
				int yy = sy << 4;
				des_subchunk *sub = ch.subs[sy];
				if (!sub)
					continue;
				
				for (int mi = 0; mi < 8; ++mi)
					{
						des_microchunk *micro = sub->micro[mi];
						if (!micro)
							continue;
						
						int mx = (mi & 1) << 3;
						int my = ((mi >> 2) & 1) << 3; 
						int mz = ((mi >> 1) & 1) << 3;
						for (int x = 0; x < 8; ++x)
							for (int z = 0; z < 8; ++z)
								for (int y = 0; y < 8; ++y)
									{
										unsigned int index = (y << 6) | (z << 3) | x;
								
										id = micro->data[index] >> 4;
										if (id != ES_NONE)
											{
												rx = mx | x;
												ry = my | y;
												rz = mz | z;
												
												int wx = (cx << 4) | rx;
												int wy = yy | ry;
												int wz = (cz << 4) | rz;
												
												meta = micro->data[index] & 0xF;
												ex   = micro->ex[index];
												if (id == ES_REM)
													{
														block_data bd = this->w->get_block (wx, wy, wz);
														id = bd.id;
														meta = bd.meta;
													}
								
												wch->set_block (rx, wy, rz, id, meta, ex);
												// TODO: lighting?
											}
									}
					}
			}
	}
	
	
	
	/* 
	 * Clears the edit stage.
	 */
	void
	dense_edit_stage::clear ()
	{
		this->chunks.clear ();
	}
	
	
	
//------------------------------------------------------------------------------
	
	sparse_edit_stage::sparse_edit_stage (world *w)
		: edit_stage (w)
		{ }
	
	
	
	/* 
	 * Block modification \ retrieval:
	 */
	void
	sparse_edit_stage::set (int x, int y, int z, unsigned short id, unsigned char meta, unsigned char ex)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		unsigned char bx = x & 15;
		unsigned char bz = z & 15;
		
		ses_chunk& ch = this->chunks[{cx, cz}];
		ch.changes[{bx, y, bz}] = (unsigned int)(ex << 16) | (id << 4) | (meta & 0xF);
	}
	
	
	blocki
	sparse_edit_stage::get (int x, int y, int z)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		
		auto itr = this->chunks.find ({cx, cz});
		if (itr == this->chunks.end ())
			{
				block_data bd = this->w->get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		unsigned char bx = x & 15;
		unsigned char bz = z & 15;
		
		ses_chunk& ch = itr->second;
		auto bitr = ch.changes.find ({bx, y, bz});
		if (bitr == ch.changes.end ())
			{
				block_data bd = this->w->get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		unsigned short id = (bitr->second >> 4) & 0xFFF;
		unsigned char  ex = bitr->second >> 16;
		if (id == ES_REM)
			{
				block_data bd = this->w->get_block (x, y, z);
				return {bd.id, bd.meta};
			}
			
		return {id, (unsigned char)((bitr->second) & 0xF), ex};
	}
	
	
	void
	sparse_edit_stage::reset (int x, int y, int z)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		
		auto itr = this->chunks.find ({cx, cz});
		if (itr == this->chunks.end ())
			return;
		
		unsigned char bx = x & 15;
		unsigned char bz = z & 15;
		
		ses_chunk& ch = itr->second;
		auto bitr = ch.changes.find ({bx, y, bz});
		if (bitr == ch.changes.end ())
			return;
		
		ch.changes.erase (bitr);
	}
	
	int
	sparse_edit_stage::mod_count_at (int cx, int cz)
	{
		auto itr = this->chunks.find ({cx, cz});
		if (itr == this->chunks.end ())
			return 0;
		else
			return itr->second.changes.size ();
	}
	
	
	
	namespace {
		struct sb_correction {
			player *pl;
			int x, y, z;
			
			sb_correction (player *pl, int x, int y, int z)
			{
				this->pl = pl;
				this->x = x;
				this->y = y;
				this->z = z;
			}
		};
	}
	
	void
	sparse_edit_stage::send_to_players (std::vector<player *>& _players,
		int cx, int cz, ses_chunk& ch, bool restore, bool update_sbs)
	{
		std::vector<player *> players (_players);
		if (_players.empty ())
			return;
		
		// don't send to players that are too far away
		for (auto itr = players.begin (); itr != players.end (); )
			{
				player *pl = *itr;
				if ((pl->get_world () == this->w) && pl->can_see_chunk (cx, cz))
					++ itr;
				else
					itr = players.erase (itr);
			}
		
		std::vector<sb_correction> corrections;
		std::vector<block_change_record> records;
		for (auto bitr = ch.changes.begin (); bitr != ch.changes.end (); ++bitr)
			{
				unsigned char x = bitr->first.x;
				unsigned char y = bitr->first.y;
				unsigned char z = bitr->first.z;
				
				// selection blocks
				if (update_sbs)
					for (player *pl : players)
						if ((pl->get_world () == this->w) && pl->sb_exists ((cx << 4) | x, y, (cz << 4) | z))
							corrections.emplace_back (pl, (cx << 4) | x, y, (cz << 4) | z);
				
				if (restore || ((((bitr->second) >> 4) & 0xFFF) == ES_REM))
					{
						block_data bd = this->w->get_block ((cx << 4) | x, y, (cz << 4) | z);
						records.push_back ({x, y, z, bd.id, bd.meta});
					}
				else
					records.push_back ({x, y, z,
						(unsigned short)(((bitr->second) >> 4) & 0xFFF), (unsigned char)((bitr->second) & 0xF)});
			}
		
		if (players.size () == 1)
			{
				if (players[0]->get_world () == this->w)
					players[0]->send
						(packets::play::make_multi_block_change (cx, cz, records));
			}
		else
			{
				packet *pack = packets::play::make_multi_block_change (cx, cz, records);
				for (player *pl : players)
					if (pl->get_world () == this->w)
						pl->send (new packet (*pack));
				delete pack;
			}
		
		// resend modified selection blocks
		for (sb_correction& sbc : corrections)
			sbc.pl->sb_send (sbc.x, sbc.y, sbc.z);
	}
	
	
	
	/* 
	 * Sends all modified blocks to the specified player(s).
	 */
	void
	sparse_edit_stage::preview (std::vector<player *>& players, bool update_sbs)
	{
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			this->send_to_players (players, itr->first.x, itr->first.z, itr->second,
				false, update_sbs);
	}
	
	
	/* 
	 * Same as preview (), but only sends a single chunk.
	 */
	void
	sparse_edit_stage::preview_chunk (std::vector<player *>& players, int cx, int cz,
		bool update_sbs)
	{
		auto itr = this->chunks.find ({cx, cz});
		if (itr != this->chunks.end ())
		 	{
				this->send_to_players (players, cx, cz, itr->second, false, update_sbs);
			}
	}
	
	
	/* 
	 * Restores back all block modifications sent by preview().
	 */
	void
	sparse_edit_stage::restore (std::vector<player *>& players, bool update_sbs)
	{
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			this->send_to_players (players, itr->first.x, itr->first.z, itr->second,
				true, update_sbs);
	}
	
	
	
	/* 
	 * Commits all block modifications to the underlying world.
	 * The edit stage is then cleared.
	 */
	void
	sparse_edit_stage::commit (bool physics)
	{
		std::vector<player *> affected_players;
		auto& chunks_ref = this->chunks;
		this->w->get_players ().all (
			[&affected_players, &chunks_ref] (player *pl)
				{
					bool found_common_chunk = false;
					for (auto itr = chunks_ref.begin (); itr != chunks_ref.end (); ++itr)
						{
							if (pl->can_see_chunk (itr->first.x, itr->first.z))
								{
									found_common_chunk = true;
									break;
								}
						}
					
					if (found_common_chunk)
						affected_players.push_back (pl);
				});
		
		int x, y, z;
		int wx, wz;
		unsigned short id;
		unsigned char meta;
		unsigned char ex;
		std::vector<sb_correction> corrections;
		
		std::lock_guard<std::mutex> u_guard ((this->w->update_lock));
		std::lock_guard<std::mutex> es_guard ((this->w->estage_lock));
		std::lock_guard<std::mutex> lm_guard ((this->w->lm.get_lock ()));
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			{
				int cx = itr->first.x;
				int cz = itr->first.z;
				
				chunk *wch = this->w->load_chunk (cx, cz);
				if (wch == this->w->get_edge_chunk ())
					continue;
				ses_chunk &ch = itr->second;
				
				std::vector<block_change_record> records;
				std::bitset<256> column_changed;
				
				for (auto bitr = ch.changes.begin (); bitr != ch.changes.end (); ++bitr)
					{
						x = bitr->first.x;
						y = bitr->first.y;
						z = bitr->first.z;
						id = ((bitr->second) >> 4) & 0xFFF;
						meta = (bitr->second) & 0xF;
						ex = (bitr->second) >> 16;
						
						wx = (cx << 4) | x;
						wz = (cz << 4) | z;
						if (id == ES_REM)
							{
								block_data bd = this->w->get_block (wx, y, wz);
								id = bd.id;
								meta = bd.meta;
							}
						
						column_changed.set ((z << 4) | x);
						this->w->estage.set (wx, y, wz, ES_NONE, 0xF);
						
						// selection blocks
						for (player *pl : affected_players)
							if (pl->sb_exists (wx, y, wz))
								corrections.emplace_back (pl, wx, y, wz);
						
						block_change_record rec;
						rec.x = x;
						rec.z = z;
						rec.y = y;
						rec.id = id;
						rec.meta = meta;
						records.push_back (rec);
						
						// update world
						wch->set_block (x, y, z, id, meta, ex);
						
						//if (this->w->auto_lighting)
						// NOTE: we already acquired the lighting manager's lock,
						//       so this is perfectly safe.
						this->w->queue_lighting_nolock (wx, y, wz);
						
						if (physics)
							{
								physics_block *ph = physics_block::from_id (id);
								if (ph)
									this->w->queue_physics (wx, y, wz, 0, nullptr, ph->tick_rate ());
							}
					}
				
				// adjust heightmap
				for (int x = 0; x < 16; ++x)
					for (int z = 0; z < 16; ++z) 
						{
							if (column_changed.test ((z << 4) | x))
								wch->recalc_heightmap (x, z);
						}
				
				// update players
				packet *mbcp = packets::play::make_multi_block_change (cx, cz, records);
				for (player *pl : affected_players)
					pl->send (new packet (*mbcp));
				delete mbcp;
				
				// resend modified selection blocks
				for (sb_correction& sbc : corrections)
					sbc.pl->sb_send (sbc.x, sbc.y, sbc.z);
			}
	}
	
	/* 
	 * Does not notify players, nor does this activate physics blocks.
	 */
	void
	sparse_edit_stage::commit_chunk (chunk *ch, int cx, int cz)
	{
		auto ch_itr = this->chunks.find ({cx, cz});
		if (ch_itr == this->chunks.end ())
			return;
		
		ses_chunk &sch = ch_itr->second;
		for (auto itr = sch.changes.begin (); itr != sch.changes.end (); ++itr)
			{
				block_pos pos = itr->first;
				unsigned int data = itr->second;
				ch->set_block (pos.x, pos.y, pos.z, (data >> 4) & 0xFFF, data & 0xF, data >> 16);
			}
	}
	
	
	
	/* 
	 * Clears the edit stage.
	 */
	void
	sparse_edit_stage::clear ()
	{
		this->chunks.clear ();
	}
	
	
	
//------------------------------------------------------------------------------
	
	/* 
	 * @{queue_updates} determins the set of methods that will be used to set\get
	 * blocks. If it's true, then all block modifications will be sent to the
	 * world's queue_update () method, otherwise, set_id_and_meta () will be used
	 * instead. The same applies for block retrieval - if @{queue_updates} is
	 * true, get_final_block(), otherwise, get_block().
	 */
	direct_edit_stage::direct_edit_stage (world *w, bool queue_updates)
		: edit_stage (w), queue_updates (queue_updates)
		{ }
		
		
	/* 
	 * Block modification \ retrieval:
	 */
	
	void
	direct_edit_stage::set (int x, int y, int z, unsigned short id, unsigned char meta, unsigned char ex)
	{
		if (this->queue_updates)
			this->w->queue_update (x, y, z, id, meta);
		else
			{
				this->w->set_block (x, y, z, id, meta, ex);
			}
	}
	
	blocki
	direct_edit_stage::get (int x, int y, int z)
	{
		if (this->queue_updates)
			{
				blocki bl = this->w->get_final_block (x, y, z);
				return {bl.id, bl.meta};
			}
		
		block_data bl = this->w->get_block (x, y, z);
		return {bl.id, bl.meta};
	}
}

