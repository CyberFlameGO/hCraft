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

#include "physics/langtons_ant.hpp"
#include "world.hpp"
#include "utils.hpp"


namespace hCraft {
	
	namespace physics {
		
		static inline bool
		queue_update_if_empty (world &w, int x, int y, int z, unsigned short id,
			unsigned char meta, int extra)
		{
			int prev_id = w.get_final_block (x, y, z).id;
			if (prev_id != BT_AIR)
				return false;
			
			w.queue_update (x, y, z, id, meta, extra);
			return true;
		}
		
		
		static inline int
		wool_from_color (int col)
		{
			static const int meta_table[] =
				{
					0, // white
					14, // red
					5, // green
					9, // cyan
					4, // yellow
					2, // pink
					7, // gray
					12, // brown\ ("dark red")
					13, // dark green
					11, // dark blue
				};
			
			return meta_table[col % 10];
		}
		
		static inline int
		color_from_wool (int wool)
		{
			static const int col_table[] =
				{ 0, 0, 5, 0, 4, 2, 6, 0, 0, 3, 0, 9, 7, 8, 1, 0};
			return col_table[wool & 0xF];
		}
		
		static inline int
		next_color (int col)
			{ return (col + 1) % 10; }
		
		enum rot_direction { RD_LEFT, RD_RIGHT };
		
		static inline rot_direction
		direction_from_color (int col)
		{
			static const rot_direction rot_table[] =
				{
					RD_RIGHT, // white
					RD_RIGHT, // red
					RD_LEFT,  // green
					RD_RIGHT, // cyan
					RD_LEFT,  // yellow
					RD_RIGHT, // pink
					RD_LEFT,  // gray
					RD_LEFT,  // dark red
					RD_RIGHT, // dark green
					RD_LEFT,  // dark blue
				};
			return rot_table[col % 10];
		}
		
		static inline int
		rotate (int curr, rot_direction dir)
		{
			int add = (dir == RD_RIGHT) ? 1 : (-1);
			return utils::mod (curr + add, 4);
		}
		
		
		void
		langtons_ant::tick (world &w, int x, int y, int z, int extra, void *ptr)
		{
			if (y == 0)
				{ w.queue_update (x, y, z, BT_AIR); return; }
			int block_below = w.get_final_block (x, y - 1, z).id;
			if (block_below != BT_WOOL && block_below != BT_GRASS)
				{ w.queue_update (x, y, z, BT_AIR); return; }
			
			int col_below = color_from_wool (w.get_meta (x, y - 1, z));
			int next_col  = next_color (col_below);
			
			w.queue_update (x, y - 1, z, BT_WOOL, wool_from_color (next_col));
			w.queue_update (x, y, z, BT_AIR);
			
			int next_dir = rotate (extra, direction_from_color (next_col));
			switch (next_dir)
				{
					case 0: queue_update_if_empty (w, x + 1, y, z, BT_MOSSY_COBBLE, 0, (int)next_dir); break;
					case 1: queue_update_if_empty (w, x, y, z + 1, BT_MOSSY_COBBLE, 0, (int)next_dir); break;
					case 2: queue_update_if_empty (w, x - 1, y, z, BT_MOSSY_COBBLE, 0, (int)next_dir); break;
					case 3: queue_update_if_empty (w, x, y, z - 1, BT_MOSSY_COBBLE, 0, (int)next_dir); break;
				}
		}
	}
}

