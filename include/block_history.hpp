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

#ifndef _hCraft__BLOCK_HISTORY_H_
#define _hCraft__BLOCK_HISTORY_H_

#include "blocks.hpp"
#include <vector>
#include <mutex>
#include <ctime>


namespace hCraft {

	class world;	
	class player;
	
	
	// 21~ bytes
	struct block_history_record
	{
		// pos
		int x;
		unsigned char y;
		int z;
		
		// block type
		blocki oldt;
		blocki newt;
		
		// player
		int pid;
		
		std::time_t tm;
	};
	
	
	using block_history = std::vector<block_history_record>;
	
	
	class block_history_manager
	{
		world &w;
		
		int cache_limit;
		std::vector<block_history_record> cache;
		std::mutex update_lock;
		
	private:
		void flush_nolock ();
		
	public:
		/* 
		 * Constructs a new block history manager for the specified server.
		 */
		block_history_manager (world &w, int cache_size = 120);
		
		/* 
		 * Calls flush ().
		 */
		~block_history_manager ();
		
		
		
		/* 
		 * Records block modification.
		 */
		void insert (int x, int y, int z, blocki oldt, blocki newt, player *pl);
		
		/* 
		 * Pushes all changes stored in cache to the SQL database.
		 */
		void flush ();
		
		
		
		/* 
		 * Gets all block modification records that have the given coordinates.
		 * The results are stored in @{out}.
		 */
		void get (int x, int y, int z, block_history& out);
	};
}

#endif

