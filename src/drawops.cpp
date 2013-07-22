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
#include <algorithm>

#include <iostream> // DEBUG


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
	draw_ops::draw_line (vector3 pt1, vector3 pt2, blocki material, int max_blocks)
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
	draw_ops::draw_bezier (std::vector<vector3>& points, blocki material, int max_blocks)
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
	draw_ops::draw_circle (vector3 pt, double radius, blocki material, plane pn, int max_blocks)
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
				if ((modified + 4) > max_blocks)
					return -1;
				modified += _plot_four_points (this->es, pt.x, pt.y, pt.z, x, z, material, pn);
				if (x != z)
					{
						if ((modified + 4) > max_blocks)
							return -1;
						modified += _plot_four_points (this->es, pt.x, pt.y, pt.z, z, x, material, pn);
					}
				
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
	
	
	
	static int
	_plot_ellipse_points (edit_stage& es, int cx, int cy, int cz, int a, int b, 
		blocki material, draw_ops::plane pn)
	{
		switch (pn)
			{
			case draw_ops::XZ_PLANE:
				if (a != 0)
					{
						es.set (cx - a, cy, cz + b, material.id, material.meta);
						es.set (cx + a, cy, cz + b, material.id, material.meta);
						es.set (cx + a, cy, cz - b, material.id, material.meta);
						es.set (cx - a, cy, cz - b, material.id, material.meta);
						return 4;
					}
				
				es.set (cx, cy, cz + b, material.id, material.meta);
				es.set (cx, cy, cz - b, material.id, material.meta);
				return 2;
			
			case draw_ops::YX_PLANE:
				if (a != 0)
					{
						es.set (cx - a, cy + b, cz, material.id, material.meta);
						es.set (cx + a, cy + b, cz, material.id, material.meta);
						es.set (cx + a, cy - b, cz, material.id, material.meta);
						es.set (cx - a, cy - b, cz, material.id, material.meta);
						return 4;
					}
				
				es.set (cx, cy + b, cz, material.id, material.meta);
				es.set (cx, cy - b, cz, material.id, material.meta);
				return 2;
			
			case draw_ops::YZ_PLANE:
				if (a != 0)
					{
						es.set (cx, cy + b, cz - a, material.id, material.meta);
						es.set (cx, cy + b, cz + a, material.id, material.meta);
						es.set (cx, cy - b, cz + a, material.id, material.meta);
						es.set (cx, cy - b, cz - a, material.id, material.meta);
						return 4;
					}
				
				es.set (cx, cy + b, cz, material.id, material.meta);
				es.set (cx, cy - b, cz, material.id, material.meta);
				return 2;
			}
		
		return 0;
	}
	
	/* 
	 * Draws a 2D ellipse centered at @{pt} with the given radius and material.
	 * Returns the total number of blocks modified.
	 */
	int
	draw_ops::draw_ellipse (vector3 pt, double a, double b, blocki material, plane pn, int max_blocks)
	{
		int modified = 0;
		long x = -a, z = 0;
		long e2 = b, dx = (1+2*x)*e2*e2;
		long dz = x*x, err = dx+dz;
		
		do
			{
				modified += _plot_ellipse_points (this->es, pt.x, pt.y, pt.z, x, z, material, pn);
				
				e2 = 2*err;
				if (e2 >= dx) { ++ x; err += dx += 2*(long)b*b; }
				if (e2 <= dz) { ++ z; err += dz += 2*(long)a*a; }
			}
		while (x <= 0);
		
		while (z++ < b)
			{
				modified += _plot_ellipse_points (this->es, pt.x, pt.y, pt.z, 0, z, material, pn);
			}
		
		return modified;
	}
	
	
	
	/* 
	 * Connects all specified points with lines to form a shape.
	 * Returns the total number of blocks modified.
	 */
	int
	draw_ops::draw_polygon (const std::vector<vector3>& points, blocki material, int max_blocks)
	{
		if (points.empty ()) return 0;
		if (points.size () == 1)
			{
				this->es.set (points[0].x, points[0].y, points[0].z, material.id, material.meta);
				return 1;
			}
		
		int modified = 0;
		for (int i = 0; i < ((int)points.size () - 1); ++i)
			modified += this->draw_line (points[i], points[i + 1], material);
		modified += this->draw_line (points[points.size () - 1], points[0], material);
		
		return modified;
	}
	
	
	
	static vector3
	catmull_spline (vector3 p0, vector3 p1, vector3 p2, vector3 p3, double t)
	{
		return 0.5 * ( ((2 * p1)) +
									 (t * (-p0 + p2)) +
									 ((t*t) * (2*p0 - 5*p1 + 4*p2 - p3)) +
									 ((t*t*t) * (-p0 + 3*p1 - 3*p2 + p3)) );
	}
	
	static int
	draw_curve_segment (draw_ops &draw, vector3 p0, vector3 p1, vector3 p2,
		vector3 p3, blocki material)
	{
		int modified = 0;
		
		vector3 last = p1;
		double t = 0.0;
		double t_inc = 1.0 / (p2 - p1).magnitude ();
		while (t <= 1.0)
			{
				vector3 next = catmull_spline (p0, p1, p2, p3, t);
				modified += draw.draw_line (last, next, material);
				last = next;
				t += t_inc;
			}
		
		return modified;
	}
	
	/* 
	 * Approximates a curve between the given vector of points.
	 * NOTE: The curve is guaranteed to pass through all BUT the first and last
	 * points.
	 * Returns the total number of blocks modified.
	 */
	int
	draw_ops::draw_curve (const std::vector<vector3>& points, blocki material, int max_blocks)
	{
		int modified = 0;
		
		switch (points.size ())
			{
				case 0: return 0;
				case 1:
					this->es.set (points[0].x, points[0].y, points[0].z, material.id, material.meta);
					return 1;
				case 2:
					this->es.set (points[1].x, points[1].y, points[1].z, material.id, material.meta);
					return 1;
				case 3:
					return this->draw_line (points[1], points[2], material);
			}
		
		int j = points.size () - 3;
		for (int i = 0; i < j; ++i)
			{
				modified += draw_curve_segment (*this,
					points[i + 0],
					points[i + 1],
					points[i + 2],
					points[i + 3], material);
			}
		
		return modified;
	}
	
	
	
	/* 
	 * Fills the cuboid bounded between the two specified points with the given
	 * block. Returns the total number of blocks modified.
	 */
	int
	draw_ops::fill_cuboid (vector3 pt1, vector3 pt2, blocki material, int max_blocks)
	{
		int sx = utils::min ((int)pt1.x, (int)pt2.x);
		int sy = utils::min ((int)pt1.y, (int)pt2.y);
		int sz = utils::min ((int)pt1.z, (int)pt2.z);
		int ex = utils::max ((int)pt1.x, (int)pt2.x);
		int ey = utils::max ((int)pt1.y, (int)pt2.y);
		int ez = utils::max ((int)pt1.z, (int)pt2.z);
		
		int c = 0;
		for (int x = sx; x <= ex; ++x)
			for (int y = sy; y <= ey; ++y)
				for (int z = sz; z <= ez; ++z)
					{
						if (c >= max_blocks)
							return -1;
						this->es.set (x, y, z, material.id, material.meta);
						++ c;
					}
		
		return c;
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
	straight_y_line (edit_stage& es, int x, int y1, int y2, int z, blocki material)
	{
		int ey = utils::max (y1, y2);
		int sy = utils::min (y1, y2);
		for (int y = sy; y <= ey; ++y)
			es.set (x, y, z, material.id, material.meta);
		return (ey - sy + 1);
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
		blocki material, draw_ops::plane pn, int max)
	{
		if (max == 0) return -1;
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
					if (modified > max) return -1;
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
					if (modified > max) return -1;
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
					if (modified > max) return -1;
					break;
			}
		
		return modified;
	}
	
	/* 
	 * Fills a 2D circle centered at @{pt} with the given radius and material.
	 * Returns the total number of blocks modified.
	 */
	int
	draw_ops::fill_circle (vector3 pt, double radius, blocki material, plane pn, int max_blocks)
	{
		int modified = 0;
		int iradius = std::round (radius);
		int error = -iradius;
		int x = iradius;
		int z = 0;
		int r;
		
		while (x >= z)
			{
				r = _plot_four_lines (this->es, pt.x, pt.y, pt.z, x, z, material, pn, max_blocks - modified);
				if (r == -1)
					return -1;
				else
					modified += r;
				
				if (x != z)
					{
						r = _plot_four_lines (this->es, pt.x, pt.y, pt.z, z, x, material, pn, max_blocks - modified);
						if (r == -1)
							return -1;
						else
							modified += r;
					}
				
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
	
	
	
	static int
	_plot_ellipse_lines (edit_stage& es, int cx, int cy, int cz, int a, int b, 
		blocki material, draw_ops::plane pn)
	{
		int modified = 0;
		switch (pn)
			{
			case draw_ops::XZ_PLANE:
				if (a != 0)
					{
						modified += straight_x_line (es, cx - a, cx + a, cy, cz + b, material);
						modified += straight_x_line (es, cx - a, cx + a, cy, cz - b, material);
					}
				else
					modified += straight_z_line (es, cx, cy, cz - b, cz + b, material);
				break;
			
			case draw_ops::YX_PLANE:
				if (a != 0)
					{
						modified += straight_x_line (es, cx - a, cx + a, cy + b, cz, material);
						modified += straight_x_line (es, cx - a, cx + a, cy - b, cz, material);
					}
				else
					modified += straight_y_line (es, cx, cy - b, cy + b, cz, material);
				break;
			
			case draw_ops::YZ_PLANE:
				if (a != 0)
					{
						modified += straight_z_line (es, cx, cy + b, cz - a, cz + a, material);
						modified += straight_z_line (es, cx, cy - b, cz - a, cz + a, material);
					}
				else
					modified += straight_y_line (es, cx, cy - b, cy + b, cz, material);
				break;
			}
		
		return modified;
	}
	
	/* 
	 * Fills a 2D ellipse centered at @{pt} with the given radius and material.
	 * Returns the total number of blocks modified.
	 */
	int
	draw_ops::fill_ellipse (vector3 pt, double a, double b, blocki material, plane pn, int max_blocks)
	{
		int modified = 0;
		long x = -a, z = 0;
		long e2 = b, dx = (1+2*x)*e2*e2;
		long dz = x*x, err = dx+dz;
		
		do
			{
				modified += _plot_ellipse_lines (this->es, pt.x, pt.y, pt.z, x, z, material, pn);
				
				e2 = 2*err;
				if (e2 >= dx) { ++ x; err += dx += 2*(long)b*b; }
				if (e2 <= dz) { ++ z; err += dz += 2*(long)a*a; }
			}
		while (x <= 0);
		
		while (z++ < b)
			{
				modified += _plot_ellipse_lines (this->es, pt.x, pt.y, pt.z, 0, z, material, pn);
			}
		
		return modified;
	}
	
	
	
	/* 
	 * Fills the sphere centered at @{pt} with the radius of @{radius} with the
	 * given block. Returns the total number of blocks modified.
	 */
	int
	draw_ops::fill_sphere (vector3 pt, double radius, blocki material, int max_blocks)
	{
		int rad = std::round (radius);
		int srad = rad * rad;
		
		int cx = pt.x, cy = pt.y, cz = pt.z;
		
		int modified = 0;
		for (int x = -rad; x <= rad; ++x)
			for (int y = -rad; y <= rad; ++y)
				for (int z = -rad; z <= rad; ++z)
					{
						if (x*x + y*y + z*z <= srad)
							{
								this->es.set (x + cx, y + cy, z + cz, material.id, material.meta);
								++ modified;
							}
					}
		
		return modified;
	}
	
	
	
	/* 
	 * Fills a hollow sphere centered at @{pt} with the radius of @{radius} with the
	 * given block. Returns the total number of blocks modified.
	 */
	int
	draw_ops::fill_hollow_sphere (vector3 pt, double radius, blocki material, int max_blocks)
	{
		int srad = std::round (radius * radius);
		int rad = std::round (radius);
		int cx = pt.x, cy = pt.y, cz = pt.z;
		
		enum {
			ST_BEFORE,
			ST_IN,
			ST_AFTER,
		} state = ST_BEFORE;
		
		int modified = 0;
		for (int y = -rad; y <= rad; ++y)
			for (int x = -rad; x <= rad; ++x)
				{
					state = ST_BEFORE;
					for (int z = -rad; z <= rad; ++z)
						{
							switch (state)
								{
								case ST_BEFORE:
									if ((x*x + y*y + z*z) <= srad)
										{
											this->es.set (x + cx, y + cy, z + cz, material.id, material.meta);
											state = ST_IN;
											++ modified;
										}
									break;
								
								case ST_IN:
									if ((x*x + y*y + z*z) > srad)
										{
											this->es.set (x + cx, y + cy, z + cz - 1, material.id, material.meta);
											state = ST_AFTER;
											++ modified;
										}
									break;
								
								case ST_AFTER: break;
								}
						}
				}
		
		return modified;
	}
}

