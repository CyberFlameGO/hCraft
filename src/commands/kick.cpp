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
		
		/* /kick
		 * 
		 * Kicks a player from the server.
		 * 
		 * Permissions:
		 *   - command.admin.kick
		 *       Needed to execute the command.
		 */
		void
		c_kick::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
			
			reader.add_option ("message", "m", 1, 1);
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string& target_name = reader.next ().as_str ();
			std::string  reason = reader.has_next () ? reader.all_after (0)
				: "No reason specified";
			std::string  kick_msg = "§c";
			{
				auto opt = reader.opt ("message");
				if (opt->found ())
					kick_msg.append (opt->arg (0).as_str ());
				else
					kick_msg.append ("You have been kicked from the server");
			}
			
			player *target = pl->get_server ().get_players ().find (target_name.c_str ());
			if (!target)
				{
					pl->message ("§c * §7No such player§f: §c" + target_name);
					return;
				}
			else if (target->bad ()) return;
			
			int target_pid = target->pid ();
			int kicker_pid = pl->pid ();
			
			server& srv = pl->get_server ();
			
			// record kick
			{
				soci::session sql (srv.sql_pool ());
				try
					{
						sqlops::record_kick (sql, target_pid, kicker_pid, reason.c_str ());
					}
				catch (const std::exception& ex)
					{
						pl->message ("§4 * §cAn error has occurred while recording kick message");
					}
				
				std::ostringstream ss;
				ss << "§7 | §eRecorded kick message§7: §c\"" << reason << "§c\"";
				pl->message (ss.str ());
			}
			
			{
				std::ostringstream ss;
				ss << "§4 > " << target->get_colored_nickname () << " §chas been kicked by "
					 << pl->get_colored_nickname () << "§c!";
				srv.get_players ().message (ss.str ());
			}
			target->kick (kick_msg.c_str (), reason.c_str ());
		}
	}
}

