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

#include "commands/infoc.hpp"
#include "player.hpp"
#include "server.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
	
		
		static const char*
		_suffix (const std::string& str)
		{
			if (str.size () >= 2 && str[str.size () - 2] == 'c' && str[str.size () - 2] == 'h')
				return "";
			else if (str.size () >= 2 && str[str.size () - 2] == 's' && str[str.size () - 2] == 'h')
				return "";
			else if (!str.empty () && str.back () == 'x')
				return "";
			else if (!str.empty () && str.back () == 's')
				return "";
			return "s";
		}
		
		/* 
		 * /players
		 * 
		 * Displays a list of online players.
		 * 
		 * Permissions:
		 *   - command.info.players
		 *       Needed to execute the command.
		 */
		void
		c_players::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			auto& players = pl->get_server ().get_players ();
			
			std::ostringstream ss;
			ss << "§eThere " << ((players.count () == 1) ? "is" : "are") << " currently §b" << players.count () << " §eplayer"
				<< ((players.count () == 1) ? "" : "s") << " online§f:";
			pl->message (ss.str ());
			
			server &srv = pl->get_server ();
			group_manager &gman = srv.get_groups ();
			
			// create a sorted list of groups (in descending values of power)
			std::vector<group *> group_list;
			for (auto itr = gman.begin (); itr != gman.end (); ++itr)
				group_list.push_back (itr->second);
			std::sort (group_list.begin (), group_list.end (),
				[] (const group *a, const group *b) -> bool
					{
						return (*a) > (*b);
					});
			
			for (group *grp : group_list)
				{
					std::vector<player *> vec;
					
					players.all (
						[&vec, grp] (player *pl)
							{
								if (pl->get_rank ().main () == grp)
									vec.push_back (pl);
							});
					
					if (!vec.empty ())
						{
							ss.str (std::string ());
							ss << "§" << grp->color << "  " << grp->name << _suffix (grp->name) << " §e(§7" << vec.size () << "§e): §7";
							for (player *pl : vec)
								ss << pl->get_username () << " ";
							pl->message_spaced (ss.str ());
						}
				}
		}
	}
}

