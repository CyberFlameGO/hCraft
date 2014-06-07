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

#include "slot/crafting.hpp"
#include "slot/items.hpp"
#include <cctype>

#include <iostream> // DEBUG


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
		this->shape_recipes.push_back ({ {"0__", "0__", "___"}, { {BT_WOOD, 0x10} }, false, {IT_STICK, 0, 4} });
		this->shape_recipes.push_back ({ {"000", "0_0", "000"}, { {BT_WOOD, 0x10} }, false, {BT_CHEST, 0, 1} });
		this->shape_recipes.push_back ({ {"000", "0_0", "000"}, { {BT_COBBLE, 0x10} }, false, {BT_FURNACE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "00_", "___"}, { {BT_WOOD, 0x10} }, false, {BT_WORKBENCH, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "1__", "___"}, { {IT_COAL, 0x10}, {IT_STICK, 0x10} }, false, {BT_TORCH, 0, 4} });
		
		// slabs
		this->shape_recipes.push_back ({ {"000", "___", "___"}, { {BT_WOOD, 0} }, false, {BT_WOODEN_SLAB, 0, 6}});
		this->shape_recipes.push_back ({ {"000", "___", "___"}, { {BT_WOOD, 1} }, false, {BT_WOODEN_SLAB, 1, 6}});
		this->shape_recipes.push_back ({ {"000", "___", "___"}, { {BT_WOOD, 2} }, false, {BT_WOODEN_SLAB, 2, 6}});
		this->shape_recipes.push_back ({ {"000", "___", "___"}, { {BT_WOOD, 3} }, false, {BT_WOODEN_SLAB, 3, 6}});
		
		// pickaxes
		this->shape_recipes.push_back ({ {"000", "_1_", "_1_"}, { {BT_WOOD, 0x10} , {IT_STICK, 0x10} }, false, {IT_WOODEN_PICKAXE, 0, 1} });
		this->shape_recipes.push_back ({ {"000", "_1_", "_1_"}, { {BT_COBBLE, 0x10} , {IT_STICK, 0x10} }, false, {IT_STONE_PICKAXE, 0, 1} });
		this->shape_recipes.push_back ({ {"000", "_1_", "_1_"}, { {IT_IRON_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_IRON_PICKAXE, 0, 1} });
		this->shape_recipes.push_back ({ {"000", "_1_", "_1_"}, { {IT_GOLD_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_GOLDEN_PICKAXE, 0, 1} });
		this->shape_recipes.push_back ({ {"000", "_1_", "_1_"}, { {IT_DIAMOND, 0x10} , {IT_STICK, 0x10} }, false, {IT_DIAMOND_PICKAXE, 0, 1} });
		
		// axes
		this->shape_recipes.push_back ({ {"00_", "01_", "_1_"}, { {BT_WOOD, 0x10} , {IT_STICK, 0x10} }, false, {IT_WOODEN_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "01_", "_1_"}, { {BT_COBBLE, 0x10} , {IT_STICK, 0x10} }, false, {IT_STONE_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "01_", "_1_"}, { {IT_IRON_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_IRON_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "01_", "_1_"}, { {IT_GOLD_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_GOLDEN_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "01_", "_1_"}, { {IT_DIAMOND, 0x10} , {IT_STICK, 0x10} }, false, {IT_DIAMOND_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_10", "_1_"}, { {BT_WOOD, 0x10} , {IT_STICK, 0x10} }, false, {IT_WOODEN_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_10", "_1_"}, { {BT_COBBLE, 0x10} , {IT_STICK, 0x10} }, false, {IT_STONE_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_10", "_1_"}, { {IT_IRON_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_IRON_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_10", "_1_"}, { {IT_GOLD_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_GOLDEN_AXE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_10", "_1_"}, { {IT_DIAMOND, 0x10} , {IT_STICK, 0x10} }, false, {IT_DIAMOND_AXE, 0, 1} });
		
		// shovels
		this->shape_recipes.push_back ({ {"0__", "1__", "1__"}, { {BT_WOOD, 0x10} , {IT_STICK, 0x10} }, false, {IT_WOODEN_SHOVEL, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "1__", "1__"}, { {BT_COBBLE, 0x10} , {IT_STICK, 0x10} }, false, {IT_STONE_SHOVEL, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "1__", "1__"}, { {IT_IRON_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_IRON_SHOVEL, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "1__", "1__"}, { {IT_GOLD_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_GOLDEN_SHOVEL, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "1__", "1__"}, { {IT_DIAMOND, 0x10} , {IT_STICK, 0x10} }, false, {IT_DIAMOND_SHOVEL, 0, 1} });
		
		// hoes
		this->shape_recipes.push_back ({ {"00_", "_1_", "_1_"}, { {BT_WOOD, 0x10} , {IT_STICK, 0x10} }, false, {IT_WOODEN_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "_1_", "_1_"}, { {BT_COBBLE, 0x10} , {IT_STICK, 0x10} }, false, {IT_STONE_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "_1_", "_1_"}, { {IT_IRON_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_IRON_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "_1_", "_1_"}, { {IT_GOLD_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_GOLDEN_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"00_", "_1_", "_1_"}, { {IT_DIAMOND, 0x10} , {IT_STICK, 0x10} }, false, {IT_DIAMOND_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_1_", "_1_"}, { {BT_WOOD, 0x10} , {IT_STICK, 0x10} }, false, {IT_WOODEN_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_1_", "_1_"}, { {BT_COBBLE, 0x10} , {IT_STICK, 0x10} }, false, {IT_STONE_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_1_", "_1_"}, { {IT_IRON_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_IRON_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_1_", "_1_"}, { {IT_GOLD_INGOT, 0x10} , {IT_STICK, 0x10} }, false, {IT_GOLDEN_HOE, 0, 1} });
		this->shape_recipes.push_back ({ {"_00", "_1_", "_1_"}, { {IT_DIAMOND, 0x10} , {IT_STICK, 0x10} }, false, {IT_DIAMOND_HOE, 0, 1} });
		
		// swords
		this->shape_recipes.push_back ({ {"0__", "0__", "1__"}, { {BT_WOOD, 0x10}, {IT_STICK, 0x10} }, false, {IT_WOODEN_SWORD, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "0__", "1__"}, { {BT_COBBLE, 0x10}, {IT_STICK, 0x10} }, false, {IT_STONE_SWORD, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "0__", "1__"}, { {IT_IRON_INGOT, 0x10}, {IT_STICK, 0x10} }, false, {IT_IRON_SWORD, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "0__", "1__"}, { {IT_GOLD_INGOT, 0x10}, {IT_STICK, 0x10} }, false, {IT_GOLDEN_SWORD, 0, 1} });
		this->shape_recipes.push_back ({ {"0__", "0__", "1__"}, { {IT_DIAMOND, 0x10}, {IT_STICK, 0x10} }, false, {IT_DIAMOND_SWORD, 0, 1} });
		
		// trunk to wooden planks
		this->shapeless_recipes.push_back ({ {"0__", "___", "___"}, { {BT_TRUNK, 0} }, true, {BT_WOOD, 0, 4} });
		this->shapeless_recipes.push_back ({ {"0__", "___", "___"}, { {BT_TRUNK, 1} }, true, {BT_WOOD, 1, 4} });
		this->shapeless_recipes.push_back ({ {"0__", "___", "___"}, { {BT_TRUNK, 2} }, true, {BT_WOOD, 2, 4} });
		this->shapeless_recipes.push_back ({ {"0__", "___", "___"}, { {BT_TRUNK, 3} }, true, {BT_WOOD, 3, 4} });
	}
	
	
	
	static int
	_material_count (crafting_recipe& recipe)
	{
		int highest = -1;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				{
					if (std::isdigit (recipe.recipe[i][j]) && (recipe.recipe[i][j] > (highest + '0')))
						highest = recipe.recipe[i][j] - '0';
				}
		
		return highest + 1;
	}
	
	static int
	_material_count (slot_item **table)
	{
		int count = 0;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				{
					if (table[i][j].empty ())
						continue;
					
					bool found = false;
					for (int x = 0; x <= i; ++x)
						for (int y = 0; y < ((x < i) ? 3 : j); ++y)
							if (table[x][y].id () == table[i][j].id () &&
									table[x][y].damage () == table[i][j].damage ())
								{
									found = true;
									break;
								} 
					
					if (!found)
						++ count;
				}
		
		return count;
	}
	
	static int
	_count_type (crafting_recipe& recipe, int m)
	{
		int count = 0;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				if (recipe.recipe[i][j] == (m + '0'))
					++ count;
		
		return count;
	}
	
	static bool
	_shapeless_match (slot_item **table, crafting_recipe& recipe)
	{
		int m_count = _material_count (recipe);
		int t_count = _material_count (table);
		if (m_count != t_count)
			return false;
		
		// for every material, check how many times it occurs in both the crafting
		// recipe structure, and in the specified table.
		for (int i = 0; i < m_count; ++i)
			{
				blocki mat = recipe.materials[i];
				int count = _count_type (recipe, i);
				
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
	
	
	
	static int
	_recipe_width (crafting_recipe& recipe)
	{
		int max = 0;
		for (int i = 0; i < 3; ++i)
			{
				int w = 0, last = -1;
				for (int j = 0; j < 3; ++j)
					if (recipe.recipe[i][j] != '_')
						{
							w += j - last;
							last = j;
						}
				
				if (w > max)
					max = w;
			}
		
		return max;
	}
	
	static int
	_recipe_height (crafting_recipe& recipe)
	{
		int max = 0;
		for (int j = 0; j < 3; ++j)
			{
				int w = 0, last = -1;
				for (int i = 0; i < 3; ++i)
					if (recipe.recipe[i][j] != '_')
						{
							w += i - last;
							last = i;
						}
				
				if (w > max)
					max = w;
			}
		
		return max;
	}
	
	static bool
	_shape_match (slot_item **table, crafting_recipe& recipe)
	{
		int r_width = _recipe_width (recipe);
		int r_height = _recipe_height (recipe);
		
		for (int i = 0; i < (4 - r_height); ++i)
			for (int j = 0; j < (4 - r_width); ++j)
				{
					bool match = true;
					for (int y = 0; y < r_height; ++y)
						for (int x = 0; x < r_width; ++x)
							{
								blocki mat = { BT_AIR, 0 };
								if (std::isdigit (recipe.recipe[y][x]))
									mat = recipe.materials[recipe.recipe[y][x] - '0'];
								
								if ((mat.id == BT_AIR && !table[i + y][j + x].empty ()) ||
										!(table[i + y][j + x].id () == mat.id && ((mat.meta == 0x10) || (table[i + y][j + x].damage () == mat.meta))))
									{
										match = false;
										break;
									}
							}
										
					if (match)
						{
							// make sure all other squares are clean
							bool clean = true;
							for (int y = 0; y < 3; ++y)
								for (int x = 0; x < 3; ++x)
									if (!((y >= i && y < (i + r_height)) && (x >= j && x < (j + r_width))))
										{
											if (!table[y][x].empty ())
												{
													clean = false;
													goto for_end;
												}
										}
						for_end:
							if (clean)
								return true;
						}
				}
		
		return false;
	}
	
	
	
	/* 
	 * Attempts to match the given 3x3 2D-array of blocks with a crafting recipe.
	 * On success, the desired recipe is returned; otherwise, nullptr is returned.
	 */
	const crafting_recipe*
	crafting_manager::match (slot_item **table)
	{
		// try recipes with a shape first
		for (crafting_recipe& res : this->shape_recipes)
			if (_shape_match (table, res))
				return &res;
		
		// and then shapeless recipes
		for (crafting_recipe& res : this->shapeless_recipes)
			if (_shapeless_match (table, res))
				return &res;
		
		return nullptr;
	}
}

