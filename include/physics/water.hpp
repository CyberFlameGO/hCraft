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

#ifndef _hCraft__PHYSICS__WATER_H_
#define _hCraft__PHYSICS__WATER_H_

#include "physics.hpp"


namespace hCraft {
	
	namespace physics {
		
		class water: public physics_block
		{
		public:
			virtual int  id () override { return 8; }
			virtual int  vanilla_id () override { return 8; }
			virtual int  tick_rate () override { return 5; }
		
			virtual void tick (world &w, int x, int y, int z, int extra,
				void *ptr) override;
		};
	}
}

#endif

