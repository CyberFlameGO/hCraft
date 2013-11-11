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

#include "world/generation/sandbox.hpp"


namespace hCraft {
	
	/* 
	 * Constructs a new sandbox world generator.
	 */
	sandbox_world_generator::sandbox_world_generator (long seed)
	{
		this->gen_seed = seed;
	}
	
	
	
	/* 
	 * Generates sandbox terrain on the specified chunk.
	 */
	void
	sandbox_world_generator::generate (world& wr, chunk *out, int cx, int cz)
	{
		// contributed by Juholeil1
		int x, y, z;
		int realX ,realZ;
		
		int height = 64;
		int edge_size = 4;
		for (x = 0; x < 16; ++x)
			for (z = 0; z < 16; ++z)
				{
					out->set_id (x, 0, z, BT_BEDROCK);
					
					realX = cx * 16 + x;
					realZ = cz * 16 + z;
					out->set_id (x, 0, z, BT_BEDROCK);
					
					int w = wr.get_width ();
					int d = wr.get_depth ();
					if((w != 0 && d != 0) && ((realX < 0 + edge_size)
						|| (realZ < 0 + edge_size)
						|| (realX >= w - edge_size)
						|| (realZ >= d - edge_size))){
							for (y = 1; y < (height + 1); ++y)
								out->set_id (x, y, z, 5);
					}else{
						for (y = 1; y < height; ++y)
							out->set_id (x, y, z, BT_SAND);
					}
					out->set_biome (x, z, BI_PLAINS);
				}
	}
}

