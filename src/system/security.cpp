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

#include "system/security.hpp"
#include "player/player.hpp"
#include "util/stringutils.hpp"
#include "system/server.hpp"


namespace hCraft {
	
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
	bool
	basic_security::check_perm_str (player *pl, const std::string& access_str)
	{
		if (/*pl->is_op () ||*/ access_str.empty ())
			return true;
		
		enum term_type
			{
				TT_GROUP,
				TT_ATLEAST_GROUP,
				TT_PLAYER,
				TT_PERM
			};
		
		const char *str = access_str.c_str ();
		const char *ptr = str;
		while (*ptr)
			{
				term_type typ = TT_GROUP;
				std::string word;
				int neg = 0;
				
				while (*ptr && (*ptr != '|'))
					{
						int c = *ptr;
						switch (c)
							{
								case '!':
									++ neg;
									if (neg > 1)
										return false;
									break;
								
								case '^':
									if (typ == TT_PLAYER)
										return false;
									typ = TT_PLAYER;
									break;
								
								case '*':
									if (typ == TT_PERM)
										return false;
									typ = TT_PERM;
									break;
								
								case '>':
									if (typ == TT_ATLEAST_GROUP)
										return false;
									typ = TT_ATLEAST_GROUP;
									break;
								
								default:
									word.push_back (c);
							}
						++ ptr;
					}
				
				if (*ptr == '|')
					++ ptr;
				
				bool res = true;
				switch (typ)
					{
						case TT_GROUP:
							{
								group *grp = pl->get_server ().get_groups ().find (word.c_str ());
								if (!grp)
									res = false;
								else if (!pl->get_rank ().contains (grp))
									res = false;
							}
							break;
						
						case TT_ATLEAST_GROUP:
							{
								group *grp = pl->get_server ().get_groups ().find (word.c_str ());
								if (grp && (pl->get_rank ().power () < grp->power))
									res = false;
							}
							break;
						
						case TT_PLAYER:
							res = sutils::iequals (word, pl->get_username ());
							break;
						
						case TT_PERM:
							res = pl->has (word.c_str ());
							break;
					}
				
				if (neg == 1)
					res = !res;
				if (res)
					return true;
			}
		
		return false;
	}
	
	
	
//------------------------------------------------------------------------------
	
	/* 
	 * Checks whether the specified player is a world owner.
	 */
	bool
	ownership_security::is_owner (player *pl) const
	{
		return this->is_owner (pl->pid ());
	}
	
	bool
	ownership_security::is_owner (int pid) const
	{
		return (this->owners.find (pid) != this->owners.end ());
	}
	
	
	
	/* 
	 * Checks whether the specified player is a world member.
	 */
	bool
	ownership_security::is_member (player *pl) const
	{
		return this->is_member (pl->pid ());
	}
	
	bool
	ownership_security::is_member (int pid) const
	{
		return (this->members.find (pid) != this->members.end ());
	}
	
	
	
	/* 
	 * Adds the specified player to the world owner list.
	 */
	void
	ownership_security::add_owner (player *pl)
	{
		this->add_owner (pl->pid ());
	}
	
	void
	ownership_security::add_owner (int pid)
	{
		this->owners.insert (pid);
	}
	
	
	
	/* 
	 * Adds the specified player to the world member list.
	 */
	void
	ownership_security::add_member (player *pl)
	{
		this->add_member (pl->pid ());
	}
	
	void
	ownership_security::add_member (int pid)
	{
		this->members.insert (pid);
	}
	
	
	
	/* 
	 * Removes the specified player from the world owner list.
	 */
	void
	ownership_security::remove_owner (player *pl)
	{
		this->remove_owner (pl->pid ());
	}
	
	void
	ownership_security::remove_owner (int pid)
	{
		this->owners.erase (pid);
	}
	
	
	
	/* 
	 * Removes the specified player from the world member list.
	 */
	void
	ownership_security::remove_member (player *pl)
	{
		this->remove_member (pl->pid ());
	}
	
	void
	ownership_security::remove_member (int pid)
	{
		this->members.erase (pid);
	}
}

