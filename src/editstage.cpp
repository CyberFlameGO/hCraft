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

#include "editstage.hpp"
#include "world.hpp"
#include "player.hpp"
#include <cstring>


namespace hCraft {
	
	void
	edit_stage::preview_to (player *pl)
	{
		std::vector<player *> vec (1, pl);
		this->preview (vec);
	}
	
	void
	edit_stage::restore_to (player *pl)
	{
		std::vector<player *> vec (1, pl);
		this->restore (vec);
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
	}
	
	des_chunk::~des_chunk ()
	{
		for (int i = 0; i < 16; ++i)
			delete this->subs[i];
	}
	
	
	
//----
	
	dense_edit_stage::dense_edit_stage (world &w)
		: w (w)
		{ }
	
	
	
	/* 
	 * Block modification \ retreival:
	 */
	
	void
	dense_edit_stage::set (int x, int y, int z, unsigned short id, unsigned char meta)
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
		micro->data[b_index] = (id << 4) | (meta & 0xF);
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
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		else
			ch = &itr->second;
		
		int sy = y >> 4;
		des_subchunk *sub = ch->subs[sy];
		if (!sub)
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		int bx = x & 0xF;
		int by = y & 0xF;
		int bz = z & 0xF;
		int m_index = ((by >> 3) << 2) | ((bz >> 3) << 1) | ((bx >> 3));
		des_microchunk *micro = sub->micro[m_index];
		if (!micro)
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		int b_index = ((y & 0x7) << 6) | ((z & 0x7) << 3) | ((x & 0x7));
		unsigned short val = micro->data[b_index];
		if (val == 0xFFFF)
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		return {(unsigned short)(val >> 4), (unsigned char)(val & 0xF)};
	}
	
	
	
	void
	dense_edit_stage::send_to_players (std::vector<player *>& _players, bool restore)
	{
		std::vector<player *> players (_players);
		if (_players.empty ())
			goto done_sending;
		
		// don't send to players that are too far away
		for (auto itr = players.begin (); itr != players.end (); )
			{
				bool found_common_chunk = false;
				player *pl = *itr;
				
				for (auto chitr = this->chunks.begin (); chitr != this->chunks.end (); ++chitr)
					if (pl->can_see_chunk (chitr->first.x, chitr->first.z))
						{ found_common_chunk = true; break; }
				
				if (found_common_chunk)
					++ itr;
				else
					itr = players.erase (itr);
			}
		
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			{
				chunk_pos cp = itr->first;
				des_chunk& ch = itr->second;
				
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
										if (id != 0xFFF)
											{
												bx = ((mi & 1) << 3) | (bi & 0x7);
												by = (sy << 4) | (((mi >> 2) & 1) << 3) | ((bi >> 6) & 0x7);
												bz = (((mi >> 1) & 1) << 3) | ((bi >> 3) & 0x7);
												
												if (restore)
													{
														bd = this->w.get_block ((itr->first.x << 4) | bx, by, (itr->first.z << 4) | bz);
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
				
				if (players.size () == 1)
					{
						players[0]->send
							(packet::make_multi_block_change (cp.x, cp.z, records));
					}
				else
					{
						packet *pack = packet::make_multi_block_change (cp.x, cp.z, records);
						for (player *pl : players)
							pl->send (new packet (*pack));
						delete pack;
					}
			}
		
	done_sending:
		;
	}
	
	
	
	/* 
	 * Sends all modified blocks to the specified player(s).
	 */
	void
	dense_edit_stage::preview (std::vector<player *>& players)
	{
		this->send_to_players (players, false);
	}
	
	
	
	/* 
	 * Restores back all block modifications sent by preview().
	 */
	void
	dense_edit_stage::restore (std::vector<player *>& players)
	{
		this->send_to_players (players, true);
	}
	
	
	
	/* 
	 * Clears the edit stage.
	 */
	void
	dense_edit_stage::reset ()
	{
		this->chunks.clear ();
	}
	
	
	
//------------------------------------------------------------------------------
	
	sparse_edit_stage::sparse_edit_stage (world &w)
		: w (w)
		{ }
	
	
	
	/* 
	 * Block modification \ retrieval:
	 */
	void
	sparse_edit_stage::set (int x, int y, int z, unsigned short id, unsigned char meta)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		unsigned char bx = x & 15;
		unsigned char bz = z & 15;
		
		ses_chunk& ch = this->chunks[{cx, cz}];
		ch.changes[{bx, y, bz}] = (id << 4) | (meta & 0xF);
	}
	
	
	blocki
	sparse_edit_stage::get (int x, int y, int z)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		
		auto itr = this->chunks.find ({cx, cz});
		if (itr == this->chunks.end ())
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		unsigned char bx = x & 15;
		unsigned char bz = z & 15;
		
		ses_chunk& ch = itr->second;
		auto bitr = ch.changes.find ({bx, y, bz});
		if (bitr == ch.changes.end ())
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		return {(unsigned short)((bitr->second) >> 4), (unsigned char)((bitr->second) & 0xF)};
	}
	
	
	
	void
	sparse_edit_stage::send_to_players (std::vector<player *>& _players, bool restore)
	{
		std::vector<player *> players (_players);
		if (_players.empty ())
			goto done_sending;
		
		// don't send to players that are too far away
		for (auto itr = players.begin (); itr != players.end (); )
			{
				bool found_common_chunk = false;
				player *pl = *itr;
				
				for (auto chitr = this->chunks.begin (); chitr != this->chunks.end (); ++chitr)
					if (pl->can_see_chunk (chitr->first.x, chitr->first.z))
						{ found_common_chunk = true; break; }
				
				if (found_common_chunk)
					++ itr;
				else
					itr = players.erase (itr);
			}
		
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			{
				chunk_pos cp = itr->first;
				ses_chunk& ch = itr->second;
				
				std::vector<block_change_record> records;
				for (auto bitr = ch.changes.begin (); bitr != ch.changes.end (); ++bitr)
					{
						unsigned char x = bitr->first.x;
						unsigned char y = bitr->first.y;
						unsigned char z = bitr->first.z;
						
						if (restore)
							{
								block_data bd = this->w.get_block (x, y, z);
								records.push_back ({x, y, z, bd.id, bd.meta});
							}
						else
							records.push_back ({x, y, z,
								(unsigned short)((bitr->second) >> 4), (unsigned char)((bitr->second) & 0xF)});
					}
				
				if (players.size () == 1)
					{
						players[0]->send
							(packet::make_multi_block_change (cp.x, cp.z, records));
					}
				else
					{
						packet *pack = packet::make_multi_block_change (cp.x, cp.z, records);
						for (player *pl : players)
							pl->send (new packet (*pack));
						delete pack;
					}
			}
		
	done_sending:
		;
	}
	
	
	
	/* 
	 * Sends all modified blocks to the specified player(s).
	 */
	void
	sparse_edit_stage::preview (std::vector<player *>& players)
	{
		this->send_to_players (players, false);
	}
	
	
	
	/* 
	 * Restores back all block modifications sent by preview().
	 */
	void
	sparse_edit_stage::restore (std::vector<player *>& players)
	{
		this->send_to_players (players, true);
	}
	
	
	
	/* 
	 * Clears the edit stage.
	 */
	void
	sparse_edit_stage::reset ()
	{
		this->chunks.clear ();
	}
}

