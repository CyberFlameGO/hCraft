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

#ifndef _hCraft__PORTAL_H_
#define _hCraft__PORTAL_H_

#include "position.hpp"
#include <string>
#include <vector>


namespace hCraft {
	
	/* 
	 * Block portal.
	 */
	struct portal
	{
		entity_pos  dest_pos;
		std::string dest_world;
		std::vector<block_pos> affected;
		
		// block range
		block_pos r_min, r_max;
		
	//----
		
		/* 
		 * Checks whether the specified block is contained within the portal's
		 * range.
		 */
		bool in_range (int x, int y, int z);
		
		/* 
		 * Computes range minimum & maximum.
		 */
		void calc_bounds ();
	};
}

#endif

