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

#ifndef _hCraft__WORLDGENERATOR__EMPTY_H_
#define _hCraft__WORLDGENERATOR__EMPTY_H_

#include "worldgenerator.hpp"


namespace hCraft {
	
	/* 
	 * A generator that basically does nothing.
	 */
	class empty_world_generator: public world_generator
	{
		long gen_seed;
		
	public:
		/* 
		 * Constructs a new empty world generator.
		 */
		empty_world_generator (long seed);
		
		
		/* 
		 * Returns the name of this generator.
		 */
		virtual const char* name ()
			{ return "empty"; }
		
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

