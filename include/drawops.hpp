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

#ifndef _hCraft__DRAWOPS_H_
#define _hCraft__DRAWOPS_H_

#include "position.hpp"
#include "blocks.hpp"
#include <vector>


namespace hCraft {
	
	class edit_stage;
	
	
	/* 
	 * A featureful class containing methods to draw both 2D and 3D primitives
	 * onto an edit stage supplied by the user.
	 */
	class draw_ops
	{
		edit_stage &es;
		
	public:
		enum plane {
			XZ_PLANE,
			YX_PLANE,
			YZ_PLANE,
		};
		
	public:
		/* 
		 * Constructs a new draw_ops instace around the given edit stage.
		 */
		draw_ops (edit_stage &es);
		
		
		/* 
		 * Draws a line from @{pt1} to @{pt2} using the specified material.
		 * Returns the total number of blocks modified.
		 */
		int draw_line (vector3 pt1, vector3 pt2, blocki material);
		
		/* 
		 * Draws an N-th order bezier curve using the given list of N control points
		 * in the specified material. Returns the total numebr of blocks modified.
		 */
		int draw_bezier (std::vector<vector3>& points, blocki material);
		
		/* 
		 * Draws a 2D circle centered at @{pt} with the given radius and material.
		 * Returns the total number of blocks modified.
		 */
		int draw_circle (vector3 pt, double radius, blocki material, plane pn = XZ_PLANE);
		
		/* 
		 * Draws a 2D ellipse centered at @{pt} with the given radius and material.
		 * Returns the total number of blocks modified.
		 */
		int draw_ellipse (vector3 pt, double a, double b, blocki material, plane pn = XZ_PLANE);
		
		/* 
		 * Connects all specified points with lines to form a shape.
		 * Returns the total number of blocks modified.
		 */
		int draw_polygon (const std::vector<vector3>& points, blocki material);
		
		/* 
		 * Approximates a curve between the given vector of points.
		 * NOTE: The curve is guaranteed to pass through all BUT the first and last
		 * points.
		 * Returns the total number of blocks modified.
		 */
		int draw_curve (const std::vector<vector3>& points, blocki material);
		
		
		
		/* 
		 * Fills the cuboid bounded between the two specified points with the given
		 * block. Returns the total number of blocks modified.
		 */
		int fill_cuboid (vector3 pt1, vector3 pt2, blocki material);
		
		/* 
		 * Fills the sphere centered at @{pt} with the radius of @{radius} with the
		 * given block. Returns the total number of blocks modified.
		 */
		int fill_sphere (vector3 pt, double radius, blocki material);
		
		/* 
		 * Fills a hollow sphere centered at @{pt} with the radius of @{radius} with the
		 * given block. Returns the total number of blocks modified.
		 */
		int fill_hollow_sphere (vector3 pt, double radius, blocki material);
		
		/* 
		 * Fills a 2D circle centered at @{pt} with the given radius and material.
		 * Returns the total number of blocks modified.
		 */
		int fill_circle (vector3 pt, double radius, blocki material, plane pn = XZ_PLANE);
		
		/* 
		 * Fills a 2D ellipse centered at @{pt} with the given radius and material.
		 * Returns the total number of blocks modified.
		 */
		int fill_ellipse (vector3 pt, double a, double b, blocki material, plane pn = XZ_PLANE);
	};
}

#endif

