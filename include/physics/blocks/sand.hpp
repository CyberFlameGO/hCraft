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

#ifndef _hCraft__PHYSICS__SAND_H_
#define _hCraft__PHYSICS__SAND_H_

#include "physics/blocks/physics_block.hpp"


namespace hCraft {
	
	namespace physics {
		
		class sand: public physics_block
		{
		public:
			virtual int  id () override { return BT_SAND; }
			virtual blocki vanilla_block () override { return BT_SAND; }
			virtual int  tick_rate () override { return 3; }
			virtual const char* name () { return "sand"; }
			virtual bool affected_by_neighbours () { return true; }
		
			virtual void tick (world &w, int x, int y, int z, int data,
				void *ptr, std::minstd_rand& rnd) override;
			virtual void on_neighbour_modified (world &w, int x, int y, int z,
				int nx, int ny, int nz) override;
		};
	}
}

#endif

