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

#include "player_transaction.hpp"
#include "player.hpp"
#include "packet.hpp"


namespace hCraft {
	
	/* 
	 * Inserts a block update.
	 */
	void
	player_transaction::insert (int x, int y, int z, unsigned short id, unsigned char meta)
	{
		//std::lock_guard<std::mutex> guard ((this->lock));
		int cx = x >> 4;
		int cz = z >> 4;
		unsigned char bx = x & 15;
		unsigned char bz = z & 15;
		
		pl_tr_chunk& ch = this->chunks[chunk_pos (cx, cz)];
		ch.changes.push_back ({bx, (unsigned char)y, bz, id, meta});
	}
	
	
	
	/* 
	 * Sends all block updates at once to the specified players.
	 * All updates will then be cleared if @{clear} is true.
	 */
	void
	player_transaction::commit (std::vector<player *>& players, bool clear)
	{
		if (players.empty ())
			goto done_sending;
		
		//std::lock_guard<std::mutex> guard ((this->lock));
		for (auto itr = this->chunks.begin (); itr != this->chunks.end (); ++itr)
			{
				chunk_pos cp = itr->first;
				pl_tr_chunk& ch = itr->second;
				
				std::vector<block_change_record> records;
				for (pl_tr_change c : ch.changes)
					records.push_back ({c.x, c.y, c.z, c.id, c.meta});
				
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
		if (clear)
			this->clear ();
	}
	
	void
	player_transaction::commit (player *pl, bool clear)
	{
		std::vector<player *> vec (1, pl);
		this->commit (vec, clear);
	}
	
	/* 
	 * Clear all block updates.
	 */
	void
	player_transaction::clear ()
	{
		//std::lock_guard<std::mutex> guard ((this->lock));
		this->chunks.clear ();
	}
}

