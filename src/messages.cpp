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

#include "messages.hpp"
#include "rank.hpp"
#include "permissions.hpp"
#include "server.hpp"
#include "player.hpp"
#include <sstream>
#include <vector>
#include <algorithm>


namespace hCraft {

	/* 
	 * Constructs a default environment for the specified player.
	 */
	messages::environment::environment (player *pl)
		: srv (pl->get_server ())
	{
		this->target = pl;
		
		this->curr_world = pl->get_world ();
		this->prev_world = nullptr;
	}
	
	
	static void
	_replace_all (std::string& str, const std::string& from, const std::string& to)
	{
		size_t start_pos = 0;
		while ((start_pos = str.find (from, start_pos)) != std::string::npos)
			{
				str.replace (start_pos, from.length (), to);
				start_pos += to.length ();
			}
	}
	
	/* 
	 * %target             = Player username
	 * %target-nick        = Player nickname
	 * %target-col         = Colored player username
	 * %target-col-nick    = Colored player nickname
	 * %target-login-count = Number of times player logged into the server.
	 * 
	 * %curr-world         = Target player's current world name
	 * %curr-world-col     = Colored world name
	 * %prev-world         = Target player's previous world name
	 * %prev-world-col     = COlored previous world name
	 */
	std::string
	messages::compile (std::string input, const messages::environment& env)
	{
		/* 
		 * TODO: Switch to a better replacement method... :X
		 *       If the number of variables becomes large, doing it this way will
		 *       be terribly inefficient.
		 */
		
		std::ostringstream ss;
		
		// from longest to shortest
		if (env.target)
			{
				ss << env.target->log_count;
				_replace_all (input, "%target-login-count", ss.str ());
				ss.str (std::string ());
				
				_replace_all (input, "%target-col-nick", env.target->get_colored_nickname ());
				_replace_all (input, "%target-nick", env.target->get_nickname ());
				_replace_all (input, "%target-col", env.target->get_colored_username ());
				_replace_all (input, "%target", env.target->get_username ());
			}
		
		if (env.curr_world)
			{
				_replace_all (input, "%curr-world-col", env.curr_world->get_colored_name ());
				_replace_all (input, "%curr-world", env.curr_world->get_name ());
			}
		
		if (env.prev_world)
			{
				_replace_all (input, "%prev-world-col", env.prev_world->get_colored_name ());
				_replace_all (input, "%prev-world", env.prev_world->get_name ());
			}
		
		return input;
	}

//----
	
	std::string
	messages::insufficient_permissions (group_manager& groups, const char *perm)
	{
		permission_manager& perm_man = groups.get_permission_manager ();
		permission perm_struct       = perm_man.get (perm);
		if (!perm_struct.valid ())
			return "§4 * §cYou are not allowed to do that§7.";
		
		// create a vector of groups sorted in ascending order of power.
		std::vector<group *> sorted_groups;
		for (auto itr = groups.begin (); itr != groups.end (); ++itr)
			sorted_groups.push_back (itr->second);
		std::sort (sorted_groups.begin (), sorted_groups.end (),
			[] (const group *a, const group *b) -> bool	
				{ return (*a) < (*b); });
		
		// find the least powerful group that has the permission
		group *lgrp = nullptr;
		int   count = 0;
		for (group *grp : sorted_groups)
			{
				if (grp->has (perm_struct))
					{
						if (!lgrp)
							lgrp = grp;
						++ count;
					}
			}
		
		if (count == 0)
			return "§4 * §cYou are not allowed to do that§7.";
		
		std::ostringstream ss;
		const char *grp_name = lgrp->name.c_str ();
		bool insert_n =
			((grp_name[0] == 'a') || (grp_name[0] == 'i') ||
			 (grp_name[0] == 'e') || (grp_name[0] == 'o'));
		
		if (count == 1)
			ss << "§4 * §cYou must be a";
		else
			{
				if ((count == 2) && (lgrp != sorted_groups[sorted_groups.size () - 1])
					&& (sorted_groups[sorted_groups.size () - 1]->has (perm_struct)))
					ss << "§4 * §cYou must be a";
				else
					ss << "§4 * §cYou must be at least a";
			}
				
		if (insert_n) ss << 'n';
		ss << " §" << lgrp->color << grp_name << " §cto do that§7.";
		return ss.str ();
	}
	
	std::string
	messages::over_fill_limit (int f_limit)
	{
		std::ostringstream ss;
		ss << "§c * §7You are trying to fill too many blocks§c! (§7limit §f- §8" << f_limit << " §7blocks§c)";
		return ss.str ();
	}
		
	std::string
	messages::over_select_limit (int s_limit)
	{
		std::ostringstream ss;
		ss << "§c * §7You are selecting too many blocks§c! (§7limit §f- §8" << s_limit << " §7blocks§c)";
		return ss.str ();
	}
	
	std::string
	messages::not_allowed ()
	{
		return "§c * §7You are not allowed to do that§c.";
	}
}

