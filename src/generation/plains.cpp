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

#include "generation/plains.hpp"
#include "blocks.hpp"
#include <random>


namespace hCraft {
	
	/* 
	 * Constructs a new flatgrass world generator.
	 */
	plains_world_generator::plains_world_generator (long seed)
	{
		this->gen_seed = seed; 
		this->gen_trees.seed (seed);
		
		this->pn.SetSeed (seed & 0x7FFFFFFF);
		this->pn.SetNoiseQuality (noise::QUALITY_FAST);
		this->pn.SetFrequency (0.037);
		this->pn.SetPersistence (0.2);
		this->pn.SetLacunarity (0.5);
	}
	
	
	
	/* 
	 * Generates on the specified chunk.
	 */
	void
	plains_world_generator::generate (world&  wr, chunk *out, int cx, int cz)
	{
		static const int water_cap = 59;
		std::minstd_rand rnd (this->gen_seed + cx * 1917 + cz * 3947);
		std::uniform_int_distribution<> dis (0, 3);
		
		std::uniform_int_distribution<> tdis (0, 3000);
		
		int y;
		for (int x = 0; x < 16; ++x)
			for (int z = 0; z < 16; ++z)
				{
					double n = this->pn.GetValue ((cx << 4) | x, 0, (cz << 4) | z);
					int l = 64 + (n * 13);
					
					out->set_id (x, 0, z, BT_BEDROCK);
					for (y = 1; y < (l - 5); ++y)
						out->set_id (x, y, z, BT_STONE);
					for (; y < l; ++y)
						out->set_id (x, y, z, BT_DIRT);
					
					if ((y + 1) >= water_cap)
						{
							if (y == water_cap - 1)
								{
									out->set_id (x, y++, z, BT_SAND);
								}
							else
								{
									out->set_id (x, y++, z, BT_GRASS);
									
									if (tdis (rnd) == 0)
										{
											this->gen_trees.generate (wr, (cx << 4) | x, y, (cz << 4) | z);
										}
								 	else if (dis (rnd) == 1)
										out->set_id_and_meta (x, y, z, BT_TALL_GRASS, 1);
								}
						}
					else
						{
							out->set_id (x, y++, z, BT_SAND);
							for (; y < water_cap; ++y)
								out->set_id (x, y, z, BT_WATER);						
						}
				}
	}
}

