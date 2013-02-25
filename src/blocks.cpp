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

#include "blocks.hpp"
#include "cistring.hpp"
#include <cstring>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cctype>


namespace hCraft {
	
	block_info::block_info ()
	{
		this->id = 0;
		this->blast_resistance = 0.0f;
		this->opacity = 0;
		this->luminance = 0;
		this->max_stack = 64;
		this->opaque = false;
		this->state = BS_NONSOLID;
		std::strcpy (this->name, "unknown");
	}
	
	block_info::block_info (unsigned short id, const char *name,
		float blast_resistance, char opacity, char luminance, char max_stack,
		bool opaque, block_state state)
	{
		this->id = id;
		this->blast_resistance = blast_resistance;
		this->opacity = opacity;
		this->luminance = luminance;
		this->max_stack = max_stack;
		this->opaque = opaque;
		this->state = state;
		std::strcpy (this->name, name);
	}
	
	
//----
	
	static std::vector<block_info> block_list {
		{ 0x00, "air", 0.0f, 0, 0, 64,  false, BS_NONSOLID },
		{ 0x01, "stone", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x02, "grass", 3.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x03, "dirt", 2.5f, 15, 0, 64, true, BS_SOLID },
		{ 0x04, "cobble", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x05, "wood", 15.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x06, "sapling", 0.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x07, "bedrock", 18000000.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x08, "water", 500.0f, 3, 0, 64, true, BS_FLUID },
		{ 0x09, "still-water", 500.0f, 3, 0, 64, true, BS_FLUID },
		{ 0x0A, "lava", 0.0f, 0, 15, 64, false, BS_FLUID },
		{ 0x0B, "still-lava", 500.0f, 0, 15, 64, false, BS_FLUID },
		{ 0x0C, "sand", 2.5f, 15, 0, 64, true, BS_SOLID },
		{ 0x0D, "gravel", 3.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x0E, "gold-ore", 15.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x0F, "iron-ore", 15.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x10, "coal-ore", 15.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x11, "trunk", 10.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x12, "leaves", 1.0f, 1, 0, 64, true, BS_SOLID },
		{ 0x13, "sponge", 3.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x14, "glass", 1.5f, 0, 0, 64, true, BS_SOLID },
		{ 0x15, "lapis-ore", 15.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x16, "lapis-block", 15.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x17, "dispenser", 17.5f, 15, 0, 64, true, BS_SOLID },
		{ 0x18, "sandstone", 4.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x19, "note-block", 4.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x1A, "bed", 1.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x1B, "powered-rail", 3.5f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x1C, "detector-rail", 3.5f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x1D, "sticky-piston", 2.5f, 0, 0, 64, true, BS_NONSOLID },
		{ 0x1E, "cobweb", 20.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x1F, "tall-grass", 0.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x20, "dead-bush", 0.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x21, "piston", 2.5f, 0, 0, 64, true, BS_SOLID },
		{ 0x22, "piston-extension", 2.5f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x23, "wool", 4.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x24, "piston-move", 0.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x25, "dandelion", 0.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x26, "rose", 0.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x27, "brown-mushroom", 0.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x28, "red-mushroom", 0.0f, 0, 0, 64, false, BS_NONSOLID },
		{ 0x29, "gold-block", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x2A, "iron-block", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x2B, "dslab", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x2C, "slab", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x2D, "bricks", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x2E, "tnt", 0.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x2F, "bookshelf", 7.5f, 15, 0, 64, true, BS_SOLID },
		{ 0x30, "mossy-cobble", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x31, "obsidian", 6000.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x32, "torch", 0.0f, 0, 14, 64, false, BS_NONSOLID },
		{ 0x33, "fire", 0.0f, 0, 15, 64, false, BS_NONSOLID },
		{ 0x34, "monster-spawner", 25.0f, 15, 0, 64, false, BS_SOLID },
		{ 0x35, "stairs", 30.0f, 15, 0, 64, true, BS_SOLID },
		{ 0x36, "chest", 12.5f, 15, 0, 64, false, BS_NONSOLID },
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
		{ 0x59, "glowstone", 1.5f, 15, 15, 64, true, BS_SOLID },
	};
	
	static std::unordered_map<cistring, int> name_map {
		{ "air", 0x00 },
		{ "stone", 0x01 },
		{ "grass", 0x02 },
		{ "dirt", 0x03 },
		{ "cobble", 0x04 },
		{ "wood", 0x05 },
		{ "sapling", 0x06 },
		{ "bedrock", 0x07 },
		{ "water", 0x08 },
		{ "still-water", 0x09 },
		{ "lava", 0x0A },
		{ "still-lava", 0x0B },
		{ "sand", 0x0C },
		{ "gravel", 0x0D },
		{ "gold-ore", 0x0E },
		{ "iron-ore", 0x0F },
		{ "coal-ore", 0x10 },
		{ "trunk", 0x11 },
		{ "leaves", 0x12 },
		{ "sponge", 0x13 },
		{ "glass", 0x14 },
		{ "lapis-ore", 0x15 },
		{ "lapis-block", 0x16 },
		{ "dispenser", 0x17 },
		{ "sandstone", 0x18 },
		{ "note-block", 0x19 },
		{ "bed", 0x1A },
		{ "powered-rail", 0x1B },
		{ "detector-rail", 0x1C },
		{ "sticky-piston", 0x1D },
		{ "cobweb", 0x1E },
		{ "tall-grass", 0x1F },
		{ "dead-bush", 0x20 },
		{ "piston", 0x21 },
		{ "piston-extension", 0x22 },
		{ "wool", 0x23 },
		{ "piston-move", 0x24 },
		{ "dandelion", 0x25 },
		{ "rose", 0x26 },
		{ "brown-mushroom", 0x27 },
		{ "red-mushroom", 0x28 },
		{ "gold-block", 0x29 },
		{ "iron-block", 0x2A },
		{ "dslab", 0x2B },
		{ "slab", 0x2C },
		{ "bricks", 0x2D },
		{ "tnt", 0x2E },
		{ "bookshelf", 0x2F },
		{ "mossy-cobble", 0x30 },
		{ "obsidian", 0x31 },
		{ "torch", 0x32 },
		{ "fire", 0x33 },
		{ "monster-spawner", 0x34 },
		{ "stairs", 0x35 },
		{ "chest", 0x36 },
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
		{ "glowstone", 0x59 },
	};
	
	
	
	/* 
	 * Returns the block_info structure describing the block associated with the
	 * specified ID number.
	 */
	block_info*
	block_info::from_id (unsigned short id)
	{
		if (id >= block_list.size ())
			return nullptr;
		return &block_list[id];
	}
	
	/* 
	 * Returns the block info structure that corresponds with the given block
	 * name.
	 */
	block_info*
	block_info::from_name (const char *name)
	{
		auto itr = name_map.find (name);
		if (itr == name_map.end ())
			return nullptr;
		return block_info::from_id (itr->second);
	}
	
	/* 
	 * Attempts to find the block info structure associated with the specified
	 * string. Inputs such as "cobble" and "4" will both return the same result.
	 */
	block_info*
	block_info::from_id_or_name (const char *str)
	{
		// check if number
		bool is_num = true;
		const char *ptr = str;
		while (*ptr)
			if (!std::isdigit (*ptr++))
				{ is_num = false; break; }
		
		if (is_num)
			{
				std::istringstream ss (str);
				int num;
				ss >> num;
				return block_info::from_id (num);
			}
		return block_info::from_name (str);
	}
}

