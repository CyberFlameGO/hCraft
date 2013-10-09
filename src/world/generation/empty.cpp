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

#include "world/generation/empty.hpp"


namespace hCraft {
	
	/* 
	 * Constructs a new empty world generator.
	 */
	empty_world_generator::empty_world_generator (long seed)
	{
		this->gen_seed = seed;
	}
	
	
	
	/* 
	 * Generates on the specified chunk.
	 */
	void
	empty_world_generator::generate (world& wr, chunk *out, int cx, int cz)
	{
		int x, z;
		
		for (x = 0; x < 16; ++x)
			for (z = 0; z < 16; ++z)
				{
					out->set_id (x, 0, z, BT_BEDROCK);
				}
	}
}

