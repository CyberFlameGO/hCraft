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

#include "commands/adminc.hpp"
#include "player.hpp"
#include "stringutils.hpp"
#include "server.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/* /unban
		 * 
		 * Revokes a permanent ban from a specified player.
		 * 
		 * Permissions:
		 *   - command.admin.unban
		 *       Needed to execute the command.
		 */
		void
		c_unban::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
			
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string  target_name = reader.next ().as_str ();
			std::string  reason = reader.has_next () ? reader.all_after (0)
				: "No reason specified";
			
			player *target = pl->get_server ().get_players ().find (target_name.c_str ());
			if (target)
				{
					if (target == pl)
						{
							pl->message ("§c * §7You are not banned§c.");
							return;
						}
					
					pl->message ("§c * §7That player is online§c...");
					return;
				}
				
			std::string target_colored_nick;
			server& srv = pl->get_server ();

			// record unban
			{
				soci::session sql (srv.sql_pool ());
				try
					{
						if (!sqlops::player_exists (sql, target_name.c_str ()))
							{
								pl->message ("§c * §7No such player§f: §c" + target_name);
								return;
							}
						
						if (!sqlops::is_banned (sql, target_name.c_str ()))
							{
								pl->message ("§c * §7Player is not banned§c.");
								return;
							}
						
						target_name = sqlops::player_name (sql, target_name.c_str ());
						target_colored_nick = sqlops::player_colored_nick (sql, target_name.c_str (), pl->get_server ());
						sqlops::modify_ban_status (sql, target_name.c_str (), false);
						sqlops::record_unban (sql, target_name.c_str (),
							pl->get_username (), reason.c_str ());
					}
				catch (const std::exception& ex)
					{
						pl->message ("§4 * §cAn error has occurred while recording unban message");
					}
				
				std::ostringstream ss;
				ss << "§7 | §eRecorded unban message§7: §c\"" << reason << "§c\"";
				pl->message (ss.str ());
			}
			
			{
				std::ostringstream ss;
				ss << "§8 > " << target_colored_nick << " §8has been unbanned by "
					 << pl->get_colored_nickname () << "§8!";
				srv.get_players ().message (ss.str ());
			}
		}
	}
}

