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

#include "slot/items.hpp"
#include <unordered_map>
#include <cstring>
#include <vector>


namespace hCraft {
	
	item_info::item_info ()
	{
		this->id = IT_UNKNOWN;
		this->durability = 0;
		this->max_stack = 64;
		std::strcpy (this->name, "unknown");
	}
	
	item_info::item_info (unsigned short id, const char *name,
		unsigned short durability, char max_stack)
	{
		this->id = id;
		this->durability = durability;
		this->max_stack = max_stack;
		std::strcpy (this->name, name);
	}
	
	
//----
	
	static std::vector<item_info> item_list {
		{ 0x100, "iron-shovel", 251, 1 },
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{ 0x120, "feather", 0, 64 },
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{},
		{ 0x143, "sign", 0, 16 },
	};
	
	static std::unordered_map<unsigned short, food_info> food_list {
		{ IT_APPLE, { 4, 2.4f } },
		{ IT_BAKED_POTATO, { 6, 7.2f } },
		{ IT_BREAD, { 5, 6.0f } },
		{ IT_CAKE, { 2, 0.4f } },
		{ IT_CARROT, { 4, 4.8f } },
		{ IT_COOKED_CHICKEN, { 6, 7.2f } },
		{ IT_COOKED_FISH, { 5, 6.0f } },
		{ IT_COOKED_PORKCHOP, { 8, 12.8f } }, 
		{ IT_COOKIE, { 2, 0.4f } },
		{ IT_GOLDEN_APPLE, { 4, 9.6f } },
		{ IT_GOLDEN_CARROT, { 6, 14.4f } },
		{ IT_MELON_SLICE, { 2, 1.2f } },
		{ IT_MUSHROOM_STEW, { 6, 7.2f } },
		{ IT_POISONOUS_POTATO, { 2, 1.2f } },
		{ IT_POTATO, { 1, 0.6f } },
		{ IT_PUMPKIN_PIE, { 8, 4.8f } },
		{ IT_RAW_BEEF, { 3, 1.8f } },
		{ IT_RAW_CHICKEN, { 2, 1.2f } },
		{ IT_RAW_FISH, { 2, 1.2f } },
		{ IT_RAW_PORKCHOP, { 3, 1.8f } },
		{ IT_ROTTEN_FLESH, { 4, 0.8f } },
		{ IT_SPIDER_EYE, { 2, 3.2f } },
		{ IT_STEAK, { 8, 12.8f } },
	};
	
	
	
	/* 
	 * Returns the item_info structure describing the item associated with the
	 * specified ID number.
	 */
	item_info*
	item_info::from_id (unsigned short id)
	{
		if ((id - 0x100) >= (int)item_list.size ())
			return nullptr;
		return &item_list[id - 0x100];
	}
	
	
	
	/* 
	 * Returns an appropriate food_info structure which information about food
	 * aspects of the item with the specified ID.
	 */
	food_info
	item_info::get_food_info (unsigned short id)
	{
		auto itr = food_list.find (id);
		if (itr == food_list.end ())
			return {};
		return itr->second;
	}
	
	
	
	/* 
	 * Checks whether the item associated with the given ID is a tool.
	 * (Anything that has a durability bar is considered a tool (except armor)).
	 */
	bool
	item_info::is_tool (unsigned short id)
	{
		switch (id)
			{
				case IT_WOODEN_AXE:
				case IT_STONE_AXE:
				case IT_IRON_AXE:
				case IT_GOLDEN_AXE:
				case IT_DIAMOND_AXE:
				
				case IT_WOODEN_PICKAXE:
				case IT_STONE_PICKAXE:
				case IT_IRON_PICKAXE:
				case IT_GOLDEN_PICKAXE:
				case IT_DIAMOND_PICKAXE:
				
				case IT_WOODEN_SHOVEL:
				case IT_STONE_SHOVEL:
				case IT_IRON_SHOVEL:
				case IT_GOLDEN_SHOVEL:
				case IT_DIAMOND_SHOVEL:
				
				case IT_WOODEN_HOE:
				case IT_STONE_HOE:
				case IT_IRON_HOE:
				case IT_GOLDEN_HOE:
				case IT_DIAMOND_HOE:
				
				case IT_FLINT_AND_STEEL:
				case IT_FISHING_ROD:
				case IT_CARROT_ON_A_STICK:
				case IT_SHEARS:
					return true;
				
				default:
					return false;
			}
	}
	
	/* 
	 * Checks whether the specified ID is that of an armor piece.
	 */
	bool
	item_info::is_armor (unsigned short id)
	{
		return (id >= 0x12A && id <= 0x13D);
	}
}

