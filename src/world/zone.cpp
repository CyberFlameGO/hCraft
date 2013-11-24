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

#include "world/zone.hpp"

#include <iostream> // DEBUG


namespace hCraft {

	void
	zone_security::set_build_perms (const std::string& str)
		{ this->ps_build = str; }
	
	void
	zone_security::set_enter_perms (const std::string& str)
		{ this->ps_enter = str; }
	
	void
	zone_security::set_leave_perms (const std::string& str)
		{ this->ps_leave = str; }
	
	
	
	/* 
	 * Returns true if the specified player is allowed to place/destroy blocks
	 * in the zone.
	 */
	bool
	zone_security::can_build (player *pl)
	{
		return this->is_owner (pl) || this->is_member (pl)
			|| basic_security::check_perm_str (pl, this->ps_build);
	}
	
	/* 
	 * Returns true if the specified player can enter the zone.
	 */
	bool
	zone_security::can_enter (player *pl)
	{
		return this->is_owner (pl) || this->is_member (pl)
			|| basic_security::check_perm_str (pl, this->ps_enter);
	}
	
	/* 
	 * Returns true if the specified player can leave the zone.
	 */
	bool
	zone_security::can_leave (player *pl)
	{
		return this->is_owner (pl) || this->is_member (pl)
			|| basic_security::check_perm_str (pl, this->ps_leave);
	}
	
	
	
//-------------------------------------
	
	/* 
	 * Constructs a new zone from the specified world selection.
	 */
	zone::zone (const std::string& name, world_selection *sel)
		: name (name)
	{
		this->sel = sel;
	}
	
	/* 
	 * Destructor.
	 */
	zone::~zone ()
	{
		delete this->sel;
	}
	
	
	
	/* 
	 * Destructor - destroys all zones owned by the manager.
	 */
	zone_manager::~zone_manager ()
	{
		std::lock_guard<std::mutex> guard {this->zone_lock};
		for (zone *z : this->zones)
			delete z;
		
		for (int i = 0; i < 4; ++i)
			for (auto itr = this->blocks[i].begin (); itr != this->blocks[i].end (); ++itr)
				delete itr->second;
	}
	
	
	
	static inline int
	_round_down (int num, int mul)
	{
		if ((num % mul) == 0)
			return num;
		if (num < 0)
			return num + (num % mul);
		else
			return num - (num % mul);
	}
	
	static inline int
	_round_up (int num, int mul)
	{
		if ((num % mul) == 0)
			return num;
		return num + mul - (num % mul);
	}
	
	/* 
	 * Inserts the specified zone into the manager's zone list.
	 */
	void
	zone_manager::add (zone *z)
	{
		std::lock_guard<std::mutex> guard {this->zone_lock};
		
		this->zones.push_back (z);
		
		world_selection *sel = z->get_selection ();
		block_pos min = sel->min ();
		block_pos max = sel->max ();
		
		/* 
		 * Round boundaries to nearest multiple of 16 (away from zero).
		 */
		min.x = _round_down (min.x, 16);
		min.y = _round_down (min.y, 64);
		min.z = _round_down (min.z, 16);
		max.x = _round_up (max.x, 16);
		max.y = _round_up (max.y, 64);
		max.z = _round_up (max.z, 16);
		
		if (min.y < 0) min.y = 0;
		else if (min.y > 255) min.y = 255;
		
		for (int xx = min.x; xx <= max.x; xx += 16)
			for (int zz = min.z; zz <= max.z; zz += 16)
				for (int yy = min.y; yy <= max.y; yy += 64)
					{
						int yi = yy >> 6;
						auto& blocks = this->blocks[yi];
						
						auto itr = blocks.find ({xx >> 4, zz >> 4});
						if (itr != blocks.end ())
							{
								itr->second->zones.push_back (z);
							}
						else
							{
								internal::zone_block *zb = blocks[{xx >> 4, zz >> 4}] = new internal::zone_block ();
								zb->zones.push_back (z);
							}
					}
	}
	
	/* 
	 * Removes the specified zone from the manager's zone list.
	 */
	void
	zone_manager::remove (zone *z)
	{
		std::lock_guard<std::mutex> guard {this->zone_lock};
		
		bool found = false;
		for (auto itr = this->zones.begin (); itr != this->zones.end (); ++itr)
			if (*itr == z)
				{
					this->zones.erase (itr);
					found = true;
					break;
				}
		
		if (!found)
			return;
		
		world_selection *sel = z->get_selection ();
		block_pos min = sel->min ();
		block_pos max = sel->max ();
		
		/* 
		 * Round boundaries to nearest multiple of 16 (away from zero).
		 */
		min.x = _round_down (min.x, 16);
		min.y = _round_down (min.y, 64);
		min.z = _round_down (min.z, 16);
		max.x = _round_up (max.x, 16);
		max.y = _round_up (max.y, 64);
		max.z = _round_up (max.z, 16);
		
		if (min.y < 0) min.y = 0;
		else if (min.y > 255) min.y = 255;
		
		for (int xx = min.x; xx <= max.x; xx += 16)
			for (int zz = min.z; zz <= max.z; zz += 16)
				for (int yy = min.y; yy <= max.y; yy += 64)
					{
						int yi = yy >> 6;
						auto& blocks = this->blocks[yi];
						
						auto itr = blocks.find ({xx >> 4, zz >> 4});
						if (itr != blocks.end ())
							{
								for (auto itr2 = itr->second->zones.begin (); itr2 != itr->second->zones.end (); ++itr2)
									if (*itr2 == z)
										{
											itr->second->zones.erase (itr2);
											break;
										}
							}
					}
	}
	
	/* 
	 * Fills the specified vector with all zones that contain the specified
	 * point.
	 */
	void
	zone_manager::find (int x, int y, int z, std::vector<zone *>& out)
	{
		std::lock_guard<std::mutex> guard {this->zone_lock};
		int cx = x >> 4;
		int cz = z >> 4;
		int cy = y >> 6;
		
		auto itr = this->blocks[cy].find ({cx, cz});
		if (itr != this->blocks[cy].end ())
			{
				internal::zone_block *zb = itr->second;
				for (zone *zn : zb->zones)
					if (zn->get_selection ()->contains (x, y, z))
						out.push_back (zn);
			}
	}
	
	/* 
	 * Finds and returns a zone that has the specified name.
	 * Returns null if not found.
	 */
	zone*
	zone_manager::find (const std::string& name)
	{
		std::lock_guard<std::mutex> guard {this->zone_lock};
		for (zone *zn : this->zones)
			if (zn->get_name () == name)
				return zn;
		return nullptr;
	}
}

