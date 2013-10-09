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

#include "world/generation/flatplains.hpp"
#include "slot/blocks.hpp"
#include <random>


namespace hCraft {
	
	/* 
	 * Constructs a new flatgrass world generator.
	 */
	flatplains_world_generator::flatplains_world_generator (long seed)
	{
		this->gen_seed = seed; 
		
		this->pn.SetSeed (seed & 0x7FFFFFFF);
		this->pn.SetNoiseQuality (noise::QUALITY_FAST);
		this->pn.SetFrequency (0.03);
		this->pn.SetPersistence (0.3);
		this->pn.SetLacunarity (0.5);
	}
	
	
	
	/* 
	 * Generates on the specified chunk.
	 */
	void
	flatplains_world_generator::generate (world&  wr, chunk *out, int cx, int cz)
	{
		std::minstd_rand rnd (this->gen_seed + cx * 1917 + cz * 3947);
		std::uniform_int_distribution<> dis (0, 100);
		
		int y;
		for (int x = 0; x < 16; ++x)
			for (int z = 0; z < 16; ++z)
				{
					double n = this->pn.GetValue (((cx << 4) | x) * 0.35, 0, ((cz << 4) | z) * 0.35);
					int l = 64 + (n * 6.0);
					
					out->set_id (x, 0, z, BT_BEDROCK);
					for (y = 1; y < (l - 5); ++y)
						out->set_id (x, y, z, BT_STONE);
					for (; y < l; ++y)
						out->set_id (x, y, z, BT_DIRT);
					out->set_id (x, y++, z, BT_GRASS);
					
					if (dis (rnd) == 1)
						out->set_block (x, y, z, BT_TALL_GRASS, 1);
					out->set_biome (x, z, BI_FOREST);
				}
	}
}

