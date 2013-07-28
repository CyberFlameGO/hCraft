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

#include "generation/d3_normal.hpp"
#include "world.hpp"
#include <functional>
#include <cmath>

#include <iostream> // DEBUG


namespace hCraft {
	
	/* 
	 * Constructs a new D3 normal world generator.
	 */
	d3_normal_world_generator::d3_normal_world_generator (long seed)
	{
		this->gen_seed = seed;
	}
	
	
	
	/* 
	 * Generates terrain on the specified chunk.
	 */
	void
	d3_normal_world_generator::generate (world& wr, chunk *out, int cx, int cz)
	{
		this->rnd.seed (this->gen_seed);
		
#define IND(X,Z) (((X)*csize)+(Z))
		
		int wd_max = std::max (wr.get_width (), wr.get_depth ());
		int iterations = std::ceil (std::log2 (wd_max) - 1);
		
		int csize = wd_max * 118 / 100;
		double *counter = new double [csize * csize];
		for (int i = 0; i < (csize * csize); ++i)
			counter[i] = 0.0;
		
		int size = 2;
		
		int map_size_y = (wd_max <= 256) ? wd_max : 256;
		double height_stone = map_size_y * 0.3;
		double height_water = map_size_y * 0.5;
		double height_grass = map_size_y * 0.65;
		double height_mountain = map_size_y * 0.8;
		
		std::uniform_real_distribution<> dis1 (1.0, map_size_y * 0.9);
		for (int ix = 0; ix <= size; ++ix)
			for (int iz = 0; iz <= size; ++iz)
				{
					counter[IND(ix, iz)] = dis1 (this->rnd);
					if (counter[IND(ix, iz)] < height_water)
						counter[IND(ix, iz)] = (counter[IND(ix, iz)] + height_water*15) / 16.0;
					if (counter[IND(ix, iz)] > height_water && counter[IND(ix, iz)] < height_grass)
						counter[IND(ix, iz)] = (counter[IND(ix, iz)] + height_water*80) / 81.0;
				}
			
			std::uniform_real_distribution<> dis2 (1.0, 255.0);
			double rnd_factor;
			for (int i = 1; i <= iterations; ++i)
				{
					if (i >= (iterations - 1))
						rnd_factor = 0.0;
					else
						rnd_factor = 0.010 * (iterations - i);
					
					for (int ix = size; ix >= 0; --ix)
						for (int iz = size; iz >= 0; --iz)
							counter[IND(ix*2, iz*2)] = counter[IND(ix, iz)];
					
					for (int ix = 0; ix < size; ++ix)
						for (int iz = 0; iz < size; ++iz)
							{
								double rnd_factor_2 = rnd_factor;
								if (counter[IND(ix*2, iz*2)] <= height_water)
									rnd_factor_2 = rnd_factor * 0.5;
								else if (counter[IND(ix*2, iz*2)] <= height_grass)
									rnd_factor_2 = rnd_factor * 0.3;
								
								counter[IND(ix*2, iz*2+1)] = (counter[IND(ix*2, iz*2)] + counter[IND(ix*2, iz*2+2)]) / 2 + ((dis2 (rnd) - 128.0) * rnd_factor_2);
								counter[IND(ix*2+2, iz*2+1)] = (counter[IND(ix*2+2, iz*2)] + counter[IND(ix*2+2, iz*2+2)]) / 2 + ((dis2 (rnd) - 128.0) * rnd_factor_2);
								
								counter[IND(ix*2+1, iz*2)] = (counter[IND(ix*2, iz*2)] + counter[IND(ix*2+2, iz*2)]) / 2 + ((dis2 (rnd) - 128.0) * rnd_factor_2);
								counter[IND(ix*2+1, iz*2+2)] = (counter[IND(ix*2, iz*2+2)] + counter[IND(ix*2+2, iz*2+2)]) / 2 + ((dis2 (rnd) - 128.0) * rnd_factor_2);
								
								counter[IND(ix*2+1, iz*2+1)] = (counter[IND(ix*2, iz*2)] + counter[IND(ix*2+2, iz*2)] + counter[IND(ix*2, iz*2+2)] + counter[IND(ix*2+2, iz*2+2)]) / 4 + ((dis2 (rnd) - 128.0) * rnd_factor_2);
							}
					
					size *= 2;
				}
		
		int height, ix, iz;
		for (int xx = (cx << 4); xx < ((cx + 1) << 4); ++xx)
			for (int zz = (cz << 4); zz < ((cz + 1) << 4); ++zz)
				{
					ix = xx & 0xF;
					iz = zz & 0xF;
					height = std::ceil (counter[IND(xx, zz)]);

					out->set_biome (ix, iz, BI_FOREST);
					
					if (height > height_grass)
						{
							for (int iy = 0; iy <= (height - 1); ++iy)
								{
									if (iy >= (height_mountain - 1))
										out->set_id (ix, iy, iz, BT_WOOL);
									else
										out->set_id (ix, iy, iz, BT_STONE);
								}
						}
					else if (height > height_water)
						{
							for (int iy = 0; iy <= (height - 1); ++iy)
								{
									if (iy == (height - 1))
										out->set_id (ix, iy, iz, BT_GRASS);
									else if (iy > height_stone)
										out->set_id (ix, iy, iz, BT_DIRT);
									else
										out->set_id (ix, iy, iz, BT_STONE);
								}
						}
					else if (height <= height_water)
						{
							for (int iy = 0; iy <= (height_water - 1); ++iy)
								{
									if (iy > (height - 1))
										out->set_id (ix, iy, iz, BT_WATER);
									else if (iy == (height_water - 1))
										out->set_id (ix, iy, iz, BT_SAND);
									else if (iy > height_stone)
										out->set_id (ix, iy, iz, BT_DIRT);
									else
										out->set_id (ix, iy, iz, BT_STONE);
								}
						}
				}
			
		delete[] counter;
	}
}

