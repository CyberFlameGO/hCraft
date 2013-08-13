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

#include "physics/blocks/activewater.hpp"
#include "world.hpp"


namespace hCraft {
	
	namespace physics {
		
		void
		active_water::tick (world &w, int x, int y, int z, int data, void *ptr,
			std::minstd_rand& rnd)
		{
			if (w.get_final_block (x, y, z).id != this->id ())
				return;
			
			//w.queue_update (x, y, z, BT_STILL_WATER);
			
			if (y > 0 && w.get_final_block (x, y - 1, z).id == BT_AIR)
				w.queue_update (x, y - 1, z, this->id ());
			if (w.get_final_block (x - 1, y, z).id == BT_AIR)
				w.queue_update (x - 1, y, z, this->id ());
			if (w.get_final_block (x + 1, y, z).id == BT_AIR)
				w.queue_update (x + 1, y, z, this->id ());
			if (w.get_final_block (x, y, z - 1).id == BT_AIR)
				w.queue_update (x, y, z - 1, this->id ());
			if (w.get_final_block (x, y, z + 1).id == BT_AIR)
				w.queue_update (x, y, z + 1, this->id ());
		}
	}
}

