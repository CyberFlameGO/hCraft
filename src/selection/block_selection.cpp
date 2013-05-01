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

#include "selection/block_selection.hpp"
#include "utils.hpp"
#include "player.hpp"


namespace hCraft {
	
	/* 
	 * Constructs a new block selection from the two given points.
	 */
	block_selection::block_selection (block_pos a, block_pos b)
		: p1 (a), p2 (b)
	{
		block_pos pmin = this->min (), pmax = this->max ();
		this->width = pmax.x - pmin.x + 1;
		this->height = pmax.y - pmin.y + 1;
		this->depth = pmax.z - pmin.z + 1;
		this->blocks.resize (this->width * this->height * this->depth, false);
		this->vol = 0;
	}
	
	
	
	/* 
	 * Returns the minimum and maximum points of this selection.
	 */
	block_pos
	block_selection::min ()
	{
		return {
			utils::min (this->p1.x, this->p2.x),
			utils::min (this->p1.y, this->p2.y),
			utils::min (this->p1.z, this->p2.z) };
	}
	
	block_pos
	block_selection::max ()
	{
		return {
			utils::max (this->p1.x, this->p2.x),
			utils::max (this->p1.y, this->p2.y),
			utils::max (this->p1.z, this->p2.z) };
	}
	
	
	
	void
	block_selection::reconstruct (block_pos a, block_pos b)
	{
		block_pos pp1 = this->p1, pp2 = this->p2;
		block_pos ppmin = this->min (), ppmax = this->max ();
		
		this->p1 = a;
		this->p2 = b;
		if (pp1 == this->p1 && pp2 == this->p2)
			return;
		
		int dx = this->p1.x - pp1.x;
		int dy = this->p1.y - pp1.y;
		int dz = this->p1.z - pp1.z;
		
		block_pos pmin = this->min (), pmax = this->max ();
		this->width = pmax.x - pmin.x + 1;
		this->height = pmax.y - pmin.y + 1;
		this->depth = pmax.z - pmin.z + 1;
		
		std::vector<bool> prev_blocks (this->blocks);
		int volume = this->width * this->height * this->depth;
		this->blocks.resize (volume);
		for (int i = 0; i < volume; ++i)
			this->blocks[i] = false;

		int x, y, z;
		int pwidth = ppmax.x - ppmin.x + 1;
		int pdepth = ppmax.z - ppmin.z + 1;
		
		int b_x = ppmax.x + dx, b_y = ppmax.y + dy, b_z = ppmax.z + dz;
		for (x = ppmin.x + dx; x <= b_x; ++x)
			for (y = ppmin.y + dy; y <= b_y; ++y)
				for (z = ppmin.z + dz; z <= b_z; ++z)
					{
						if ((x < pmin.x) || (y < pmin.y) || (z < pmin.z) ||
								(x > pmax.x) || (y > pmax.y) || (z > pmax.z))
						continue;
				
						if (prev_blocks[(((y - dy) - ppmin.y) * pdepth + ((z - dz) - ppmin.z)) * pwidth
							+ ((x - dx) - ppmin.x)])
							this->blocks[((y - pmin.y) * this->depth + (z - pmin.z))
								* this->width + (x - pmin.x)] = true;
					}
	}
	
	void
	block_selection::scale (block_pos a, block_pos b)
	{
		block_pos pp1 = this->p1, pp2 = this->p2;
		block_pos ppmin = this->min (), ppmax = this->max ();
		
		this->p1 = a;
		this->p2 = b;
		if (pp1 == this->p1 && pp2 == this->p2)
			return;
		
		block_pos pmin = this->min (), pmax = this->max ();
		this->width = pmax.x - pmin.x + 1;
		this->height = pmax.y - pmin.y + 1;
		this->depth = pmax.z - pmin.z + 1;
		
		std::vector<bool> prev_blocks (this->blocks);
		int volume = this->width * this->height * this->depth;
		this->blocks.resize (volume);
		for (int i = 0; i < volume; ++i)
			this->blocks[i] = false;
		this->vol = 0;
		
		int x, y, z;
		int pwidth = ppmax.x - ppmin.x + 1;
		int pheight = ppmax.y - ppmin.y + 1;
		int pdepth = ppmax.z - ppmin.z + 1;
		
		double scale_x = (double)pwidth / this->width;
		double scale_y = (double)pheight / this->height;
		double scale_z = (double)pdepth / this->depth;
		
		int px, py, pz;
		for (x = pmin.x; x <= pmax.x; ++x)
			for (y = pmin.y; y <= pmax.y; ++y)
				for (z = pmin.z; z <= pmax.z; ++z)
					{
						px = (x - pmin.x) * scale_x;
						py = (y - pmin.y) * scale_y;
						pz = (z - pmin.z) * scale_z;
						if (prev_blocks[(py * pdepth + pz) * pwidth + px])
							{
								this->blocks[((y - pmin.y) * this->depth + (z - pmin.z))
									* this->width + (x - pmin.x)] = true;
								++ this->vol;
							}
					}
	}
	
	
	
	/* 
	 * Checks whether the specified point is contained by the selected area.
	 */
	bool
	block_selection::contains (int x, int y, int z)
	{
		block_pos pmin = this->min ();
		block_pos pmax = this->max ();
		
		if ((x < pmin.x) || (y < pmin.y) || (z < pmin.z) ||
				(x > pmax.x) || (y > pmax.y) || (z > pmax.z))
			return false;
		
		x -= pmin.x;
		y -= pmin.y;
		z -= pmin.z;
		return this->blocks[(y * this->depth + z) * this->width + x];
	}
	
	void
	block_selection::set_block (int x, int y, int z, bool include)
	{
		block_pos pmin = this->min ();
		block_pos pmax = this->max ();
		
		if ((x < pmin.x) || (y < pmin.y) || (z < pmin.z) ||
				(x > pmax.x) || (y > pmax.y) || (z > pmax.z))
			return;
		
		x -= pmin.x;
		y -= pmin.y;
		z -= pmin.z;
		
		bool prev_val = this->blocks[(y * this->depth + z) * this->width + x];
		if (prev_val && !include)
			-- this->vol;
		else if (!prev_val && include)
			++ this->vol;
		this->blocks[(y * this->depth + z) * this->width + x] = include;
	}
	
	
	
	/* 
	 * Expands\Contracts the selection in the given direction.
	 */
	void
	block_selection::expand (int x, int y, int z)
	{
		block_pos a = this->p1, b = this->p2;
		bool x_1max = ((utils::max (this->p1.x, this->p2.x)) == this->p1.x);
		bool y_1max = ((utils::max (this->p1.y, this->p2.y)) == this->p1.y);
		bool z_1max = ((utils::max (this->p1.z, this->p2.z)) == this->p1.z);
		
		int xx = x, yy = y, zz = z;
		
		if (x_1max)
			xx = -x;
		a.x -= xx;
		b.x += xx;
		
		if (y_1max)
			yy = -y;
		a.y -= yy;
		b.y += yy;
		
		if (z_1max)
			zz = -z;
		a.z -= zz;
		b.z += zz;
		
		this->scale (a, b);
	}
	
	void
	block_selection::contract (int x, int y, int z)
	{
		block_pos a = this->p1, b = this->p2;
		bool x_1max = !((utils::max (this->p1.x, this->p2.x)) == this->p1.x);
		bool y_1max = !((utils::max (this->p1.y, this->p2.y)) == this->p1.y);
		bool z_1max = !((utils::max (this->p1.z, this->p2.z)) == this->p1.z);
		
		int xx = x, yy = y, zz = z;
		
		if (x_1max)
			xx = -x;
		a.x -= xx;
		b.x += xx;
		
		if (y_1max)
			yy = -y;
		a.y -= yy;
		b.y += yy;
		
		if (z_1max)
			zz = -z;
		a.z -= zz;
		b.z += zz;
		
		this->scale (a, b);
	}
	
	
	
	/* 
	 * Sets the @{n}th point to @{pt}.
	 */
	void
	block_selection::set (int n, block_pos pt)
	{
		block_pos a, b;
		if (n == 0)
			{
				a = pt;
				b = this->p2;
			}
		else
			{
				a = this->p1;
				b = pt;
			}
		
		this->reconstruct (a, b);
	}
	
	
	static void
	_show (player *pl, block_selection *sel, bool show)
	{
		block_pos pmin = sel->min (), pmax = sel->max ();
		
		std::lock_guard<std::mutex> sb_guard {pl->sb_lock};
		
		int x, y, z;
		for (x = pmin.x; x <= pmax.x; ++x)
			for (y = pmin.y; y <= pmax.y; ++y)
				for (z = pmin.z; z <= pmax.z; ++z)
					{
						if (sel->contains (x, y, z))
							{
								if (show)
									pl->sb_add_nolock (x, y, z);
								else
									pl->sb_remove_nolock (x, y, z);
							}
					}
	}
	
	/* 
	 * Draws a minimal wireframe version of the selection for the specified
	 * player (usually with water and brown mushrooms).
	 */
	void
	block_selection::show (player *pl)
	{
		world_selection::show (pl);
		_show (pl, this, true);
	}
	void
	block_selection::hide (player *pl)
	{
		world_selection::hide (pl);
		_show (pl, this, false);
	}
	
	
	/* 
	 * Returns the number of blocks contained by this selection.
	 */
	int
	block_selection::volume ()
	{
		return this->vol;
	}
	
	
	/* 
	 * Moves the selection @{units} blocks into the direction @{dir}.
	 */
	void
	block_selection::move (direction dir, int units)
	{
		block_pos a = this->p1, b = this->p2;
		
		switch (dir)
			{
				case DI_EAST:
					a.x += units;
					b.x += units;
					break;
				case DI_WEST:
					a.x -= units;
					b.x -= units;
					break;
				case DI_SOUTH:
					a.z += units;
					b.z += units;
					break;
				case DI_NORTH:
					a.z -= units;
					b.z -= units;
					break;
				case DI_UP:
					a.y += units;
					b.y += units;
					break;
				case DI_DOWN:
					a.y -= units;
					b.y -= units;
					break;
				
				default: return;
			}
		
		this->reconstruct (a, b);
	}
}

