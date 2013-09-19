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
		_ban_player (player *pl, std::string target_name,
			const std::string& reason, const std::string& ban_msg, bool silent = false)
		{
			server& srv = pl->get_server ();
			
			if (!sutils::is_valid_username (target_name))
				{
					if (!silent)
						pl->message ("§c * §7Invalid username§f: §c" + target_name);
					return;
				}
			
			player *target = srv.get_players ().find (target_name.c_str ());
			if (target)
				{
					target->banned = true;
				}
			std::string target_colored_nick;
			
			int target_pid = 0;
			int banner_pid = pl->pid ();

			// record ban
			{
				soci::session sql (srv.sql_pool ());
				
				try
					{
						if (target)
							{
								target_colored_nick = target->get_colored_nickname ();
								target_name = target->get_username ();
								target_pid = target->pid ();
							}
						else
							{
								if (!sqlops::player_exists (sql, target_name.c_str ()))
									{
										sqlops::player_info pd;
										sqlops::default_player_data (target_name.c_str (), srv, pd);
										pd.banned = true;
										sqlops::save_player_data (sql, target_name.c_str (), srv, pd);
										
										target_colored_nick.assign ("§");
										target_colored_nick.push_back (srv.get_groups ().default_rank.main ()->color);
										target_colored_nick.append (target_name);
										
										if (!silent)
											pl->message ("§7 | §cNOTE§7: §ePlayer " + target_colored_nick + " §ehas not logged in even once§7.");
									}
								else
									{
										if (sqlops::is_banned (sql, target_name.c_str ()))
											{
												if (!silent)
													pl->message ("§c * §7Player is already banned§c.");
												return;
											}
										
										target_name = sqlops::player_name (sql, target_name.c_str ());
										target_colored_nick = sqlops::player_colored_nick (sql, target_name.c_str (), srv);
									}
								target_pid = sqlops::player_id (sql, target_name.c_str ());
							}
						
						sqlops::modify_ban_status (sql, target_name.c_str (), true);
						sqlops::record_ban (sql, target_pid, banner_pid, reason.c_str ());
						
						if (!silent)
							{
								pl->message ("§7 | §eRecorded ban message§7: §c\"" + reason + "§c\"");
								
								std::ostringstream ss;
								ss << "§4 > " << target_colored_nick << " §4has been banned by "
									 << pl->get_colored_nickname () << "§4!";
								srv.get_players ().message (ss.str ());
							}
					}
				catch (const std::exception& ex)
					{
						pl->message ("§4 * §cAn error has occurred while recording ban");
					}
			}
			
			if (target)
				target->kick (ban_msg.c_str (), ("Banned by " + std::string (pl->get_username ())).c_str ());
		}
		
		
		static void
		_ban_ip (player *pl, const std::string& ip_addr,
			const std::string& reason, const std::string& ban_msg)
		{
			if (!sutils::is_ip_addr (ip_addr))
				{
					pl->message ("§c * §7Invalid IP address§f: §c" + ip_addr);
					return;
				}
			
			if (ip_addr.compare (pl->get_ip ()) == 0)
				{
					pl->message ("§c * §7You cannot ban your own IP address§c.");
					return;
				}
			
			server& srv = pl->get_server ();
			int banner_pid = pl->pid ();

			// record ban
			{
				soci::session sql (srv.sql_pool ());
				
				try
					{
						if (sqlops::is_ip_banned (sql, ip_addr.c_str ()))
							{
								pl->message ("§c * §7That IP address is already banned from this server§c.");
								return;
							}
						
						sqlops::record_ipban (sql, ip_addr.c_str (), banner_pid, reason.c_str ());
						
						// kick players
						std::vector<int> pids;
						sqlops::get_pids_from_ip (sql, ip_addr.c_str (), pids);
						for (int pid : pids)
							{
								player *target = pl->get_server ().get_players ().find (
									sqlops::player_name (sql, pid).c_str ());
								if (target)
									target->kick (ban_msg.c_str (), ("Banned by " + std::string (pl->get_username ())).c_str ());
							}
					}
				catch (const std::exception& ex)
					{
						pl->message ("§4 * §cAn error has occurred while recording ban");
						return;
					}
			}
			
			pl->message ("§7 | §eRecorded ban message§7: §c\"" + reason + "§c\"");
			pl->message ("§7 | §4IP address §c" + ip_addr + " §4has been banned§c.");
		}
		
		
		static void
		_ban_players_from_ip (player *pl, const std::string& ip_addr,
			const std::string& reason, const std::string& ban_msg)
		{
			if (!sutils::is_ip_addr (ip_addr))
				{
					pl->message ("§c * §7Invalid IP address§f: §c" + ip_addr);
					return;
				}
			
			server& srv = pl->get_server ();
		
			{
				soci::session sql (srv.sql_pool ());
				
				try
					{
						std::vector<int> pids;
						sqlops::get_pids_from_ip (sql, ip_addr.c_str (), pids);
						
						std::ostringstream ss;
						ss << "§7 | §eMatched §b" << pids.size () << " §eusername" << ((pids.size () == 1) ? "" : "s") << "§7.";
						pl->message (ss.str ());
						
						int c = 0;
						bool f = false;
						for (size_t i = 0; i < pids.size (); ++i)
							{
								int pid = pids[i];
								if (pid == pl->pid ())
									{
										if (!f)
											pl->message ("§c * §7That IP address belongs to your user§c.");
										f = true;
										continue;
									}
								
								std::string name = sqlops::player_name (sql, pid);
								
								if (i < 3)
									{
										pl->message ("§7 | > " + sqlops::player_colored_name (sql, name.c_str (), srv));
									}
								else if (i == 3)
									{
										ss.str (std::string ());
										ss << "§7 | > ... and §f" << (pids.size () - 3) << " §7other" << ((pids.size () == 1) ? "." : "s.");
										pl->message (ss.str ());
									} 
								
								_ban_player (pl, name, reason, ban_msg, true);
								++ c;
							}
						
						if (c > 0)
							pl->message ("§7 | §eRecorded ban message§7: §c\"" + reason + "§c\"");
						
						ss.str (std::string ());
						ss << "§7 | §c" << c << " §4player" << ((c == 1) ? " has" : "s have") << " been banned§.";
						pl->message (ss.str ());
					}
				catch (const std::exception& ex)
					{
						pl->message ("§4 * §cFailed to fetch usernames from IP address§4.");
					}
			}
		}
		
		
		/*
		 * /ban
		 * 
		 * Permanently bans a player from the server.
		 * 
		 * Permissions:
		 *   - command.admin.ban
		 *       Needed to execute the command.
		 *   - command.admin.ban.ip
		 *       Required to issue IP bans.
		 *   - command.admin.ban.ip*
		 *       Required to issue regular bans on accounts that belong to a certain
		 *       IP address.
		 */
		void
		c_ban::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			reader.add_option ("message", "m", 1, 1);
			reader.add_option ("ip", "i");
			if (!reader.parse (this, pl))
				return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string  target_name = reader.next ().as_str ();
			std::string  reason = reader.has_next () ? reader.all_after (0)
				: "No reason specified";
			std::string  ban_msg = "§4";
			
			if (reader.opt ("ip")->found ())
				{
					if (!pl->has ("command.admin.ban.ip"))
						{
							pl->message ("§c * §7You are not allowed to do that§c.");
							return;
						}
					
					{
						auto opt = reader.opt ("message");
						if (opt->found ())
							ban_msg.append (opt->arg (0).as_str ());
						else
							ban_msg.append ("You have been §5IP§c-§4banned from this server");
					}
					
					_ban_ip (pl, target_name, reason, ban_msg);
				}
			else
				{
					{
						auto opt = reader.opt ("message");
						if (opt->found ())
							ban_msg.append (opt->arg (0).as_str ());
						else
							ban_msg.append ("You have been banned from this server");
					}
					if (sutils::is_ip_addr (target_name))
						{
							if (!pl->has ("command.admin.ban.ip*"))
							{
								pl->message ("§c * §7You are not allowed to do that§c.");
								return;
							}
						
							_ban_players_from_ip (pl, target_name, reason, ban_msg);
						}
					else
						_ban_player (pl, target_name, reason, ban_msg, false);
				}
		}
	}
}

