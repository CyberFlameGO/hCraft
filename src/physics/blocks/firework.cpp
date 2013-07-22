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

#include "physics/blocks/firework.hpp"
#include "world.hpp"
#include "utils.hpp"
#include <random>


namespace hCraft {
	
	namespace physics {
		
		static void
		_remove_rocket (world &w, int x, int y, int z)
		{
			if (w.get_id (x, y, z) != 2004)
				return;
			
			w.queue_update (x, y, z, BT_AIR);
			if (w.get_id (x, y - 1, z) == BT_STILL_LAVA)
				{
					w.queue_update (x, y - 1, z, BT_AIR);
					if (w.get_id (x, y - 2, z) == BT_STILL_LAVA)
						w.queue_update (x, y - 2, z, BT_AIR);
				}
		}
		
		
		static void
		_explode (world &w, int x, int y, int z, std::minstd_rand& rnd)
		{
			std::uniform_int_distribution<> chance_dis (1, 40);
			
			std::uniform_int_distribution<> wool_dis (0, 15);
			
			int col1 = wool_dis (rnd);
			int col2 = wool_dis (rnd);
			std::uniform_int_distribution<> col_dis (std::min (col1, col2), std::max (col1, col2));
			
			physics_params particle_params;
			physics_params::build ("drop 100 dissipate 25", particle_params);
			
			int rad = 5;
			for (int xx = -rad; xx <= rad; ++xx)
				for (int yy = -rad; yy <= rad; ++yy)
					for (int zz = -rad; zz <= rad; ++zz)
						if ((xx*xx + yy*yy + zz*zz) <= (rad*rad))
							{
								if (chance_dis (rnd) < 2 && w.get_id (xx + x, yy + y, zz + z) == BT_AIR)
									{
										w.queue_update (xx + x, yy + y, zz + z, BT_WOOL, col_dis (rnd));
										w.queue_physics (xx + x, yy + y, zz + z, 0, nullptr, 2, &particle_params, nullptr);
									}
							}
		}
		
		void
		firework::on_break_attempt (world &w, int x, int y, int z)
		{
			w.queue_update_nolock (x, y, z, this->id ());
			
			// spawn rocket
			w.queue_update_nolock (x, y + 1, z, 2004);
		}
		
		
		
		void
		firework_rocket::tick (world &w, int x, int y, int z, int extra,
			void *ptr, std::minstd_rand& rnd)
		{
			if (w.get_id (x, y, z) != 2004)
				return;
				
			if (extra == 20 || (w.get_id (x, y + 1, z) != BT_AIR))
				{
					_remove_rocket (w, x, y, z);
					_explode (w, x, y, z, rnd);
					return;
				}
			
			w.queue_update (x, y, z, BT_STILL_LAVA);
			if (w.get_id (x, y - 1, z) == BT_AIR)
				w.queue_update (x, y - 1, z, BT_STILL_LAVA);
			if (w.get_id (x, y - 2, z) == BT_STILL_LAVA)
				w.queue_update (x, y - 2, z, BT_AIR);
			
			w.queue_update (x, y + 1, z, this->id (), 0, extra + 1);
		}
		
		void
		firework_rocket::on_modified (world &w, int x, int y, int z)
		{
			if (w.get_id (x, y, z) != BT_STILL_LAVA)
				{
					if (w.get_id (x, y - 1, z) == BT_STILL_LAVA)
						w.queue_update_nolock (x, y - 1, z, BT_AIR);
					if (w.get_id (x, y - 2, z) == BT_STILL_LAVA)
						w.queue_update_nolock (x, y - 2, z, BT_AIR);
				}
		}
	}
}

