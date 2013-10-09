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

#include "player/permissions.hpp"
#include <cstring>


namespace hCraft {
	
	/* 
	 * Class constructor.
	 */
	permission_manager::permission_manager ()
	{
	}
	
	
	
	static int
	get_wildcard (const char *str)
	{
		switch (*str)
			{
				case '*': return PERM_WILD_ALL;
			}
		return PERM_INVALID;
	}
	
	/* 
	 * Registers the specified permission with the manager and returns the
	 * associated permission structure.
	 */
	permission
	permission_manager::add (const char *perm)
	{
		const char *ptr = perm;
		char tmp[33];
		
		permission result {};
		
		if (*ptr == '-')
			{
				result.neg = true;
				++ ptr;
			}
		else
			result.neg = false;
		
		int depth = 0;
		for ( ; *ptr != '\0' && depth < PERM_NODE_COUNT; ++ depth)
			{
				std::unordered_map<std::string, int>& id_map = this->id_maps[depth];
				std::vector<std::string>& name_map = this->name_maps[depth];
				
				const char *dot = std::strchr (ptr, '.');
				if (dot)
					{
						std::memcpy (tmp, ptr, dot - ptr);
						tmp[dot - ptr] = '\0';
						ptr = dot + 1;
					}
				else
					{
						int len = std::strlen (ptr);
						if (len > 32)
							{
								std::memcpy (tmp, ptr, 32);
								tmp[32] = '\0';
							}
						else
							std::strcpy (tmp, ptr);
						ptr += len;
					}
				
				// test for wildcards
				int wc = get_wildcard (tmp);
				if (wc != PERM_INVALID)
					{
						result.nodes[depth] = wc;
						continue;
					}
				
				auto itr = id_map.find (tmp);
				if (itr != id_map.end ())
					{
						result.nodes[depth] = itr->second;
						continue;
					}
				
				// register new node;
				int id = name_map.size () + PERM_ID_START;
				name_map.push_back (tmp);
				id_map[tmp] = id;
				result.nodes[depth] = id;
			}
		
		for (; depth < PERM_NODE_COUNT; ++depth)
			result.nodes[depth] = 0;
		
		return result;
	}
	
	/* 
	 * Returns the permission structure associated with the given string.
	 * If the string does not name a valid permission node, the return value
	 * of the structure's `valid ()' member function will be false.
	 */
	permission
	permission_manager::get (const char *perm) const
	{
		const char *ptr = perm;
		char tmp[33];
		
		permission result {};
		
		int depth = 0;
		for ( ; *ptr != '\0' && depth < PERM_NODE_COUNT; ++ depth)
			{
				const std::unordered_map<std::string, int>& id_map = this->id_maps[depth];
				
				const char *dot = std::strchr (ptr, '.');
				if (dot)
					{
						std::memcpy (tmp, ptr, dot - ptr);
						tmp[dot - ptr] = '\0';
						ptr = dot + 1;
					}
				else
					{
						int len = std::strlen (ptr);
						if (len > 32)
							{
								std::memcpy (tmp, ptr, 32);
								tmp[32] = '\0';
							}
						else
							std::strcpy (tmp, ptr);
						ptr += len;
					}
				
				// test for wildcards
				int wc = get_wildcard (tmp);
				if (wc != PERM_INVALID)
					{
						result.nodes[depth] = wc;
						continue;
					}
				
				auto itr = id_map.find (tmp);
				if (itr == id_map.end ())
					{
						result.nodes[0] = PERM_INVALID;
						return result;
					}
				
				result.nodes[depth] = itr->second;
			}
		
		for (; depth < PERM_NODE_COUNT; ++depth)
			result.nodes[depth] = 0;
		
		return result;
	}
	
	
	
	/* 
	 * Returns a human-readable representation of the given permission node.
	 */
	std::string
	permission_manager::to_string (permission perm) const
	{
		if (perm.nodes[0] == PERM_INVALID)
			return "unknown";
		
		std::string str;
		if (perm.neg)
			str.push_back ('-');
		
		for (int i = 0; i < PERM_NODE_COUNT; ++i)
			{
				int node = perm.nodes[i];
				if (node == 0)
					break;
				else if (node == PERM_INVALID)
					return "unknown";
				
				if (i > 0)
					str.push_back ('.');
				
				if (node == PERM_WILD_ALL)
					str.push_back ('*');
				else
					{
						str.append (this->name_maps[i][node - PERM_ID_START]);
					}
			}
		
		return str;
	}
}

