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

#include "world/portal.hpp"


namespace hCraft {
	
	/* 
	 * Checks whether the specified block is contained within the portal's
	 * range.
	 */
	bool
	portal::in_range (int x, int y, int z)
	{
		return (x >= r_min.x && x <= r_max.x)
			  && (y >= r_min.y && y <= r_max.y)
			  && (z >= r_min.z && z <= r_max.z);
	}
	
	/* 
	 * Computes range minimum & maximum.
	 */
	void
	portal::calc_bounds ()
	{
		this->r_min.set ( 2147483647,  2147483647,  2147483647);
		this->r_max.set (-2147483648, -2147483648, -2147483648);
		
		for (block_pos bp : this->affected)
			{
				if (bp.x < this->r_min.x) this->r_min.x = bp.x;
				if (bp.x > this->r_max.x) this->r_max.x = bp.x;
				
				if (bp.y < this->r_min.y) this->r_min.y = bp.y;
				if (bp.y > this->r_max.y) this->r_max.y = bp.y;
				
				if (bp.z < this->r_min.z) this->r_min.z = bp.z;
				if (bp.z > this->r_max.z) this->r_max.z = bp.z;
			}
	}
}

