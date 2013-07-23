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

#ifndef _hCraft__WORLD_LIST_H_
#define _hCraft__WORLD_LIST_H_

#include "cistring.hpp"
#include <unordered_map>
#include <mutex>
#include <vector>


namespace hCraft {

	class world;	
	
	
	enum class world_find_method
	{
		case_sensitive,
		case_insensitive,
		name_completion,
	};
	
	
	
	/* 
	 * Thread-safe world list.
	 */
	class world_list
	{
		std::unordered_map<cistring, world *> worlds;
		std::mutex lock;
		
	public:
		world_list ();
		world_list (const world_list& other);
		~world_list ();
		
		
		
		/* 
		 * Adds the specified world into the world list.
		 * Returns false if a world with the same name already exists in the list;
		 * true otherwise.
		 */
		bool add (world *w);
		
		/* 
		 * Removes the world that has the specified name from this world list.
		 * NOTE: the search is done using case-INsensitive comparison.
		 */
		void remove (const char *name, bool delete_world = false);
		
		/* 
		 * Removes the specified world from this world list.
		 */
		void remove (world *w, bool delete_world = false);
		
		/* 
		 * Removes all worlds from this world list.
		 */
		void clear (bool delete_worlds = false);
		
		
		
		/* 
		 * Searches the world list for a world that has the specified name.
		 * Uses the given method to determine if names match.
		 */
		world* find (const char *name,
			world_find_method method = world_find_method::case_insensitive);
		
		
		
		/* 
		 * Calls the function @{f} on all worlds in this list.
		 */
		void all (std::function<void (world *)> f);
		
		
		
		/* 
		 * Inserts all worlds in this list into vector @{vec}.
		 */
		void populate (std::vector<world *>& vec);
	};
}

#endif

