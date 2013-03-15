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

#include "physics/water.hpp"
#include "world.hpp"
#include "blocks.hpp"


namespace hCraft {
	
	namespace physics {
		
		static bool
		can_be_placed_at (world& w, int x, int y, int z, int lv)
		{
			block_data bd = w.get_block (x, y, z);
			block_info *binf = block_info::from_id (bd.id);
			if (!binf->opaque)
				return true;
			
			if (bd.id == BT_WATER)
				{
					if ((lv & 8) == 8) return true;
					if (bd.meta >= lv)
						return true;
				}
			return false;
		}
		
		void
		water::tick (world &w, int x, int y, int z, int extra, void *ptr, block_physics_worker& worker)
		{
			block_data bd = w.get_block (x, y, z);
			if (bd.id != BT_WATER)
				return;
			
			unsigned char lv = bd.meta;
			if (lv > 8)
				lv = 0;
			
			if (can_be_placed_at (w, x, y - 1, z, 8 | lv))
				w.queue_update (x, y - 1, z, BT_WATER, 8 | lv);
			else if ((lv & 7) != 7)
				{
					unsigned char next_lv = (lv & 7) + 1;
					if (can_be_placed_at (w, x + 1, y, z, next_lv))
						w.queue_update (x + 1, y, z, BT_WATER, next_lv);
					if (can_be_placed_at (w, x - 1, y, z, next_lv))
						w.queue_update (x - 1, y, z, BT_WATER, next_lv);
					if (can_be_placed_at (w, x, y, z + 1, next_lv))
						w.queue_update (x, y, z + 1, BT_WATER, next_lv);
					if (can_be_placed_at (w, x, y, z - 1, next_lv))
						w.queue_update (x, y, z - 1, BT_WATER, next_lv);
				}
		}
	}
}

