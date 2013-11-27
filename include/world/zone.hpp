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

#ifndef _hCraft__ZONE_H_
#define _hCraft__ZONE_H_

#include "drawing/selection/world_selection.hpp"
#include "system/security.hpp"
#include "util/position.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <ctime>
#include <mutex>


namespace hCraft {
	
	/* 
	 * Adds more zone-specific functionality.
	 */
	class zone_security: public ownership_security
	{
		std::string ps_build;
		std::string ps_enter;
		std::string ps_leave;
		
	public:
		inline const std::string& get_build_perms () const { return this->ps_build; }
		inline const std::string& get_enter_perms () const { return this->ps_enter; }
		inline const std::string& get_leave_perms () const { return this->ps_leave; }
		
		void set_build_perms (const std::string& str);
		void set_enter_perms (const std::string& str);
		void set_leave_perms (const std::string& str);
		
	public:
		/* 
		 * Returns true if the specified player is allowed to place/destroy blocks
		 * in the zone.
		 */
		bool can_build (player *pl);
		
		/* 
		 * Returns true if the specified player can enter the zone.
		 */
		bool can_enter (player *pl);
		
		/* 
		 * Returns true if the specified player can leave the zone.
		 */
		bool can_leave (player *pl);
	};
	
	
	
	/* 
	 * A zone is essentially a world selection with some additional information
	 * (such as who can build in the zone, enter it, etc...)
	 */
	class zone
	{
		std::string name;
		world_selection *sel;
		zone_security sec;
		
		int creator_pid;
		std::time_t created_time;
		
		std::string enter_msg, leave_msg;
		
	public:
		inline const std::string& get_name () const { return this->name; }
		inline world_selection* get_selection () { return this->sel; }
		inline zone_security& get_security () { return this->sec; }
		
		inline const std::string& get_enter_msg () const { return this->enter_msg; }
		inline const std::string& get_leave_msg () const { return this->leave_msg; }
		
		void set_enter_msg (const std::string& msg) { this->enter_msg = msg; }
		void set_leave_msg (const std::string& msg) { this->leave_msg = msg; }
		
	public:
		/* 
		 * Constructs a new zone from the specified world selection.
		 */
		zone (const std::string& name, world_selection *sel);
		
		/* 
		 * Constructs a new zone as a copy of the specified zone with the given
		 * selection.
		 */
		zone (zone *other, world_selection *sel);
		
		/* 
		 * Destructor.
		 */
		~zone ();
	};
	
	
	
	namespace internal {
		
		/* 
		 * A 16x64x16 block of zones.
		 */
		struct zone_block
		{
			std::vector<zone *> zones;
		};
	}
	
	class zone_manager
	{
		std::vector<zone *> zones;
		std::unordered_map<chunk_pos, internal::zone_block *, chunk_pos_hash> blocks[4]; // 4x64 = 256
		std::mutex zone_lock;
		
	public:
		inline const std::vector<zone *>& get_all () { return this->zones; }
		
	public:
		/* 
		 * Destructor - destroys all zones owned by the manager.
		 */
		~zone_manager ();
		
		
		
		/* 
		 * Inserts the specified zone into the manager's zone list.
		 */
		void add (zone *z);
		
		/* 
		 * Removes the specified zone from the manager's zone list.
		 */
		void remove (zone *z);
		
		/* 
		 * Fills the specified vector with all zones that contain the specified
		 * point.
		 */
		void find (int x, int y, int z, std::vector<zone *>& out);
		
		/* 
		 * Finds and returns a zone that has the specified name.
		 * Returns null if not found.
		 */
		zone* find (const std::string& name);
	};
}

#endif

