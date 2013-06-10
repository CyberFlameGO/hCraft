/* 
 * hCraft - A custom Minecraft server.
 * Copyright (C) 2012	Jacob Zhitomirsky
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
		
		/* /ban
		 * 
		 * Permanently bans a player from the server.
		 * 
		 * Permissions:
		 *   - command.admin.ban
		 *       Needed to execute the command.
		 */
		void
		c_ban::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			reader.add_option ("message", "m", 1, 1);
			if (!reader.parse (this, pl))
				return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string  target_name = reader.next ().as_str ();
			std::string  reason = reader.has_next () ? reader.all_after (0)
				: "No reason specified";
			std::string  ban_msg = "§4";
			{
				auto opt = reader.opt ("message");
				if (opt->found ())
					ban_msg.append (opt->arg (0).as_str ());
				else
					ban_msg.append ("You have been banned from this server");
			}
			
			player *target = pl->get_server ().get_players ().find (target_name.c_str ());
			if (target)
				{
					if (target == pl)
						{
							pl->message ("§c * §7You cannot ban yourself, silly§c.");
							return;
						}
					
					target->banned = true;
				}
			std::string target_colored_nick;
			
			server& srv = pl->get_server ();

			// record ban
			{
				auto& conn = pl->get_server ().sql ().pop ();
				try
					{
						if (target)
							{
								target_colored_nick = target->get_colored_nickname ();
								target_name = target->get_username ();
							}
						else
							{
								if (!sqlops::player_exists (conn, target_name.c_str ()))
									{
										sqlops::player_info pd;
										sqlops::default_player_data (target_name.c_str (), pl->get_server (), pd);
										pd.banned = true;
										sqlops::save_player_data (conn, target_name.c_str (), pl->get_server (), pd);
										
										target_colored_nick.assign ("§");
										target_colored_nick.push_back (pl->get_server ().get_groups ().default_rank.main ()->color);
										target_colored_nick.append (target_name);
										
										pl->message ("§7 | §cNOTE§7: §ePlayer " + target_colored_nick + " §ehas not logged in even once§7.");
									}
								else
									{
										if (sqlops::is_banned (conn, target_name.c_str ()))
											{
												pl->message ("§c * §7Player is already banned§c.");
												return;
											}
										
										target_name = sqlops::player_name (conn, target_name.c_str ());
										target_colored_nick = sqlops::player_colored_nick (conn, target_name.c_str (), pl->get_server ());
									}
							}
						
						sqlops::modify_ban_status (conn, target_name.c_str (), true);
						sqlops::record_ban (conn, target_name.c_str (),
							pl->get_username (), reason.c_str ());
					}
				catch (const std::exception& ex)
					{
						pl->message ("§4 * §cAn error has occurred while recording ban message");
					}
				
				pl->get_server ().sql ().push (conn);
				
				std::ostringstream ss;
				ss << "§7 | §eRecorded ban message§7: §c\"" << reason << "§c\"";
				pl->message (ss.str ());
			}
			
			{
				std::ostringstream ss;
				ss << "§4 > " << target_colored_nick << " §4has been banned by "
					 << pl->get_colored_nickname () << "§4!";
				srv.get_players ().message (ss.str ());
			}
			
			if (target)
				target->kick (ban_msg.c_str (), ("Banned by " + std::string (pl->get_username ())).c_str ());
		}
	}
}

