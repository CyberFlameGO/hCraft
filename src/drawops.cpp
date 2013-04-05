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

#include "drawops.hpp"
#include "editstage.hpp"
#include "utils.hpp"
#include <alloca.h>
#include <cmath>


namespace hCraft {
	
	/* 
	 * Constructs a new draw_ops instace around the given edit stage.
	 */
	draw_ops::draw_ops (edit_stage &es)
		: es (es)
		{ }
	
	
	
	/* 
	 * Draws a line from @{pt1} to @{pt2} using the specified material.
	 * Returns the total number of blocks modified.
	 */
	int
	draw_ops::draw_line (vector3 pt1, vector3 pt2, blocki material)
	{
		/* 
		 * Bresenham's line algorithm modified to work in three dimensions.
		 */
		
		int x1 = (int)(pt1.x);
		int y1 = (int)(pt1.y);
		int z1 = (int)(pt1.z);
		int x2 = (int)(pt2.x);
		int y2 = (int)(pt2.y);
		int z2 = (int)(pt2.z);
		
		int xd, yd, zd;
	  int x, y, z;
	  int ax, ay, az;
	  int sx, sy, sz;
	  int dx, dy, dz;

	  dx = x2 - x1;
	  dy = y2 - y1;
	  dz = z2 - z1;

	  ax = utils::iabs (dx) << 1;
	  ay = utils::iabs (dy) << 1;
	  az = utils::iabs (dz) << 1;

	  sx = utils::zsgn (dx);
	  sy = utils::zsgn (dy);
	  sz = utils::zsgn (dz);

	  x = x1;
	  y = y1;
	  z = z1;
	  
	  int modified = 0;

	  if (ax >= utils::max (ay, az))            /* x dominant */
			{
		    yd = ay - (ax >> 1);
		    zd = az - (ax >> 1);
		    for (;;)
				  {
			      this->es.set (x, y, z, material.id, material.meta);
			      ++ modified;
			      if (x == x2)
					  	break;

			      if (yd >= 0)
					    {
				        y += sy;
				        yd -= ax;
					    }

			      if (zd >= 0)
					    {
				        z += sz;
				        zd -= ax;
					    }

			      x += sx;
			      yd += ay;
			      zd += az;
				  }
			}
	  else if (ay >= utils::max (ax, az))            /* y dominant */
	  {
      xd = ax - (ay >> 1);
      zd = az - (ay >> 1);
      for (;;)
		    {
	      	this->es.set (x, y, z, material.id, material.meta);
	      	++ modified;
	        if (y == y2)
			    	break;

	        if (xd >= 0)
			      {
		          x += sx;
		          xd -= ay;
			      }

	        if (zd >= 0)
			      {
		          z += sz;
		          zd -= ay;
			      }

	        y += sy;
	        xd += ax;
	        zd += az;
		    }
	  }
	  else if (az >= utils::max (ax, ay))            /* z dominant */
			{
		    xd = ax - (az >> 1);
		    yd = ay - (az >> 1);
		    for (;;)
				  {
			      this->es.set (x, y, z, material.id, material.meta);
			      ++ modified;
			      if (z == z2)
					    break;

			      if (xd >= 0)
					    {
				        x += sx;
				        xd -= az;
					    }

			      if (yd >= 0)
					    {
				        y += sy;
				        yd -= az;
					    }

			      z += sz;
			      xd += ax;
			      yd += ay;
				  }
			}
		
		return modified;
	}
	
	
	
	// with t being in the range of 0-1
	static inline vector3
	lerp3 (vector3 a, vector3 b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t };
	}
	
	static vector3
	bezier_curve (std::vector<vector3>& points, double t, int start, int end)
	{
		int i, s = points.size ();
		vector3 *arr = (vector3 *)alloca (s * sizeof (vector3));
		for (i = 0; i < s; ++i)
			arr[i] = points[i];
		while (s > 2)
			{
				-- s;
				for (i = 0; i < s; ++i)
					arr[i] = lerp3 (arr[i], arr[i + 1], t);
			}
		return lerp3 (arr[0], arr[1], t);
	}
	
	/* 
	 * Draws an N-th order bezier curve using the given list of N control points
	 * in the specified material. Returns the total numebr of blocks modified.
	 */
	int
	draw_ops::draw_bezier (std::vector<vector3>& points, blocki material)
	{
		if (points.size () == 1)
			this->es.set (points[0].x, points[0].y, points[0].z, material.id, material.meta);
		
		int modified = 0;
		double t = 0.0;
		double inc =
			1.0 / (points[points.size () - 1] - points[0]).magnitude ();
		vector3 last_pt = points[0];
		while (t <= 1.0)
			{
				vector3 pt = bezier_curve (points, t, 0, points.size () - 1);
				modified += this->draw_line (last_pt, pt, material);
				last_pt = pt;
				t += inc;
			}
		
		return modified;
	}
	
	
	
	static int
	_plot_four_points (edit_stage& es, int cx, int cy, int cz, int x, int z,
		blocki material, draw_ops::plane pn)
	{
		int modified = 1;
		
		switch (pn)
			{
				case draw_ops::XZ_PLANE:
					es.set (cx + x, cy, cz + z, material.id, material.meta);
					if (x != 0)
						{
							es.set (cx - x, cy, cz + z, material.id, material.meta);
							++ modified;
						}
					if (z != 0) 
						{
							es.set (cx + x, cy, cz - z, material.id, material.meta);
							++ modified;
						}
					if (x != 0 && z != 0)
						{
							es.set (cx - x, cy, cz - z, material.id, material.meta);
							++ modified;
						}
					break;
				
				case draw_ops::YX_PLANE:
					es.set (cx + x, cy + z, cz, material.id, material.meta);
					if (x != 0)
						{
							es.set (cx - x, cy + z, cz, material.id, material.meta);
							++ modified;
						}
					if (z != 0) 
						{
							es.set (cx + x, cy - z, cz, material.id, material.meta);
							++ modified;
						}
					if (x != 0 && z != 0)
						{
							es.set (cx - x, cy - z, cz, material.id, material.meta);
							++ modified;
						}
					break;
				
				case draw_ops::YZ_PLANE:
					es.set (cx, cy + z, cz + x, material.id, material.meta);
					if (x != 0)
						{
							es.set (cx, cy + z, cz - x, material.id, material.meta);
							++ modified;
						}
					if (z != 0) 
						{
							es.set (cx, cy - z, cz + x, material.id, material.meta);
							++ modified;
						}
					if (x != 0 && z != 0)
						{
							es.set (cx, cy - z, cz - x, material.id, material.meta);
							++ modified;
						}
					break;
			}
		
		return modified;
	}
	
	/* 
	 * Draws a 2D circle centered at @{pt} with the given radius and material.
	 * Returns the total number of blocks modified.
	 */
	int
	draw_ops::draw_circle (vector3 pt, double radius, blocki material, plane pn)
	{
		/* 
		 * Bresenham's 2D circle algorithm. 
		 */
		
		int modified = 0;
		int iradius = std::round (radius);
		int error = -iradius;
		int x = iradius;
		int z = 0;
		
		while (x >= z)
			{
				modified += _plot_four_points (this->es, pt.x, pt.y, pt.z, x, z, material, pn);
				if (x != z)
					modified += _plot_four_points (this->es, pt.x, pt.y, pt.z, z, x, material, pn);
				
				error += z;
				++ z;
				error += z;
				
				if (error >= 0)
					{
						error -= x;
						-- x;
						error -= x;
					}
			}
		
		return modified;
	}
	
	
	
	/* 
	 * Fills the cuboid bounded between the two specified points with the given
	 * block. Returns the total number of blocks modified.
	 */
	int
	draw_ops::fill_cuboid (vector3 pt1, vector3 pt2, blocki material)
	{
		int sx = utils::min ((int)pt1.x, (int)pt2.x);
		int sy = utils::min ((int)pt1.y, (int)pt2.y);
		int sz = utils::min ((int)pt1.z, (int)pt2.z);
		int ex = utils::max ((int)pt1.x, (int)pt2.x);
		int ey = utils::max ((int)pt1.y, (int)pt2.y);
		int ez = utils::max ((int)pt1.z, (int)pt2.z);
		
		for (int x = sx; x <= ex; ++x)
			for (int y = sy; y <= ey; ++y)
				for (int z = sz; z <= ez; ++z)
					{
						this->es.set (x, y, z, material.id, material.meta);
					}
		
		return ((ex - sx + 1) * (ey - sy + 1) * (ez - sz + 1));
	}
	
	
	
	static int
	straight_x_line (edit_stage& es, int x1, int x2, int y, int z, blocki material)
	{
		int ex = utils::max (x1, x2);
		int sx = utils::min (x1, x2);
		for (int x = sx; x <= ex; ++x)
			es.set (x, y, z, material.id, material.meta);
		return (ex - sx + 1);
	}
	
	static int
	straight_z_line (edit_stage& es, int x, int y, int z1, int z2, blocki material)
	{
		int ez = utils::max (z1, z2);
		int sz = utils::min (z1, z2);
		for (int z = sz; z <= ez; ++z)
			es.set (x, y, z, material.id, material.meta);
		return (ez - sz + 1);
	}
	
	static int
	_plot_four_lines (edit_stage& es, int cx, int cy, int cz, int x, int z,
		blocki material, draw_ops::plane pn)
	{
		int modified = 1;
		
		switch (pn)
			{
				case draw_ops::XZ_PLANE:
					es.set (cx + x, cy, cz + z, material.id, material.meta);
					if (x != 0)
						{
							modified += straight_x_line (es, cx + x, cx - x, cy, cz + z, material) - 1;
						}
					if (z != 0) 
						{
							es.set (cx + x, cy, cz - z, material.id, material.meta);
							if (x != 0)
								{
									modified += straight_x_line (es, cx + x, cx - x, cy, cz - z, material);
								}
							else
								++ modified;
						}
					break;
				
				case draw_ops::YX_PLANE:
					es.set (cx + x, cy + z, cz, material.id, material.meta);
					if (x != 0)
						{
							modified += straight_x_line (es, cx + x, cx - x, cy + z, cz, material) - 1;
						}
					if (z != 0) 
						{
							es.set (cx + x, cy - z, cz, material.id, material.meta);
							if (x != 0 && z != 0)
								{
									modified += straight_x_line (es, cx + x, cx - x, cy - z, cz, material);
								}
							else
								++ modified;
						}
					break;
				
				case draw_ops::YZ_PLANE:
					es.set (cx, cy + z, cz + x, material.id, material.meta);
					if (x != 0)
						{
							modified += straight_z_line (es, cx, cy + z, cz + x, cz - x, material) - 1;
						}
					if (z != 0) 
						{
							es.set (cx, cy - z, cz + x, material.id, material.meta);
							if (x != 0)
								{
									modified += straight_z_line (es, cx, cy - z, cz + x, cz - x, material);
								}
							else
								++ modified;
						}
					break;
			}
		
		return modified;
	}
	
	/* 
	 * Fills a 2D circle centered at @{pt} with the given radius and material.
	 * Returns the total number of blocks modified.
	 */
	int
	draw_ops::fill_circle (vector3 pt, double radius, blocki material, plane pn)
	{
		int modified = 0;
		int iradius = std::round (radius);
		int error = -iradius;
		int x = iradius;
		int z = 0;
		
		while (x >= z)
			{
				modified += _plot_four_lines (this->es, pt.x, pt.y, pt.z, x, z, material, pn);
				if (x != z)
					modified += _plot_four_lines (this->es, pt.x, pt.y, pt.z, z, x, material, pn);
				
				error += z;
				++ z;
				error += z;
				
				if (error >= 0)
					{
						error -= x;
						-- x;
						error -= x;
					}
			}
		
		return modified;
	}
}

