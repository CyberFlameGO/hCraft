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

#include "physics/snow.hpp"
#include "world.hpp"


namespace hCraft {
	
	namespace physics {
		
		void
		snow::tick (world &w, int x, int y, int z, int extra, void *ptr,
			std::minstd_rand& rnd)
		{
			if (y <= 0)
				{ w.queue_update (x, y, z, BT_AIR); return; }
			if (w.get_final_block (x, y, z).id != BT_SNOW_BLOCK)
				return;
			
			int below = w.get_final_block (x, y - 1, z).id;
			if (w.get_final_block (x, y - 1, z).id != BT_AIR)
				{
					if (below == BT_SNOW_BLOCK || below == BT_SNOW_COVER)
						w.queue_update (x, y, z, BT_AIR);
					else
						w.queue_update (x, y, z, BT_SNOW_COVER);
				}
			else
				{
					w.queue_update (x, y, z, BT_AIR);
					
					int nx = x, nz = z;
					
					std::uniform_int_distribution<> dis (1, 4);
					int d = dis (rnd);
					switch (d)
						{
							case 1: ++ nx; break;
							case 2: -- nx; break;
							case 3: ++ nz; break;
							case 4: -- nz; break;
						}
					
					if (w.get_final_block (nx, y - 1, nz) == BT_AIR)
						w.queue_update (nx, y - 1, nz, BT_SNOW_BLOCK);
					else
						w.queue_update (x, y - 1, z, BT_SNOW_BLOCK);
				}
		}
	}
}

