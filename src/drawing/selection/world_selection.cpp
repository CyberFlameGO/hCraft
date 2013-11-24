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

#include "drawing/selection/world_selection.hpp"
#include "drawing/selection/cuboid_selection.hpp"
#include "drawing/selection/sphere_selection.hpp"
#include "util/utils.hpp"


namespace hCraft {
	
	/* 
	 * Constructs a new selection from the serialized data in the specified byte
	 * array.
	 * Returns null on failure.
	 */
	world_selection*
	world_selection::deserialize (const unsigned char *data,
		unsigned int len)
	{
		if (len == 0)	return nullptr;
		
		unsigned char type = data[0];
		switch (type)
			{
				case (int)ST_CUBOID:
					if (len != 19)
						return nullptr;
					{
						block_pos p1, p2;
						
						p1.x = utils::read_int (data + 1);
						p1.y = data[5];
						p1.z = utils::read_int (data + 6);
						
						p2.x = utils::read_int (data + 10);
						p2.y = data[14];
						p2.z = utils::read_int (data + 15);
						
						return new cuboid_selection (p1, p2);
					}
				
				case (int)ST_SPHERE:
					if (len != 18)
						return nullptr;
					{
						block_pos cp;
						
						cp.x = utils::read_int (data + 1);
						cp.y = data[5];
						cp.z = utils::read_int (data + 6);
						
						double rad = utils::read_double (data + 10);
						
						return new sphere_selection (cp, rad);
					}
			}
		
		return nullptr;
	}
}

