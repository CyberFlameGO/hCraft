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

#ifndef _hCraft__SECURITY_H_
#define _hCraft__SECURITY_H_

#include <string>
#include <set>


namespace hCraft {
	
	class player;
	
	
	class basic_security
	{
	public:
		/* 
		 * Checks whether the given player has the required permissions as
		 * specified in the permission string.
		 * 
		 * Syntax:
		 *   term1|term2|term3|...|termN
		 * 
		 * Where a term is one of:
		 *   <group name>   : Player's rank must include this group.
		 *   ><group name>  : Player's rank must have a power value higher or equal
		 *                    to the power value of this group.
		 *   ^<player name> : Player must have this name.
		 *   *<permission>  : Player must have this permission.
		 * 
		 * The vertical bars (|) denote OR.
		 * Terms may be negated by being preceeded with !
		 */
		static bool check_perm_str (player *pl, const std::string& str);
	};
	
	
	/* 
	 * Adds owner and member support.
	 */
	class ownership_security: public basic_security
	{
		std::set<int> owners;
		std::set<int> members;
		
	public:
		inline const std::set<int>& get_owners () const { return this->owners; }
		inline const std::set<int>& get_members () const { return this->members; }
		
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
	};
}

#endif

