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

#ifndef _hCraft__PLAYER_TRANSACTION_H_
#define _hCraft__PLAYER_TRANSACTION_H_

#include <vector>
#include <unordered_map>
#include <vector>
#include "position.hpp"


namespace hCraft {
	
	// forward dec
	class player;
	
	
	struct pl_tr_change
		{ unsigned char x, y, z; unsigned short id; unsigned char meta; };
	
	struct pl_tr_chunk
	{
		std::vector<pl_tr_change> changes;
	};
	
	/* 
	 * Very similar to a world transaction, but does not actually modify any
	 * blocks in the underlying world. Blocks updates are grouped into multi-
	 * block-change packets to improve efficiency, and are sent only the selected
	 * player.
	 * 
	 * Since no blocks are modified, the way the update are stores is a bit
	 * different that how it's done in a world transaction.
	 */
	class player_transaction
	{
		std::unordered_map<chunk_pos, pl_tr_chunk, chunk_pos_hash> chunks;
		//std::mutex lock;
		
	public:
		/* 
		 * Inserts a block update.
		 */
		void insert (int x, int y, int z, unsigned short id, unsigned char meta = 0);
		
		
		/* 
		 * Sends all block updates at once to the specified players.
		 * All updates will then be cleared if @{clear} is true.
		 */
		void commit (std::vector<player *>& players, bool clear = false);
		void commit (player *pl, bool clear = false);
		
		/* 
		 * Clear all block updates.
		 */
		void clear ();
	};
}

#endif

