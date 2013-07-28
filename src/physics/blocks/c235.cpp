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

#include "physics/blocks/c235.hpp"
#include "world.hpp"

#include <iostream> // DEBUG


namespace hCraft {
	
	namespace physics {
		
		static int
		_get_neighbour_count (world &w, int x, int y, int z)
		{
			int c = 0;
			
			if (w.get_id (x - 1, y, z) == 45) ++ c;
			if (w.get_id (x + 1, y, z) == 45) ++ c;
			if (w.get_id (x, y - 1, z) == 45) ++ c;
			if (w.get_id (x, y + 1, z) == 45) ++ c;
			if (w.get_id (x, y, z - 1) == 45) ++ c;
			if (w.get_id (x, y, z + 1) == 45) ++ c;
			
			if (w.get_id (x - 1, y - 1, z - 1) == 45) ++ c;
			if (w.get_id (x + 1, y - 1, z - 1) == 45) ++ c;
			if (w.get_id (x - 1, y - 1, z + 1) == 45) ++ c;
			if (w.get_id (x + 1, y - 1, z + 1) == 45) ++ c;
			
			if (w.get_id (x - 1, y, z - 1) == 45) ++ c;
			if (w.get_id (x + 1, y, z - 1) == 45) ++ c;
			if (w.get_id (x - 1, y, z + 1) == 45) ++ c;
			if (w.get_id (x + 1, y, z + 1) == 45) ++ c;
			
			if (w.get_id (x - 1, y + 1, z - 1) == 45) ++ c;
			if (w.get_id (x + 1, y + 1, z - 1) == 45) ++ c;
			if (w.get_id (x - 1, y + 1, z + 1) == 45) ++ c;
			if (w.get_id (x + 1, y + 1, z + 1) == 45) ++ c;
			
			return c;
		}
		
		/*
		static void
		_air_agent (world &w, int x, int y, int z, int data, std::minstd_rand& rnd)
		{
			
		}
		*/
		
		void
		c235::tick (world &w, int x, int y, int z, int data, void *ptr,
			std::minstd_rand& rnd)
		{
			if (w.get_id (x, y, z) != 45)
				return;
			
			int nc = _get_neighbour_count (w, x, y, z);
			if (nc <= 1 || nc == 4 || nc > 5)
				{
					// die
					w.queue_update (x, y, z, BT_AIR);
				}
			else if (nc == 2 || nc == 3)
				{
					// survive
					w.queue_physics (x, y, z, 0, nullptr, this->tick_rate ());
				}
			
			
		//---
			// check adjacent blocks
			if (w.get_id (x - 1, y, z) == 0 && _get_neighbour_count (w, x - 1, y, z) == 5)
				w.queue_update (x - 1, y, z, 45);
			if (w.get_id (x + 1, y, z) == 0 && _get_neighbour_count (w, x + 1, y, z) == 5)
				w.queue_update (x + 1, y, z, 45);
			if (w.get_id (x, y - 1, z) == 0 && _get_neighbour_count (w, x, y - 1, z) == 5)
				w.queue_update (x, y - 1, z, 45);
			if (w.get_id (x, y + 1, z) == 0 && _get_neighbour_count (w, x, y + 1, z) == 5)
				w.queue_update (x, y + 1, z, 45);
			if (w.get_id (x, y, z - 1) == 0 && _get_neighbour_count (w, x, y, z - 1) == 5)
				w.queue_update (x, y, z - 1, 45);
			if (w.get_id (x, y, z + 1) == 0 && _get_neighbour_count (w, x, y, z + 1) == 5)
				w.queue_update (x, y, z + 1, 45);
			
			if (w.get_id (x - 1, y - 1, z - 1) == 0 && _get_neighbour_count (w, x - 1, y - 1, z - 1) == 5)
				w.queue_update (x - 1, y - 1, z - 1, 45);
			if (w.get_id (x + 1, y - 1, z - 1) == 0 && _get_neighbour_count (w, x + 1, y - 1, z - 1) == 5)
				w.queue_update (x + 1, y - 1, z - 1, 45);
			if (w.get_id (x - 1, y - 1, z + 1) == 0 && _get_neighbour_count (w, x - 1, y - 1, z + 1) == 5)
				w.queue_update (x - 1, y - 1, z + 1, 45);
			if (w.get_id (x + 1, y - 1, z + 1) == 0 && _get_neighbour_count (w, x + 1, y - 1, z + 1) == 5)
				w.queue_update (x + 1, y - 1, z + 1, 45);
			
			if (w.get_id (x - 1, y, z - 1) == 0 && _get_neighbour_count (w, x - 1, y, z - 1) == 5)
				w.queue_update (x - 1, y, z - 1, 45);
			if (w.get_id (x + 1, y, z - 1) == 0 && _get_neighbour_count (w, x + 1, y, z - 1) == 5)
				w.queue_update (x + 1, y, z - 1, 45);
			if (w.get_id (x - 1, y, z + 1) == 0 && _get_neighbour_count (w, x - 1, y, z + 1) == 5)
				w.queue_update (x - 1, y, z + 1, 45);
			if (w.get_id (x + 1, y, z + 1) == 0 && _get_neighbour_count (w, x + 1, y, z + 1) == 5)
				w.queue_update (x + 1, y, z + 1, 45);
			
			if (w.get_id (x - 1, y + 1, z - 1) == 0 && _get_neighbour_count (w, x - 1, y + 1, z - 1) == 5)
				w.queue_update (x - 1, y + 1, z - 1, 45);
			if (w.get_id (x + 1, y + 1, z - 1) == 0 && _get_neighbour_count (w, x + 1, y + 1, z - 1) == 5)
				w.queue_update (x + 1, y + 1, z - 1, 45);
			if (w.get_id (x - 1, y + 1, z + 1) == 0 && _get_neighbour_count (w, x - 1, y + 1, z + 1) == 5)
				w.queue_update (x - 1, y + 1, z + 1, 45);
			if (w.get_id (x + 1, y + 1, z + 1) == 0 && _get_neighbour_count (w, x + 1, y + 1, z + 1) == 5)
				w.queue_update (x + 1, y + 1, z + 1, 45);
		}
		
		void
		c235::on_neighbour_modified (world &w, int x, int y, int z,
			int nx, int ny, int nz)
		{
			w.queue_physics_once (x, y, z, 0, nullptr, this->tick_rate ());
		}
	}
}

