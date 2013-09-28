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

#include "physics/blocks/shark.hpp"
#include "world.hpp"
#include "player.hpp"
#include "player_list.hpp"


namespace hCraft {
	
	namespace physics {
		
		static bool
		_is_water (unsigned short id)
		{
			switch (id)
				{
					case BT_STILL_WATER:
					case BT_WATER:
					case BT_ACTIVE_WATER:
						return true;
					
					default:
						return false;
				}
		}
		
		static int
		_try_block (world &w, int x, int y, int z)
		{
			int prev_id = w.get_final_block (x, y, z).id;
			if (!w.in_bounds (x, y, z) || !_is_water (prev_id))
				return -1;
			
			w.queue_update (x, y, z, BT_SHARK);
			return prev_id;
		}
		
		static int
		_sur_water (world &w, int x, int y, int z)
		{
			int id;
			
			if (_is_water (id = w.get_final_block (x - 1, y, z).id)) return id;
			if (_is_water (id = w.get_final_block (x + 1, y, z).id)) return id;
			if (_is_water (id = w.get_final_block (x, y, z - 1).id)) return id;
			if (_is_water (id = w.get_final_block (x, y, z + 1).id)) return id;
			
			return 0;
		}
		
		void
		shark::tick (world &w, int x, int y, int z, int data, void *ptr,
			std::minstd_rand& rnd)
		{
			if (y <= 0)
				{ w.queue_update (x, y, z, BT_AIR); return; }
			if (w.get_final_block (x, y, z).id != BT_SHARK)
				return;
		
			int below = w.get_final_block (x, y - 1, z).id;
			if (below == BT_AIR || (_is_water (below) && (w.get_final_block (x, y + 1, z).id == BT_AIR)))
				{
					w.queue_update (x, y, z, _sur_water (w, x, y, z));
					w.queue_update (x, y - 1, z, BT_SHARK);
				}
			else
				{
					int nx = x, ny = y, nz = z;
					
					// find closest player
					player *target = nullptr;
					w.get_players ().all (
						[x, y, z, &target] (player *pl)
							{
								double dx = x - pl->pos.x;
								double dy = y - pl->pos.y;
								double dz = z - pl->pos.z;
								double dm = dx*dx + dy*dy + dz*dz;
								if (dm > 6*6)
									return;
								
								if (!target)
									target = pl;
								else
									{
										double pdx = x - target->pos.x;
										double pdy = y - target->pos.y;
										double pdz = z - target->pos.z;
										
										if (dm < (pdx*pdx + pdy*pdy + pdz*pdz))
											target = pl;
									}
							});
					
					if (!target)
						{
							// move around in a random fashion
							std::uniform_int_distribution<> dis (0, 5);
							switch (dis (rnd))
								{
									case 0: -- ny; break;
									case 1: ++ ny; break;
							
									case 2: -- nz; break;
									case 3: ++ nz; break;
							
									case 4: -- nx; break;
									case 5: ++ nx; break;
								}
						}
					else
						{
							if (x < target->pos.x)
								{
									if (_is_water (w.get_final_block (x + 1, ny, nz).id))
										++ nx;
								}
							else if (x > target->pos.x)
								{
									if (_is_water (w.get_final_block (x - 1, ny, nz).id))
										-- nx;
								}
							if (y < target->pos.y)
								{
									if (_is_water (w.get_final_block (nx, y + 1, nz).id))
										++ ny;
								}
							else if (y > target->pos.y)
								{
									if (_is_water (w.get_final_block (nx, y - 1, nz).id))
										-- ny;
								}
							if (z < target->pos.z)
								{
									if (_is_water (w.get_final_block (nx, ny, z + 1).id))
										++ nz;
								}
							else if (z > target->pos.z)
								{
									if (_is_water (w.get_final_block (nx, ny, z - 1).id))
										-- nz;
								}
						}
					
					int prev = _try_block (w, nx, ny, nz);
					if (prev == -1)
						w.queue_physics (x, y, z, 0, nullptr, this->tick_rate ());
					else
						w.queue_update (x, y, z, prev, 0, 0, 0, nullptr, nullptr, false);
				}
		}
	}
}

