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

#ifndef _hCraft__WORLD_SECURITY_H_
#define _hCraft__WORLD_SECURITY_H_

#include <vector>
#include <string>


namespace hCraft {
	
	class player;
	
	
	/* 
	 * Manages world ownership, members, build&join permissions, etc...
	 */
	class world_security
	{
		// PID vectors
		std::vector<int> owners;
		std::vector<int> members;
		
		std::string build_perm_str;
		std::string join_perm_str;
		
	public:
		/* 
		 * Checks whether the specified access string returns a positive result
		 * for the given player.
		 * 
		 * Syntax:
		 *   term1|term2|term3|...|termN
		 * 
		 * Where a term is one of:
		 *   <group name>   : e.g. moderator or builder
		 *   ^<player name> : e.g. ^BizarreCake
		 *   *<permission>  : e.g. *command.admin.say
		 * 
		 * The vertical bars (|) denote OR.
		 * Terms may be negated by being preceeded with !
		 */
		static bool check_access_str (player *pl, const std::string& str);
		
		
	public:
		const std::string& get_build_perms () const;
		const std::string& get_join_perms () const;
		
		void set_build_perms (const std::string& str);
		void set_join_perms (const std::string& str);
		
		
		const std::vector<int>& get_owners () const { return this->owners; }
		const std::vector<int>& get_members () const { return this->members; }
		
	public:
		/* 
		 * Checks whether the specified player is a world owner.
		 */
		bool is_owner (player *pl) const;
		bool is_owner (int pid) const;
		
		/* 
		 * Checks whether the specified player is a world member.
		 */
		bool is_member (player *pl) const;
		bool is_member (int pid) const;
		
		
		
		/* 
		 * Adds the specified player to the world owner list.
		 */
		void add_owner (player *pl);
		void add_owner (int pid);
		
		/* 
		 * Adds the specified player to the world member list.
		 */
		void add_member (player *pl);
		void add_member (int pid);
		
		
		
		/* 
		 * Removes the specified player from the world owner list.
		 */
		void remove_owner (player *pl);
		void remove_owner (int pid);
		
		/* 
		 * Removes the specified player from the world member list.
		 */
		void remove_member (player *pl);
		void remove_member (int pid);
		
		
		
		/* 
		 * Returns true if the specified player can modify blocks in the world.
		 */
		bool can_build (player *pl) const;
		
		/* 
		 * Returns true if the specified player can join the world.
		 */
		bool can_join (player *pl) const;
	};
}

#endif

