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

#include "physics/sand.hpp"
#include "world.hpp"


namespace hCraft {
	
	namespace physics {
		
		void
		sand::tick (world &w, int x, int y, int z, int extra, void *ptr)
		{
		/*
			if (y == 0)
				{ w.queue_update_nolock (x, y, z, BT_AIR); return; }
			
			if (w.get_id (x, y - 1, z) == BT_AIR)
				{
					w.queue_update_nolock (x, y - 1, z, BT_SAND);
					w.queue_update_nolock (x, y, z, BT_AIR);
				}
			else
				{
					if (w.get_id (x - 1, y - 1, z) == BT_AIR && w.get_id (x - 1, y, z) == BT_AIR)
						{
							w.queue_update_nolock (x - 1, y - 1, z, BT_SAND);
							w.queue_update_nolock (x, y, z, BT_AIR);
						}
					else if (w.get_id (x + 1, y - 1, z) == BT_AIR && w.get_id (x + 1, y, z) == BT_AIR)
						{
							w.queue_update_nolock (x + 1, y - 1, z, BT_SAND);
							w.queue_update_nolock (x, y, z, BT_AIR);
						}
					else if (w.get_id (x, y - 1, z - 1) == BT_AIR && w.get_id (x, y, z - 1) == BT_AIR)
						{
							w.queue_update_nolock (x, y - 1, z - 1, BT_SAND);
							w.queue_update_nolock (x, y, z, BT_AIR);
						}
					else if (w.get_id (x, y - 1, z + 1) == BT_AIR && w.get_id (x, y, z + 1) == BT_AIR)
						{
							w.queue_update_nolock (x, y - 1, z + 1, BT_SAND);
							w.queue_update_nolock (x, y, z, BT_AIR);
						}
				}*/
			
		if (y == 0)
			{ w.queue_update (x, y, z, BT_AIR); return; }
		
		if (w.get_id (x, y - 1, z) == BT_AIR)
			{
				w.queue_update (x, y - 1, z, BT_SAND);
				w.queue_update (x, y, z, BT_AIR);
			}
		else
			{
				if (w.get_id (x - 1, y - 1, z) == BT_AIR && w.get_id (x - 1, y, z) == BT_AIR)
					{
						w.queue_update (x - 1, y - 1, z, BT_SAND);
						w.queue_update (x, y, z, BT_AIR);
					}
				else if (w.get_id (x + 1, y - 1, z) == BT_AIR && w.get_id (x + 1, y, z) == BT_AIR)
					{
						w.queue_update (x + 1, y - 1, z, BT_SAND);
						w.queue_update (x, y, z, BT_AIR);
					}
				else if (w.get_id (x, y - 1, z - 1) == BT_AIR && w.get_id (x, y, z - 1) == BT_AIR)
					{
						w.queue_update (x, y - 1, z - 1, BT_SAND);
						w.queue_update (x, y, z, BT_AIR);
					}
				else if (w.get_id (x, y - 1, z + 1) == BT_AIR && w.get_id (x, y, z + 1) == BT_AIR)
					{
						w.queue_update (x, y - 1, z + 1, BT_SAND);
						w.queue_update (x, y, z, BT_AIR);
					}
			}
		}
		
		void
		sand::on_neighbour_modified (world &w, int x, int y, int z,
			int nx, int ny, int nz)
		{
			w.queue_physics_once (x, y, z);
		}
	}
}

