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

#ifndef _hCraft__WORLDGENERATOR__FLATPLAINS_H_
#define _hCraft__WORLDGENERATOR__FLATPLAINS_H_

#include "worldgenerator.hpp"
#include <noise/noise.h>


namespace hCraft {
	
	/* 
	 * A flatter version of the 'plains' generator, and with less tall grass
	 * and water.
	 */
	class flatplains_world_generator: public world_generator
	{
		long gen_seed;
		
		noise::module::Perlin pn;
		
	public:
		/* 
		 * Constructs a new flatplains world generator.
		 */
		flatplains_world_generator (long seed);
		
		
		/* 
		 * Returns the name of this generator.
		 */
		virtual const char* name ()
			{ return "flatplains"; }
		
		virtual long seed () override
			{ return this->gen_seed; }
		
		virtual void seed (long new_seed) override
			{ this->gen_seed = new_seed; }
		
		
		/* 
		 * Generates on the specified chunk.
		 */
		virtual void generate (world& wr, chunk *out, int cx, int cz);
	};
}

#endif

