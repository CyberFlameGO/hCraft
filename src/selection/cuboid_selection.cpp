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
#include "player.hpp"


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
	
	
	
	/* 
	 * Expands\Contracts the selection in the given direction.
	 */
	void
	cuboid_selection::expand (int x, int y, int z)
	{
		bool x_1max = ((utils::max (this->p1.x, this->p2.x)) == this->p1.x);
		bool y_1max = ((utils::max (this->p1.y, this->p2.y)) == this->p1.y);
		bool z_1max = ((utils::max (this->p1.z, this->p2.z)) == this->p1.z);
		
		int xx = x, yy = y, zz = z;
		
		if (x_1max)
			xx = -x;
		this->p1.x -= xx;
		this->p2.x += xx;
		
		if (y_1max)
			yy = -y;
		this->p1.y -= yy;
		this->p2.y += yy;
		
		if (z_1max)
			zz = -z;
		this->p1.z -= zz;
		this->p2.z += zz;
	}
	
	void
	cuboid_selection::contract (int x, int y, int z)
	{
		bool x_1max = !((utils::max (this->p1.x, this->p2.x)) == this->p1.x);
		bool y_1max = !((utils::max (this->p1.y, this->p2.y)) == this->p1.y);
		bool z_1max = !((utils::max (this->p1.z, this->p2.z)) == this->p1.z);
		
		int xx = x, yy = y, zz = z;
		
		if (x_1max)
			xx = -x;
		this->p1.x -= xx;
		this->p2.x += xx;
		
		if (y_1max)
			yy = -y;
		this->p1.y -= yy;
		this->p2.y += yy;
		
		if (z_1max)
			zz = -z;
		this->p1.z -= zz;
		this->p2.z += zz;
	}
	
	
	
	static void
	draw_wireframe (player *pl, block_pos p1, block_pos p2, bool add)
	{
		int sx = utils::min (p1.x, p2.x);
		int sy = utils::min (p1.y, p2.y);
		int sz = utils::min (p1.z, p2.z);
		int ex = utils::max (p1.x, p2.x);
		int ey = utils::max (p1.y, p2.y);
		int ez = utils::max (p1.z, p2.z);
		int x, y, z;
		
		std::vector<block_pos> buf;
		
		for (x = sx; x <= ex; ++x)
			{
				buf.emplace_back (x, p1.y, p1.z);
				buf.emplace_back (x, p1.y, p2.z);
				buf.emplace_back (x, p2.y, p1.z);
				buf.emplace_back (x, p2.y, p2.z);
			}
		
		for (y = sy; y <= ey; ++y)
			{
				buf.emplace_back (p1.x, y, p1.z);
				buf.emplace_back (p1.x, y, p2.z);
				buf.emplace_back (p2.x, y, p1.z);
				buf.emplace_back (p2.x, y, p2.z);
			}
		
		for (z = sz; z <= ez; ++z)
			{
				buf.emplace_back (p1.x, p1.y, z);
				buf.emplace_back (p1.x, p2.y, z);
				buf.emplace_back (p2.x, p1.y, z);
				buf.emplace_back (p2.x, p2.y, z);
			}
		
		if (add)
			for (auto pos : buf)
				pl->sb_add (pos.x, pos.y, pos.z);
		else
			for (auto pos : buf)
				pl->sb_remove (pos.x, pos.y, pos.z);
	}
	
	
	/* 
	 * Draws a minimal wireframe version of the selection for the specified
	 * player (usually with water and brown mushrooms).
	 */
	void
	cuboid_selection::show (player *pl)
	{
		world_selection::show (pl);
		draw_wireframe (pl, this->p1, this->p2, true);
	}
	
	void
	cuboid_selection::hide (player *pl)
	{
		world_selection::hide (pl);
		draw_wireframe (pl, this->p1, this->p2, false);
	}
	
	
	/* 
	 * Returns the number of blocks contained by this selection.
	 */
	int
	cuboid_selection::volume ()
	{
		block_pos start = this->min (), end = this->max ();
		return (end.x - start.x + 1) * (end.y - start.y + 1) * (end.z - start.z + 1);
	}
	
	
	/* 
	 * Moves the selection @{units} blocks into the direction @{dir}.
	 */
	void
	cuboid_selection::move (direction dir, int units)
	{
		switch (dir)
			{
				case DI_EAST:
					this->p1.x += units;
					this->p2.x += units;
					break;
				case DI_WEST:
					this->p1.x -= units;
					this->p2.x -= units;
					break;
				case DI_SOUTH:
					this->p1.z += units;
					this->p2.z += units;
					break;
				case DI_NORTH:
					this->p1.z -= units;
					this->p2.z -= units;
					break;
				case DI_UP:
					this->p1.y += units;
					this->p2.y += units;
					break;
				case DI_DOWN:
					this->p1.y -= units;
					this->p2.y -= units;
					break;
				
				default: return;
			}
	}
}

