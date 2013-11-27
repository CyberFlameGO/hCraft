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

#ifndef _hCraft__WORLDGENERATOR__SUPER_OVERHANG_H_
#define _hCraft__WORLDGENERATOR__SUPER_OVERHANG_H_

#include "world/generation/worldgenerator.hpp"
#include <noise/noise.h>

#include "world/generation/detail/trees.hpp"


namespace hCraft {
	
	/* 
	 * Extreme overhang generator.
	 */
	class super_overhang_world_generator: public world_generator
	{
		long gen_seed;
		
		noise::module::Perlin pn1, pn2, pn3;
		noise::module::Const co1;
		noise::module::Multiply mu1;
		noise::module::Select se1;
		
		dgen::pine_trees gen_trees;
		
	private:
		void terrain (world& wr, chunk *out, int cx, int cz);
		void decorate (world& wr, chunk *out, int cx, int cz);
		
	public:
		/* 
		 * Constructs a new super overhang world generator.
		 */
		super_overhang_world_generator (long seed);
		
		
		/* 
		 * Returns the name of this generator.
		 */
		virtual const char* name ()
			{ return "super-overhang"; }
		
		virtual long seed () override
			{ return this->gen_seed; }
		
		virtual void seed (long new_seed) override;
		
		
		/* 
		 * Generates on the specified chunk.
		 */
		virtual void generate (world& wr, chunk *out, int cx, int cz);
	};
}

#endif

