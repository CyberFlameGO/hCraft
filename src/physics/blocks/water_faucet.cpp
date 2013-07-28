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

#include "physics/blocks/water_faucet.hpp"
#include "world.hpp"
#include "utils.hpp"
#include "position.hpp"
#include <random>


namespace hCraft {
	
	namespace physics {
		
		void
		water_faucet::tick (world &w, int x, int y, int z, int data,
			void *ptr, std::minstd_rand& rnd)
		{
			if (w.get_id (x, y, z) != 2005)
				return;
			
			block_pos pos_list[5];
			pos_list[0] = block_pos { x, y + 1, z };
			pos_list[1] = block_pos { x - 1, y, z };
			pos_list[2] = block_pos { x + 1, y, z };
			pos_list[3] = block_pos { x, y, z - 1 };
			pos_list[4] = block_pos { x, y, z + 1 };
			
			int ind_list[5];
			for (int i = 0; i < 5; ++i)
				ind_list[i] = i;
			for (int i = 4; i >= 0; --i)
				{
					std::uniform_int_distribution<> dis (0, i);
					int k = dis (rnd);
					int t = ind_list[k];
					ind_list[k] = ind_list[i];
					ind_list[i] = t;
				}
			
			for (int i = 0; i < 5; ++i)
				{
					block_pos pos = pos_list[ind_list[i]];
					if (w.get_id (pos.x, pos.y, pos.z) == BT_AIR)
						{
							w.queue_update (pos.x, pos.y, pos.z, BT_STILL_WATER);
							
							physics_params params;
							physics_params::build ("finite 1", params, -1);
							w.queue_physics (pos.x, pos.y, pos.z, 0, nullptr, 2, &params, nullptr);
							break;
						}
				}
			
			w.queue_physics (x, y, z, 0, nullptr, this->tick_rate ());
		}
	}
}

