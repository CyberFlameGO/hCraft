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

#ifndef _hCraft__PERMISSIONS_H_
#define _hCraft__PERMISSIONS_H_

#include <unordered_map>
#include <string>
#include <functional>
#include <vector>


namespace hCraft {

#define PERM_INVALID    0xFFFFU	
#define PERM_WILD_ALL		1
#define PERM_ID_START		3


#define PERM_NODE_COUNT 6

	/* 
	 * Represents a permission node (<comp1>.<comp2>. ... .<compN>) in a compact
	 * form.
	 */
	struct permission
	{
		unsigned short nodes[PERM_NODE_COUNT]; // 6 nodes max
		bool  neg;
		
	//----
		permission (bool negated = false)
		{
			for (int i = 0; i < PERM_NODE_COUNT; ++i)
				this->nodes[i] = -1;
			this->neg = negated;
		}
		
		inline bool valid () { return this->nodes[0] != -1; }
		
		inline bool
		operator== (const permission& other) const
		{
			for (int i = 0; i < PERM_NODE_COUNT; ++i)
				if (this->nodes[i] != other.nodes[i])
					return false;
			return true;
		}
	};
	
	
	/* 
	 * Manages a collection of permissions nodes.
	 */
	class permission_manager
	{
		std::unordered_map<std::string, int> id_maps[PERM_NODE_COUNT];
		std::vector<std::string> name_maps[PERM_NODE_COUNT];
		
	public:
		/* 
		 * Class constructor.
		 */
		permission_manager ();
		
		
		
		/* 
		 * Registers the specified permission with the manager and returns the
		 * associated permission structure.
		 */
		permission add (const char *perm);
		
		/* 
		 * Returns the permission structure associated with the given string.
		 * If the string does not name a valid permission node, the return value
		 * of the structure's `valid ()' member function will be false.
		 */
		permission get (const char *perm) const;
		
		/* 
		 * Returns a human-readable representation of the given permission node.
		 */
		std::string to_string (permission perm) const;
	};
}


namespace std {
	
	template<>
	class hash<hCraft::permission>
	{
		std::hash<int> int_hash;
		
	public:
		size_t
		operator() (const hCraft::permission& perm) const
		{
			int s;
			for (int i = 0; i < PERM_NODE_COUNT; ++i)
				s += int_hash (perm.nodes[i] * (128 * (i + 1)));
			return int_hash (s);
		}
	};
}

#endif

