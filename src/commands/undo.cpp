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

#include "commands/drawc.hpp"
#include "player.hpp"
#include "server.hpp"
#include "world.hpp"
#include "stringutils.hpp"
#include "utils.hpp"
#include "sqlops.hpp"
#include <sstream>
#include <ctime>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /undo -
		 * 
		 * Reverses all block changes made by a selected player in a specified
		 * time frame.
		 * 
		 * Permissions:
		 *   - command.draw.undo
		 *       Needed to execute the command.
		 *   - command.draw.undo.others
		 *       Whether the user is allowed to undo changes made by other players.
		 */
		void
		c_undo::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
			
			if (!reader.parse (this, pl))
				return;
			
			std::string target_name;
			int seconds = 30;
			
			if (reader.arg_count () == 0)
				{
					// /undo
					target_name = pl->get_username ();
				}
			else
				{
					target_name = reader.next ().as_str ();
					if (reader.has_next ())
						{
							seconds = utils::seconds_from_time_str (reader.next ().as_str ());
							if (seconds < 0)
								{
									pl->message ("§c * §7Invalid time string");
									return;
								}
						}
				}
			
			sqlops::player_info tinf;
			{
				soci::session sql (pl->get_server ().sql_pool ());
				if (!sqlops::player_data (sql, target_name.c_str (), pl->get_server (), tinf))
					{
						pl->message ("§c * §7Unknown player§f: §c" + target_name);
						return;
					}
			}
			
			target_name = tinf.name;
			if (tinf.id != pl->pid ())
				{
					if (!pl->has ("command.draw.undo.others"))
						{
							pl->message ("§c * §7You are not allowed to do that§c.");
							return;
						}
					else
						{
							if (tinf.rnk >= pl->get_rank ())
								{
									pl->message ("§c * §7You can only undo changes made by players ranked lower than you§c.");
									return;
								}
						}
				}
				
			pl->message ("§cTo be implemented... :o");
		}
	}
}

