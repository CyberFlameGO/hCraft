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

#include "selection/sphere_selection.hpp"
#include "utils.hpp"
#include "player.hpp"
#include <cmath>
#include <unordered_set>


namespace hCraft {
	
	/* 
	 * Constructs a new sphere selection from the two given points.
	 */
	sphere_selection::sphere_selection (block_pos cp, block_pos op)
		: cp (cp)
	{
		this->rad = std::sqrt (
			std::pow (op.x - cp.x, 2) + 
			std::pow (op.y - cp.y, 2) + 
			std::pow (op.z - cp.z, 2));
	}
	
	sphere_selection::sphere_selection (block_pos cp, double rad)
		: cp (cp)
	{
		this->rad = rad;
	}
	
	
	
	/* 
	 * Checks whether the specified point is contained by the selected area.
	 */
	bool
	sphere_selection::contains (int x, int y, int z)
	{
		int dx = x - this->cp.x;
		int dy = y - this->cp.y;
		int dz = z - this->cp.z;
		
		return ((dx * dx) + (dy * dy) + (dz * dz)) <= (rad * rad);
	}
	
	
	
	/* 
	 * Returns the minimum and maximum points of this selection.
	 */
	block_pos
	sphere_selection::min ()
	{
		return block_pos (this->cp.x - this->rad, this->cp.y - this->rad,
			this->cp.z - this->rad);
	}
	
	block_pos
	sphere_selection::max ()
	{
		return block_pos (this->cp.x + this->rad, this->cp.y + this->rad,
			this->cp.z + this->rad);
	}
	
	
	
	/* 
	 * Sets the @{n}th point to @{pt}.
	 */
	void
	sphere_selection::set (int n, block_pos pt)
	{
		switch (n)
			{
				case 0:
					// center point
					this->cp = pt;
					break;
				case 1:
					// the other point
					this->rad = std::sqrt (
						std::pow (pt.x - this->cp.x, 2) + 
						std::pow (pt.y - this->cp.y, 2) + 
						std::pow (pt.z - this->cp.z, 2));
					break;
				
				default: return;
			}
	}
	
	
	
	static void
	_show (player *pl, sphere_selection *sel, bool show)
	{
		std::unordered_set<block_pos, block_pos_hash> bset;
		
		if (sel->radius () <= 1.0)
			return;
		sphere_selection s2 (sel->center (), sel->radius () - 1.0);
		
		block_pos pmin = sel->min (), pmax = sel->max ();
		for (int x = pmin.x; x <= pmax.x; ++x)
			for (int y = pmin.y; y <= pmax.y; ++y)
				for (int z = pmin.z; z <= pmax.z; ++z)
					{
						if (sel->contains (x, y, z) && !s2.contains (x, y, z))
							bset.emplace (x, y, z);
					}
		
		if (show)
			for (block_pos b : bset)
				pl->sb_add (b.x, b.y, b.z);
		else
			for (block_pos b : bset)
				pl->sb_remove (b.x, b.y, b.z);
	}
	
	/* 
	 * Draws a minimal wireframe version of the selection for the specified
	 * player (usually with water and brown mushrooms).
	 */
	void
	sphere_selection::show (player *pl)
	{
		world_selection::show (pl);
		_show (pl, this, true);
	}
	
	void
	sphere_selection::hide (player *pl)
	{
		world_selection::hide (pl);
		_show (pl, this, false);
	}
	
	
	/* 
	 * Moves the selection @{units} blocks into the direction @{dir}.
	 */
	void
	sphere_selection::move (direction dir, int units)
	{
		switch (dir)
			{
				case DI_EAST:
					this->cp.x += units;
					break;
				case DI_WEST:
					this->cp.x -= units;
					break;
				case DI_SOUTH:
					this->cp.z += units;
					break;
				case DI_NORTH:
					this->cp.z -= units;
					break;
				case DI_UP:
					this->cp.y += units;
					break;
				case DI_DOWN:
					this->cp.y -= units;
					break;
				
				default: return;
			}
	}
}

