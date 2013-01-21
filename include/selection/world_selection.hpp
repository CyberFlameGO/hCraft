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

#ifndef _hCraft__WORLD_SELECTION_H_
#define _hCraft__WORLD_SELECTION_H_

#include "position.hpp"


namespace hCraft {
	
	class player; // forward dec
	
	
	enum selection_type
	{
		ST_NONE,
		ST_CUBOID,
	};
	
	
	/* 
	 * Represents a selected area within a world.
	 */
	class world_selection
	{
	protected:
		bool hidden;
		
	public:
		world_selection () { hidden = true; }
		virtual ~world_selection () { } // destructor
		
		
		/* 
		 * Returns a copy of this selection.
		 */
		virtual world_selection* copy () = 0;
		
		
		
		/* 
		 * Checks whether the specified point is contained by the selected area.
		 */
		virtual bool contains (int x, int y, int z) = 0;
		
		/* 
		 * Returns the minimum and maximum points of this selection.
		 */
		virtual block_pos min () = 0;
		virtual block_pos max () = 0;
		
		/* 
		 * Returns the number of points required by this selection type.
		 */
		virtual int needed_points () = 0;
		
		
		/* 
		 * Sets the @{n}th point to @{pt}.
		 */
		virtual void set (int n, block_pos pt) = 0;
		virtual void set_update (int n, block_pos pt, player *pl)
			{
				this->hide (pl);
				this->set (n, pt);
				this->show (pl);
			}
		
		/* 
		 * Expands\Contracts the selection in the given direction.
		 */
		virtual void expand (int x, int y, int z) = 0;
		virtual void contract (int x, int y, int z) = 0;
		
		
		/* 
		 * Draws a minimal wireframe version of the selection for the specified
		 * player (usually with water and brown mushrooms).
		 */
		virtual void show (player *pl) { this->hidden = false; };
		virtual void hide (player *pl) { this->hidden = true; };
		virtual bool visible () { return !this->hidden; }
		
		
		/* 
		 * Moves the selection @{units} blocks into the direction @{dir}.
		 */
		virtual void move (direction dir, int units) = 0;
	};
}

#endif

