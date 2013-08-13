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

#include "crafting.hpp"


namespace hCraft {
	
	/* 
	 * Class constructor.
	 */
	crafting_manager::crafting_manager ()
	{
		this->init_recipes ();
	}
	
	
	
	/* 
	 * Initializes all the core recipes.
	 */
	void
	crafting_manager::init_recipes ()
	{
		// trunk to wooden planks
		this->shapeless_recipes.push_back ({ {"0__", "___", "___"}, { {BT_TRUNK, 0} }, true, { BT_WOOD, 0 } });
		this->shapeless_recipes.push_back ({ {"0__", "___", "___"}, { {BT_TRUNK, 1} }, true, { BT_WOOD, 1 } });
		this->shapeless_recipes.push_back ({ {"0__", "___", "___"}, { {BT_TRUNK, 2} }, true, { BT_WOOD, 2 } });
		this->shapeless_recipes.push_back ({ {"0__", "___", "___"}, { {BT_TRUNK, 3} }, true, { BT_WOOD, 3 } });
	}
	
	
	
	static int
	material_count (crafting_recipe& recipe)
	{
		int highest = -1;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				{
					if (recipe.recipe[i][j] > (highest + '0'))
						highest = recipe.recipe[i][j] - '0';
				}
		
		return highest + 1;
	}
	
	static int
	count_type (crafting_recipe& recipe, int m)
	{
		int count = 0;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				if (recipe.recipe[i][j] == (m - '0'))
					++ count;
		
		return count;
	}
	
	static bool
	shapeless_match (slot_item **table, crafting_recipe& recipe)
	{
		int m_count = material_count (recipe);
		
		// for every material, check how many times it occurs in both the crafting
		// recipe structure, and in the specified table.
		for (int i = 0; i < m_count; ++i)
			{
				blocki mat = recipe.materials[i];
				int count = count_type (recipe, i);
				
				// check that the same amount exists in the crafting table.
				int ct_count = 0;
				for (int j = 0; j < 3; ++j)
					for (int k = 0; k < 3; ++k)
						{
							// NOTE: the actual amount within the slot does not matter (for now).
							slot_item &s = table[j][k];
							if (s.id () == mat.id && s.damage () == mat.meta)
								++ ct_count;
						}
				
				if (count != ct_count)
					return false; // no match
			}
		
		return true;
	}
	
	/* 
	 * Attempts to match the given 3x3 2D-array of blocks with a crafting recipe.
	 * On success, the desired recipe is returned; otherwise, nullptr is returned.
	 */
	const crafting_recipe*
	crafting_manager::match (slot_item **table)
	{
		// try recipes with a shape first
		
		// and then shapeless recipes
		for (crafting_recipe& res : this->shapeless_recipes)
			if (shapeless_match (table, res))
				return &res;
		
		return nullptr;
	}
}

