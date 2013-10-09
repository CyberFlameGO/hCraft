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

#ifndef _hCraft__CRAFTING_H_
#define _hCraft__CRAFTING_H_

#include "blocks.hpp"
#include "slot/slot.hpp"
#include <vector>


namespace hCraft {
	
	/* 
	 * A crafting recipe consists of 3 rows, each of which hold three characters.
	 * Every character must be a number between 0-8, which serve as an index into
	 * an array of materials.
	 */
	struct crafting_recipe
	{
		const char *recipe[3];
		blocki materials[9];
		
		// The position in which materials are placed in shapeless recipes do not
		// matter. To craft a shapeless recipe, one must only put the exact amount
		// of appropriate materials.
		bool shapeless;
		
		slot_item result;
	};
	
	
	/* 
	 * Handles recipe crafting.
	 */
	class crafting_manager
	{
		std::vector<crafting_recipe> shape_recipes, shapeless_recipes;
		
	private:
		/* 
		 * Initializes all the core recipes.
		 */
		void init_recipes ();
		
	public:
		/* 
		 * Class constructor.
		 */
		crafting_manager ();
		
		
		
		/* 
		 * Attempts to match the given 3x3 2D-array of blocks with a crafting recipe.
		 * On success, the desired recipe is returned; otherwise, nullptr is returned.
		 */
		const crafting_recipe* match (slot_item **table);
	};
}

#endif

