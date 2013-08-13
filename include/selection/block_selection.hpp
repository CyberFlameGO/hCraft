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

#ifndef _hCraft__WORLD_BLOCK_SELECTION_H_
#define _hCraft__WORLD_BLOCK_SELECTION_H_

#include "world_selection.hpp"
#include "position.hpp"
#include <vector>


namespace hCraft {
	
	/* 
	 * A special type of a region that can store all kinds of selections, with the
	 * only disadvantage of higher memory use (All blocks are stored in a bitset).
	 */
	class block_selection: public world_selection
	{
		block_pos p1, p2;
		
		std::vector<bool> blocks; // bitset
		int width, height, depth;
		int vol; // volume
		
	private:
		void reconstruct (block_pos a, block_pos b);
		void scale (block_pos a, block_pos b);
		
	public:
		/* 
		 * Constructs a new block selection from the two given points.
		 */
		block_selection (block_pos a, block_pos b);
		
		/* 
		 * Returns a copy of this selection.
		 */
		world_selection* copy ()
			{ return new block_selection (*this); }
		
		
		/* 
		 * Checks whether the specified point is contained by the selected area.
		 */
		virtual bool contains (int x, int y, int z);
		
		
		/* 
		 * Returns the minimum and maximum points of this selection.
		 */
		virtual block_pos min ();
		virtual block_pos max ();
		
		
		/* 
		 * Returns the number of points required by this selection type.
		 */
		virtual int needed_points () { return 2; }
		
		
		/* 
		 * Sets the @{n}th point to @{pt}.
		 */
		virtual void set (int n, block_pos pt);
		void set_block (int x, int y, int z, bool include = true);
		virtual block_pos get (int n);
		
		/* 
		 * Expands\Contracts the selection in the given direction.
		 */
		virtual void expand (int x, int y, int z);
		virtual void contract (int x, int y, int z);
		
		/* 
		 * Returns the number of blocks contained by this selection.
		 */
		virtual int volume ();
		
		
		/* 
		 * Draws a minimal wireframe version of the selection for the specified
		 * player (usually in water blocks).
		 */
		virtual void show (player *pl);
		virtual void hide (player *pl);
		
		
		/* 
		 * Moves the selection @{units} blocks into the direction @{dir}.
		 */
		virtual void move (direction dir, int units);
	};
}

#endif

