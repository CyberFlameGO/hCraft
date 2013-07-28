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

#include "generation/experiment.hpp"
#include "noise.hpp"
#include <functional>

#include <iostream> // DEBUG


namespace hCraft {
	
	/* 
	 * Constructs a new "experiment" world generator.
	 */
	experiment_world_generator::experiment_world_generator (long seed)
	{
		this->gen_seed = seed;
	}
	
	
	
	/* 
	 * Generates terrain on the specified chunk.
	 */
	void
	experiment_world_generator::generate (world& wr, chunk *out, int cx, int cz)
	{
		int x, y, z;
		
		unsigned int xz_hash = std::hash<long> () (((long)cz << 32) | cx) & 0xFFFFFFFF;
		this->rnd.seed (this->gen_seed + xz_hash);
		
		int xx, zz;
		double v;
		
		for (x = 0; x < 16; ++x)
			for (z = 0; z < 16; ++z)
				for (y = 0; y < 40; ++y)
					out->set_id (x, y, z, BT_STONE);
		
		for (x = 0; x < 16; ++x)
			for (z = 0; z < 16; ++z)
				for (y = 40; y < 100; ++y)
					{
						xx = (cx << 4) | x;
						zz = (cz << 4) | z;
						
						v = noise::lerp (
							noise::perlin_noise_3d (this->gen_seed, xx / 32.0 + 0.5, y / 32.0 + 0.5, zz / 32.0 + 0.5),
							noise::perlin_noise_3d (this->gen_seed + 13, xx / 32.0 + 0.5, y / 32.0 + 0.5, zz / 32.0 + 0.5),
							noise::perlin_noise_3d (this->gen_seed + 47, xx / 32.0 + 0.5, y / 32.0 + 0.5, zz / 32.0 + 0.5));
						v += (64.0 - y) * 0.032;
						
						if (v > 0.0)
							out->set_id (x, y, z, BT_DIRT);
					}
	}
}

