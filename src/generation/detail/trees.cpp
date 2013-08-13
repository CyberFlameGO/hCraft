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
		
		
		static inline void
		_palm_advance (int& x, int& z, int dir)
		{
			switch (dir)
				{
					case 0: -- x; break;
					case 1: ++ x; break;
					case 2: -- z; break;
					case 3: ++ z; break;
				}
		}
		
		static void
		_palm_leaves (chunk_link_map& map, int x, int y, int z, int id, int meta, int dir)
		{
			++ y;
			map.set (x, y, z, id, meta);
			_palm_advance (x, z, dir);
			
			map.set (x, y, z, id, meta);
			_palm_advance (x, z, dir);
			
			-- y;
			map.set (x, y, z, id, meta);
			_palm_advance (x, z, dir);
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
			map.set (x, tip + 1, z, this->bl_leaves.id, this->bl_leaves.meta);
			_palm_leaves (map, x - 1, tip - 1, z, this->bl_leaves.id, this->bl_leaves.meta, 0);
			_palm_leaves (map, x + 1, tip - 1, z, this->bl_leaves.id, this->bl_leaves.meta, 1);
			_palm_leaves (map, x, tip - 1, z - 1, this->bl_leaves.id, this->bl_leaves.meta, 2);
			_palm_leaves (map, x, tip - 1, z + 1, this->bl_leaves.id, this->bl_leaves.meta, 3);
			
			// trunk
			for (int i = base; i <= tip; ++i)
				map.set (x, i, z, this->bl_trunk.id, this->bl_trunk.meta);

		}
		
		
		
//-----
		
		round_trees::round_trees (int min_height, blocki bl_trunk, blocki bl_leaves)
			: bl_trunk (bl_trunk), bl_leaves (bl_leaves)
		{
			this->min_height = min_height; 
			if (this->min_height < 1)
				this->min_height = 1;
		}
		
		
		
		void
		round_trees::seed (long s)
		{
			this->rnd.seed (s);
		}
		
		void
		round_trees::generate (world &wr, int x, int y, int z)
		{
			auto& rnd = this->rnd;
			std::uniform_int_distribution<> dis1 (0, 3), dis2 (0, 20);
			
			chunk_link_map map (wr, wr.get_chunk_at (x, z), x >> 4, z >> 4);
			
			int h = this->min_height + dis1 (rnd);
			int base = y;
		 	int tip  = y + h - 1;
			if (base < 1 || tip > 255) 
				return;
		
			// leaves
			int rad = h / 2;
			for (int xx = -rad; xx <= rad; ++xx)
				for (int yy = -rad; yy <= rad; ++yy)
					for (int zz = -rad; zz <= rad; ++zz)
						{
							if (xx*xx + yy*yy + zz*zz <= rad*rad)
								{
									if (dis2 (rnd) < 16)
										map.set (x + xx, tip - (rad - 2) + yy, z + zz, this->bl_leaves.id, this->bl_leaves.meta);
								}
						}
			
			// trunk
			for (int i = base; i <= tip; ++i)
				map.set (x, i, z, this->bl_trunk.id, this->bl_trunk.meta);
		}
		
		
		
//-----
		
		pine_trees::pine_trees (int min_height, int snow_prob, blocki bl_trunk, blocki bl_leaves)
			: bl_trunk (bl_trunk), bl_leaves (bl_leaves)
		{
			this->min_height = min_height; 
			if (this->min_height < 1)
				this->min_height = 1;
			
			this->snow_prob = snow_prob;
		}
		
		
		
		void
		pine_trees::seed (long s)
		{
			this->rnd.seed (s);
		}
		
		void
		pine_trees::generate (world &wr, int x, int y, int z)
		{
			auto& rnd = this->rnd;
			std::uniform_int_distribution<> dis1 (0, 3), dis2 (0, 20), dis3 (0, 99);
			
			chunk_link_map map (wr, wr.get_chunk_at (x, z), x >> 4, z >> 4);
			
			int h = this->min_height + dis1 (rnd);
			int base = y;
		 	int tip  = y + h - 1;
			if (base < 1 || tip > 255) 
				return;
		
			// leaves
			int ch = h - 3;
			float rr = (ch + 2) / 2, irr = rr;
			for (int yy = base + 3; yy <= tip; ++yy)
				{
					int rad = (int)rr;
					for (int xx = -rad; xx <= rad; ++xx)
						for (int zz = -rad; zz <= rad; ++zz)
							{
								if (xx*xx + zz*zz <= rad*rad)
									{
										if (dis2 (rnd) > 2)
											map.set (x + xx, yy, z + zz, this->bl_leaves.id, this->bl_leaves.meta);
									}
							}
					rr -= 0.5;
				}
			map.set (x, tip + 1, z, this->bl_leaves.id, this->bl_leaves.meta);
			map.set (x, tip + 2, z, this->bl_leaves.id, this->bl_leaves.meta);
			
			// cover with snow
			if (this->snow_prob > 0)
				{
					for (int xx = -irr; xx <= irr; ++xx)
						for (int zz = -irr; zz <= irr; ++zz)
							{
								if (xx*xx + zz*zz <= irr*irr)
									{
										for (int y = tip + 2; y >= base; --y)
											{
												if (map.get (xx + x, y, zz + z).id == BT_LEAVES)
													{
														if (dis3 (rnd) < this->snow_prob)
															map.set (x + xx, y + 1, zz + z, BT_SNOW_COVER);
														break;
													}
											}
									}
							}
				}
			
			// trunk
			for (int i = base; i <= tip; ++i)
				map.set (x, i, z, this->bl_trunk.id, this->bl_trunk.meta);
		}
	}
}

