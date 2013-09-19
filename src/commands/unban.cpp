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
	
		static void
		_unban_player (player *pl, std::string target_name,
			const std::string& reason, bool silent = false)
		{
			server& srv = pl->get_server ();
			
			player *target = srv.get_players ().find (target_name.c_str ());
			if (target)
				{
					if (target == pl)
						{
							if (!silent)
								pl->message ("§c * §7You are not banned§c.");
							return;
						}
					
					if (!silent)
						pl->message ("§c * §7That player is online§c...");
					return;
				}
				
			std::string target_colored_nick;
			
			int target_pid = 0;
			int unbanner_pid = pl->pid ();

			// record unban
			{
				soci::session sql (srv.sql_pool ());
				try
					{
						if (!sqlops::player_exists (sql, target_name.c_str ()))
							{
								if (!silent)
									pl->message ("§c * §7No such player§f: §c" + target_name);
								return;
							}
						
						if (!sqlops::is_banned (sql, target_name.c_str ()))
							{
								if (!silent)
									pl->message ("§c * §7Player not banned§c.");
								return;
							}
						
						target_name = sqlops::player_name (sql, target_name.c_str ());
						target_colored_nick = sqlops::player_colored_nick (sql, target_name.c_str (), pl->get_server ());
						target_pid = sqlops::player_id (sql, target_name.c_str ());
						
						sqlops::modify_ban_status (sql, target_name.c_str (), false);
						sqlops::record_unban (sql, target_pid, unbanner_pid, reason.c_str ());
					}
				catch (const std::exception& ex)
					{
						pl->message ("§4 * §cAn error has occurred while recording unban message");
					}
				
				pl->message ("§7 | §eRecorded unban message§7: §c\"" + reason + "§c\"");
			}
			
			{
				std::ostringstream ss;
				ss << "§8 > " << target_colored_nick << " §8has been unbanned by "
					 << pl->get_colored_nickname () << "§8!";
				srv.get_players ().message (ss.str ());
			}
		}
		
		
		static void
		_unban_ip (player *pl, const std::string& ip_addr)
		{
			if (!sutils::is_ip_addr (ip_addr))
				{
					pl->message ("§c * §7Invalid IP address§f: §c" + ip_addr);
					return;
				}
			
			if (ip_addr.compare (pl->get_ip ()) == 0)
				{
					pl->message ("§c * §7Your IP isn§c'§7t banned§c.");
					return;
				}
			
			server& srv = pl->get_server ();
			
			{
				soci::session sql (srv.sql_pool ());
				
				try
					{
						if (!sqlops::is_ip_banned (sql, ip_addr.c_str ()))
							{
								pl->message ("§c * §7IP address not banned§c.");
								return;
							}
						
						sqlops::unban_ip (sql, ip_addr.c_str ());
					}
				catch (const std::exception& ex)
					{
						pl->message ("§4 * §cAn error has occurred while unbanning IP");
						return;
					}
			}
			
			pl->message ("§7 | §8IP address §c" + ip_addr + " §8has been unbanned§c.");
		}
		
		
		/*
		 * /unban
		 * 
		 * Revokes a permanent ban from a specified player.
		 * 
		 * Permissions:
		 *   - command.admin.unban
		 *       Needed to execute the command.
		 *   - command.admin.unban.ip
		 *       Required to lift IP bans.
		 */
		void
		c_unban::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
			
			reader.add_option ("ip", "i");
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string  target_name = reader.next ().as_str ();
			std::string  reason = reader.has_next () ? reader.all_after (0)
				: "No reason specified";
			
			if (reader.opt ("ip")->found ())
				{
					if (!pl->has ("command.admin.unban.ip"))
						{
							pl->message ("§c * §7You are not allowed to do that§c.");
							return;
						}
					
					_unban_ip (pl, target_name);
				}
			else
				_unban_player (pl, target_name, reason, false);
		}
	}
}

