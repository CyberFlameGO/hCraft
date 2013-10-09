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

#include "world/world_list.hpp"
#include "world/world.hpp"


namespace hCraft {
	
	world_list::world_list ()
		{ }
	world_list::world_list (const world_list& other)
		: worlds (other.worlds), lock ()
		{ }
	world_list::~world_list ()
		{ }
	
	
	
	/* 
	 * Returns the number of worlds contianed in this list.
	 */
	int
	world_list::count ()
	{
		std::lock_guard<std::mutex> guard {this->lock};
		return this->worlds.size ();
	}
	
	
	
	/* 
	 * Adds the specified world into the world list.
	 * Returns false if a world with the same name already exists in the list;
	 * true otherwise.
	 */
	bool
	world_list::add (world *w)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		
		auto itr = this->worlds.find (w->get_name ());
		if (itr != this->worlds.end ())
			return false;
		
		this->worlds[w->get_name ()] = w;
		return true;
	}
	
	/* 
	 * Removes the world that has the specified name from this world list.
	 * NOTE: the search is done using case-INsensitive comparison.
	 */
	void
	world_list::remove (const char *name, bool delete_world)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		auto itr = this->worlds.find (name);
		if (itr == this->worlds.end ())
			return;
		
		if (delete_world)
			delete (world *)itr->second;
		this->worlds.erase (itr);
	}
	
	/* 
	 * Removes the specified world from this world list.
	 */
	void
	world_list::remove (world *w, bool delete_world)
	{
		this->remove (w->get_name (), delete_world);
	}
	
	/* 
	 * Removes all worlds from this world list.
	 */
	void
	world_list::clear (bool delete_worlds)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		
		if (delete_worlds)
			for (auto itr = this->worlds.begin (); itr != this->worlds.end (); ++itr)
				delete (world *)itr->second; 
		
		this->worlds.clear ();
	}
	
	
	
	/* 
	 * Searches the world list for a world that has the specified name.
	 * Uses the given method to determine if names match.
	 */
	world*
	world_list::find (const char *name, world_find_method method)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		
		switch (method)
			{
				case world_find_method::case_sensitive:
					{
						auto itr = this->worlds.find (name);
						if (itr != this->worlds.end ())
							{
								// do another comparison, this time with case sensitivity.
								if (std::strcmp (itr->first.c_str (), name) == 0)
									return itr->second;
							}
						
						return nullptr;
					}
				
				case world_find_method::case_insensitive:
					{
						auto itr = this->worlds.find (name);
						if (itr != this->worlds.end ())
							return itr->second;
						
						return nullptr;
					}
				
				case world_find_method::name_completion:
					{
						// TODO
						return nullptr;
					}
			}
		
		// never reached.
		return nullptr;
	}
	
	
	
	/* 
	 * Calls the function @{f} on all worlds in this list.
	 */
	void
	world_list::all (std::function<void (world *)> f)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		for (auto itr = this->worlds.begin (); itr != this->worlds.end (); ++itr)
			f (itr->second);
	}
	
	
	
	/* 
	 * Inserts all worlds in this list into vector @{vec}.
	 */
	void
	world_list::populate (std::vector<world *>& vec)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		for (auto itr = this->worlds.begin (); itr != this->worlds.end (); ++itr)
			vec.push_back (itr->second);
	}
}

