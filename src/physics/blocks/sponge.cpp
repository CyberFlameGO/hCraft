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

#include "physics/blocks/sponge.hpp"
#include "world/world.hpp"


namespace hCraft {
	
	namespace physics {
		
		static bool
		is_water_block (unsigned short id)
		{
			return (id == 8 || id == 9 || id == 2000);
		}
		
		
		
		void
		water_sponge::tick (world &w, int x, int y, int z, int data, void *ptr,
			std::minstd_rand& rnd)
		{
			if (w.get_final_block (x, y, z).id != 2001)
				return;
			
			// spawn the initial agents
			
			if (y < 255 && is_water_block (w.get_final_block (x, y + 1, z).id))
				w.queue_update (x, y + 1, z, 2002);
			if (y > 0   && is_water_block (w.get_final_block (x, y - 1, z).id))
				w.queue_update (x, y - 1, z, 2002);
			if (is_water_block (w.get_final_block (x - 1, y, z).id))
				w.queue_update (x - 1, y, z, 2002);
			if (is_water_block (w.get_final_block (x + 1, y, z).id))
				w.queue_update (x + 1, y, z, 2002);
			if (is_water_block (w.get_final_block (x, y, z - 1).id))
				w.queue_update (x, y, z - 1, 2002);
			if (is_water_block (w.get_final_block (x, y, z + 1).id))
				w.queue_update (x, y, z + 1, 2002);
		}
		
		void
		water_sponge_agent::tick (world &w, int x, int y, int z, int data, void *ptr,
			std::minstd_rand& rnd)
		{
			if (w.get_final_block (x, y, z).id != 2002)
				return;
			
			// spawn agents
			if (y < 255 && is_water_block (w.get_final_block (x, y + 1, z).id))
				w.queue_update (x, y + 1, z, 2002);
			if (y > 0   && is_water_block (w.get_final_block (x, y - 1, z).id))
				w.queue_update (x, y - 1, z, 2002);
			if (is_water_block (w.get_final_block (x - 1, y, z).id))
				w.queue_update (x - 1, y, z, 2002);
			if (is_water_block (w.get_final_block (x + 1, y, z).id))
				w.queue_update (x + 1, y, z, 2002);
			if (is_water_block (w.get_final_block (x, y, z - 1).id))
				w.queue_update (x, y, z - 1, 2002);
			if (is_water_block (w.get_final_block (x, y, z + 1).id))
				w.queue_update (x, y, z + 1, 2002);
			
			// replace self with air
			w.queue_update (x, y, z, 0);
		}
	}
}

