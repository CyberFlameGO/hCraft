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

#include "generation/detail/trees.hpp"
#include "world.hpp"
#include "chunk.hpp"


namespace hCraft {
	namespace dgen {
		
		generic_trees::generic_trees (int min_height, blocki bl_trunk, blocki bl_leaves)
			: bl_trunk (bl_trunk), bl_leaves (bl_leaves)
		{
			this->min_height = min_height; 
			if (this->min_height < 1)
				this->min_height = 1;
		}
		
		
		
		void
		generic_trees::seed (long s)
		{
			this->rnd.seed (s);
		}
		
		void
		generic_trees::generate (world &wr, int x, int y, int z)
		{
			auto& rnd = this->rnd;
			std::uniform_int_distribution<> dis1 (0, 2), dis2 (0, 20);
			
			chunk_link_map map (wr, wr.get_chunk_at (x, z), x >> 4, z >> 4);
			
			int h = this->min_height + dis1 (rnd);
			int base = y;
		 	int tip  = y + h - 1;
			if (base < 1 || tip > 255) 
				return;
		
			// leaves
			for (int xx = (x - 1); xx <= (x + 1); ++xx)
				for (int zz = (z - 1); zz <= (z + 1); ++zz)
					for (int yy = tip; yy <= (tip + 1); ++yy)
						if (dis2 (rnd) != 0)
							map.set (xx, yy, zz, this->bl_leaves.id, this->bl_leaves.meta);
			for (int xx = (x - 2); xx <= (x + 2); ++xx)
				for (int zz = (z - 2); zz <= (z + 2); ++zz)
					for (int yy = (tip - 2); yy <= (tip - 1); ++yy)
						if (dis2 (rnd) != 0)
							map.set (xx, yy, zz, this->bl_leaves.id, this->bl_leaves.meta);
			
			// trunk
			for (int i = base; i <= tip; ++i)
				map.set (x, i, z, this->bl_trunk.id, this->bl_trunk.meta);

		}
		
		
		
//------
		
		palm_trees::palm_trees (int min_height, blocki bl_trunk, blocki bl_leaves)
			: bl_trunk (bl_trunk), bl_leaves (bl_leaves)
		{
			this->min_height = min_height; 
			if (this->min_height < 1)
				this->min_height = 1;
		}
		
		
		
		void
		palm_trees::seed (long s)
		{
			this->rnd.seed (s);
		}
		
		void
		palm_trees::generate (world &wr, int x, int y, int z)
		{
			auto& rnd = this->rnd;
			std::uniform_int_distribution<> dis1 (0, 2), dis2 (0, 20);
			
			chunk_link_map map (wr, wr.get_chunk_at (x, z), x >> 4, z >> 4);
			
			int h = this->min_height + dis1 (rnd);
			int base = y;
		 	int tip  = y + h - 1;
			if (base < 1 || tip > 255) 
				return;
		
			// leaves
			// TODO
			
			// trunk
			for (int i = base; i <= tip; ++i)
				map.set (x, i, z, this->bl_trunk.id, this->bl_trunk.meta);

		}
	}
}

