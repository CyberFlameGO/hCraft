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

#ifndef _hCraft__WORLDGENERATOR__OVERHANG_H_
#define _hCraft__WORLDGENERATOR__OVERHANG_H_

#include "worldgenerator.hpp"
#include <random>
#include <noise/noise.h>

#include "generation/detail/trees.hpp"


namespace hCraft {
	
	/* 
	 * A world generator that attempts to create various mountain overhangs by
	 * using three-dimensional perlin noise.
	 */
	class overhang_world_generator: public world_generator
	{
		std::minstd_rand rnd;
		long gen_seed;
		
		noise::module::Perlin pn1, pn2, pn3, pn4, pn5;
		noise::module::ScalePoint sp1;
		noise::module::Const co1, co2;
		noise::module::Multiply mu1;
		noise::module::Select se1;
		noise::module::Blend bl1;
		
		noise::module::Perlin pn_sand, pn_foliage;
		dgen::generic_trees gen_oak_trees, gen_birch_trees;
		dgen::palm_trees gen_palm_trees;
		
		/*
		noise::module::Perlin pn1, pn2;
		noise::module::Const co1, co2;
		noise::module::Multiply mu1, mu2;
		noise::module::ScalePoint sp1;
		noise::module::Voronoi vo1;
		noise::module::Blend bl1;
		
		noise::module::Perlin pn3;
		noise::module::Voronoi vo2;
		*/
		
	private:
		void terrain (world& wr, chunk *out, int cx, int cz);
		void decorate (world& wr, chunk *out, int cx, int cz);
		
	public:
		/* 
		 * Constructs a new overhang world generator.
		 */
		overhang_world_generator (long seed);
		
		
		/* 
		 * Returns the name of this generator.
		 */
		virtual const char* name ()
			{ return "overhang"; }
		
		virtual long seed ()
			{ return this->gen_seed; }
		
		 
		/* 
		 * Generates on the specified chunk.
		 */
		virtual void generate (world& wr, chunk *out, int cx, int cz);
	};
}

#endif

