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

#include "items.hpp"
#include <cstring>
#include <vector>


namespace hCraft {
	
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
	};
	
	
	
	/* 
	 * Returns the item_info structure describing the item associated with the
	 * specified ID number.
	 */
	item_info*
	item_info::from_id (unsigned short id)
	{
		if (id >= item_list.size ())
			return nullptr;
		return &item_list[id];
	}
}

