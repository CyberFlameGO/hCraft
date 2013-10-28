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

#include "commands/warn.hpp"
#include "player/player.hpp"
#include "util/stringutils.hpp"
#include "system/server.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/*
		 * /warn
		 * 
		 * Issues a warning for a selected player.
		 * 
		 * Permissions:
		 *   - command.admin.warn
		 *       Needed to execute the command.
		 */
		void
		c_warn::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			server& srv = pl->get_server ();
			std::string target_name = reader.next ();
			std::string target_col_nick = target_name;
			int target_pid;
			
			player *target = srv.get_players ().find (target_name.c_str ());
			
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f: §e/warn §cplayer reason");
					return;
				}
			std::string reason = reader.rest ();
			
			try
				{
					soci::session sql {srv.sql_pool ()};
					
					if (target)
						{
							target_name = target->get_username ();
							target_col_nick = target->get_colored_nickname ();
							target_pid = target->pid ();
						}
					else
						{
							sqlops::player_info pinf;
							if (!sqlops::player_data (sql, target_name.c_str (), srv, pinf))
								{
									pl->message ("§c * §7Unknown player§f: §c" + target_name);
									return;
								}
							
							target_pid = pinf.id;
							target_name = pinf.name;
							target_col_nick = "§" + std::string (1, pinf.rnk.main ()->color) + pinf.nick;
						}
					
					sqlops::record_warn (sql, target_pid, pl->pid (), reason.c_str ());
				}
			catch (const std::exception& ex)
				{
					pl->message ("§4 * §cFailed to issue warning§4.");
					return;
				}
			
			if (target)
				{
					target->message ("§4█§0███§4█§0██§4███§0█§0█§4████§0█§0█§4█§0███§4█§0█§4█████§0█§4████§0█");
					target->message ("§4█§0███§4█§0█§4█§0███§4█§0█§4█§0███§4█§0█§4██§0██§4█§0█§4█§0█████§4█§0███§4█");
					target->message ("§4█§0███§4█§0█§4█████§0█§4████§0█§0█§4█§0█§4█§0█§4█§0█§4███§0███§4█§0███§4█");
					target->message ("§4█§0███§4█§0█§4█§0███§4█§0█§4█§0███§4█§0█§4█§0██§4██§0█§4█§0█████§4█§0███§4█");
					target->message ("§4█§0█§4█§0█§4█§0█§4█§0███§4█§0█§4█§0███§4█§0█§4█§0███§4█§0█§4█§0█████§4█§0███§4█");
					target->message ("§4██§0█§4██§0█§4█§0███§4█§0█§4█§0███§4█§0█§4█§0███§4█§0█§4█§0█████§4█§0███§4█");
					target->message ("§4█§0███§4█§0█§4█§0███§4█§0█§4█§0███§4█§0█§4█§0███§4█§0█§4█████§0█§4████§0█");
				}
			srv.get_players ().message ("§4 > " + target_col_nick + " §7has been warned for§f: §c" + reason); 
		}
	}
}

