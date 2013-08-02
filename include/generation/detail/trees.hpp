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

#ifndef _hCraft__WORLDGENERATOR__DETAIL__TREES_H_
#define _hCraft__WORLDGENERATOR__DETAIL__TREES_H_

#include "generation/worldgenerator.hpp"
#include "blocks.hpp"
#include <random>


namespace hCraft {
	namespace dgen {
		
		/* 
		 * Tree generators.
		 */
		 
		
		class generic_trees: public detail_generator
		{
			blocki bl_trunk;
			blocki bl_leaves;
			int min_height;
			
			std::minstd_rand rnd;
			
		public:
			generic_trees (int min_height = 4, blocki bl_trunk = {BT_TRUNK},
				blocki bl_leaves = {BT_LEAVES});
			
			virtual void seed (long s);
			virtual void generate (world &wr, int x, int y, int z);
		};
		
		
		class palm_trees: public detail_generator
		{
			blocki bl_trunk;
			blocki bl_leaves;
			int min_height;
			
			std::minstd_rand rnd;
			
		public:
			palm_trees (int min_height = 6, blocki bl_trunk = {BT_TRUNK, 3},
				blocki bl_leaves = {BT_LEAVES});
			
			virtual void seed (long s);
			virtual void generate (world &wr, int x, int y, int z);
		};
		
		
		class round_trees: public detail_generator
		{
			blocki bl_trunk;
			blocki bl_leaves;
			int min_height;
			
			std::minstd_rand rnd;
			
		public:
			round_trees (int min_height = 4, blocki bl_trunk = {BT_TRUNK},
				blocki bl_leaves = {BT_LEAVES});
			
			virtual void seed (long s);
			virtual void generate (world &wr, int x, int y, int z);
		};
		
		
		class pine_trees: public detail_generator
		{
			blocki bl_trunk;
			blocki bl_leaves;
			int min_height;
			int snow_prob;
			
			std::minstd_rand rnd;
			
		public:
			// snow_prob = 0-100
			pine_trees (int min_height = 7, int snow_prob = 0, blocki bl_trunk = {BT_TRUNK, 1},
				blocki bl_leaves = {BT_LEAVES, 1});
			
			virtual void seed (long s);
			virtual void generate (world &wr, int x, int y, int z);
		};
	}
}

#endif

