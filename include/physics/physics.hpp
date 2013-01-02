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

#ifndef _hCraft__PHYSICS_H_
#define _hCraft__PHYSICS_H_


namespace hCraft {
	
	class world;
	
	
	/* 
	 * An interface to all block physics classes.
	 */
	class physics_block
	{
	public:
		virtual int  id () = 0;
		virtual int  vanilla_id () { return 0; }
		
		/* 
		 * The amount of milliseconds between ticks.
		 */
		virtual int  tick_rate () { return 1000; }
		
		/* 
		 * Called every "tick_rate ()" milliseconds by the world's physics loop.
		 */
		virtual void tick (world &w, int x, int y, int z, int extra,
				void *ptr) = 0;
		
		/* 
		 * Called when a neighbouring block is destroyed\changed.
		 */
		virtual void on_neighbour_modified (world &w, int x, int y, int z,
			int nx, int ny, int nz) { }
	};
}

#endif

