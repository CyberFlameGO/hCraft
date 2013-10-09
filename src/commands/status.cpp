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

#include "commands/status.hpp"
#include "system/server.hpp"
#include "player/player.hpp"
#include "system/sqlops.hpp"
#include "util/stringutils.hpp"
#include "util/utils.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>


namespace hCraft { 
	namespace commands {
		
		/* 
		 * /status
		 * 
		 * Displays information about a specified player.
		 * 
		 * Permissions:
		 *   - command.info.status
		 *       Needed to execute the command.
		 *   - command.info.status.admin
		 *       Optional - will display administrative data if exists.
		 */
		void
		c_status::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
			
			if (!reader.parse (this, pl))
					return;
			if (reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			bool can_see_nick = pl->has ("command.info.status.nick");
			bool can_see_ip = pl->has ("command.info.status.ip");
			bool can_see_logins = pl->has ("command.info.status.logins");
			bool can_see_rank = pl->has ("command.info.status.rank");
			bool can_see_blockstats = pl->has ("command.info.status.blockstats");
			bool can_see_balance = pl->has ("command.info.status.balance");
			
			bool sect1 = can_see_nick || can_see_rank || can_see_ip || can_see_balance;
			bool sect2 = can_see_logins;
			bool sect3 = can_see_blockstats;
			
			std::string target_name = pl->get_username ();
			if (reader.has_next ())
				target_name = reader.next ().as_str ();
			
			{
				soci::session sql (pl->get_server ().sql_pool ());
				
				sqlops::player_info pd;
				
				player *target = pl->get_server ().get_players ().find (target_name.c_str ());
				if (target)
					target->player_data (pd);
				else if (!sqlops::player_data (sql, target_name.c_str (), pl->get_server (), pd))
					{
						pl->message ("§c * §7Player §8" + target_name + " §7does not exist§c.");
						return;
					}
				
				target_name = pd.name;
				
				std::ostringstream ss;
				
			//---
				ss << "§3Displaying §" << pd.rnk.main ()->color << target_name << "§b'§3s status§b:";
				pl->message (ss.str ());
				ss.clear (); ss.str (std::string ()); 
				
				if (sect1)
					{
						if (can_see_nick && pd.nick != pd.name)
							{
								ss << "§6 | §eNickname§6: §" << pd.rnk.main ()->color << pd.nick;
								pl->message (ss.str ());
								ss.clear (); ss.str (std::string ());
							}
				
						if (can_see_rank)
							{
								std::string crnkstr;
								pd.rnk.get_colored_string (crnkstr);
								pl->message ("§6 | §eRank§6: " + crnkstr);
							}
						
						if (can_see_ip && (pd.login_count > 0))
							pl->message ("§6 | §eLast IP address§6: §c" + pd.ip);
						
						if (can_see_balance)
							{
								ss << "§6 | §eBalance§6: §a$" << utils::format_number (pd.balance, 2);
								pl->message (ss.str ());
								ss.clear (); ss.str (std::string ());
							}
				
					//
						if (sect2 || sect3)
							pl->message ("§6 -");
					}
				
				if (sect2)
					{
						struct tm tm_first, tm_last;
						localtime_r (&pd.first_login, &tm_first);
						localtime_r (&pd.last_login, &tm_last);
						std::time_t now = std::time (nullptr);
					
						if (can_see_logins)
							{
								char out[128];
								
								if (pd.login_count > 0)
									{
										std::string first_relative;
										utils::relative_time (now, pd.first_login, first_relative);
										std::strftime (out, sizeof out, "%a %b %d  %H:%M:%S  %Y", &tm_first);
										ss << "§6 | §eFirst login§6: §a" << first_relative;
										pl->message (ss.str ());
										ss.clear (); ss.str (std::string ());
										ss << "§6   - (§b" << out << "§6)";
										pl->message (ss.str ());
										ss.clear (); ss.str (std::string ());
					
										std::string last_relative;
										utils::relative_time (now, pd.last_login, last_relative);
										std::strftime (out, sizeof out, "%a %b %d  %H:%M:%S  %Y", &tm_last);
										ss << "§6 | §eLast login§6: §a" << last_relative;
										pl->message (ss.str ());
										ss.clear (); ss.str (std::string ());
										ss << "§6   - (§b" << out << "§6)";
										pl->message (ss.str ());
										ss.clear (); ss.str (std::string ());
									}
					
								ss << "§6 | §eLogin count§6: §b" << pd.login_count;
								pl->message (ss.str ());
								ss.clear (); ss.str (std::string ());
							}
						
						if (pd.banned)
							pl->message ("§6 | §eStatus§6: §4Banned");
						
						if (sect3)
							pl->message ("§6 -");
					}

				if (sect3)
					{
						if (can_see_blockstats)
							{
								ss << "§6 | §eBlocks placed§6: §a" << pd.blocks_created;
								pl->message (ss.str ());
								ss.clear (); ss.str (std::string ());
				
								ss << "§6 | §eBlocks destroyed§6: §a" << pd.blocks_destroyed;
								pl->message (ss.str ());
								ss.clear (); ss.str (std::string ());
				
								double cd_ratio = 0.0;
								double cd_d = pd.blocks_destroyed;
								double cd_c = pd.blocks_created;
								if (cd_d > cd_c)
									{
										if (cd_c == 0.0)
											cd_ratio = -cd_d;
										else
											cd_ratio = (cd_d / cd_c) * -1.0;
									}
								else
									{
										if (cd_d == 0.0)
											cd_ratio = cd_c;
										else
											cd_ratio = cd_c / cd_d;
									}
								ss << "§6 | §eCreate\\Destroy ratio§6: §c" << cd_ratio << std::endl;
								pl->message (ss.str ());
								ss.clear (); ss.str (std::string ());
							}
					}
			//---
			}
		}
	}
}

