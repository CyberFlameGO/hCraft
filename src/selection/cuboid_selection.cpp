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

#include "selection/cuboid_selection.hpp"
#include "utils.hpp"


namespace hCraft {

	/* 
	 * Constructs a new cuboid selection from the two given points.
	 */
	cuboid_selection::cuboid_selection (block_pos a, block_pos b)
		: p1 (a), p2 (b)
	{
	}
	
	
	
	/* 
	 * Returns the minimum and maximum points of this selection.
	 */
	block_pos
	cuboid_selection::min ()
	{
		return {
			utils::min (this->p1.x, this->p2.x),
			utils::min (this->p1.y, this->p2.y),
			utils::min (this->p1.z, this->p2.z) };
	}
	
	block_pos
	cuboid_selection::max ()
	{
		return {
			utils::max (this->p1.x, this->p2.x),
			utils::max (this->p1.y, this->p2.y),
			utils::max (this->p1.z, this->p2.z) };
	}
	
	/* 
	 * Checks whether the specified point is contained by the selected area.
	 */
	bool
	cuboid_selection::contains (int x, int y, int z)
	{
		block_pos start = this->min (), end = this->max ();
		return ((x >= start.x) && (x <= end.x))
				&& ((y >= start.y) && (y <= end.y))
				&& ((z >= start.z) && (z <= end.z));
	}
	
	
	
	/* 
	 * Sets the @{n}th point to @{pt}.
	 */
	void
	cuboid_selection::set (int n, block_pos pt)
	{
		switch (n)
			{
				case 0: this->p1 = pt; break;
				case 1: this->p2 = pt; break;
			}
	}
}

