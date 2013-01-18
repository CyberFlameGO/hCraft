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

#ifndef _hCraft__WORLD_TRANSACTION_H_
#define _hCraft__WORLD_TRANSACTION_H_

#include "position.hpp"
#include <bitset>
#include <unordered_set>


namespace hCraft {
	
	class world; // forward dec
	class player;
	
	
	struct world_transaction_subchunk
	{
		unsigned char ids[4096];
		unsigned char add[2048];
		unsigned char meta[2048];
		int changes;
		
		world_transaction_subchunk ();
	};
	
	struct world_transaction_chunk
	{
		world_transaction_subchunk *subs[16];
		int changes;
		
		world_transaction_chunk ()
			{
				changes = 0;
				for (int i = 0; i < 16; ++i)
					subs[i] = nullptr;
			}
		
		~world_transaction_chunk ()
			{
				for (int i = 0; i < 16; ++i)
					if (subs[i])
						delete subs[i];
			}
	};
	
	
	/* 
	 * Transactions are used to modify a lot of blocks in a single step.
	 * Using transactions instead of modifying a world,block by block for large
	 * updates can be much faster, since transactions are far more optimized for
	 * these cases. For a small amount of modifications, block updates might be
	 * grouped together in a "Multi Block Change" packet (0x34) to reduce the
	 * amount of bytes sent. For larger updates, whole chunks will be re-sent.
	 */
	class world_transaction
	{
		world_transaction_chunk **chunks;
		
		int x_start, x_end;
		int z_start, z_end;
		int cwidth, cdepth;
		bool physics;
		
	public:
		/* 
		 * Constructs a new transaction to the modify the blocks within the given
		 * area.
		 */
		world_transaction (chunk_pos p1, chunk_pos p2, bool enq_physics = false);
		world_transaction (block_pos p1, block_pos p2, bool enq_physics = false);
		
		~world_transaction (); // destructor
		
		
		/* 
		 * Commit all changes made on the given world.
		 */
		void commit (world *wr);
		
		
		/* 
		 * Block modification:
		 */
		
		void set_id (int x, int y, int z, int id);
		void set_meta (int x, int y, int z, unsigned char meta);
		void set_id_and_meta (int x, int y, int z, int id, unsigned char meta);
		
		void clear_block (int x, int y, int z)
			{ set_id_and_meta (x, y, z, 0xFFF, 0); }
	};
}

#endif

