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

#include "commands/warnlog.hpp"
#include "player/player.hpp"
#include "util/utils.hpp"
#include "util/stringutils.hpp"
#include "system/server.hpp"
#include <sstream>
#include <algorithm>
#include <ctime>


namespace hCraft {
	namespace commands {
		
		struct warn_entry {
			unsigned int num;
			unsigned int warner;
			std::string reason;
			std::time_t when;
		};
		
		/*
		 * /warnlog
		 * 
		 * Displays all warnings issued on a selected player.
		 * 
		 * Permissions:
		 *   - command.admin.warnlog
		 *       Needed to execute the command.
		 *   - command.admin.warnlog.others
		 *       Required to check the warnings of other players.
		 */
		void
		c_warnlog::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			server& srv = pl->get_server ();
			std::string target_name = pl->get_username ();
			player *target = pl;
			if (reader.has_next ())
				{
					target_name = reader.next ().as_str ();
					
					target = srv.get_players ().find (target_name.c_str ());
					if (!pl->has ("command.admin.warnlog.others") && (!target || (target != pl)))
						{
							pl->message (messages::not_allowed ());
							return;
						}
				}
			
			std::string target_col_nick = target_name;
			int target_pid;
			
			try
				{
					soci::session sql {srv.sql_pool ()};
					
					sqlops::player_info pinf;
					if (!sqlops::player_data (sql, target_name.c_str (), srv, pinf))
						{
							pl->message ("§c * §7Unknown player§f: §c" + target_name);
							return;
						}
					
					target_pid = pinf.id;
					target_name = pinf.name;
					target_col_nick = "§" + std::string (1, pinf.rnk.main ()->color) + pinf.nick;
					
					soci::rowset<soci::row> rs = (sql.prepare << "SELECT `num`, `warner`, `reason`, `warn_time` FROM `warns` WHERE `target`=:tar",
						soci::use (target_pid));
					std::vector<warn_entry> warns;
					for (auto itr = rs.begin (); itr != rs.end (); ++itr)
						{
							const soci::row& r = *itr;
							warns.push_back ({
								.num = r.get<unsigned int> (0),
								.warner = r.get<unsigned int> (1),
								.reason = r.get<std::string> (2),
								.when = (std::time_t)r.get<unsigned long long> (3)
							});
						}
					
					std::sort (warns.begin (), warns.end (),
						[] (const warn_entry& a, const warn_entry& b)
							{
								return a.num < b.num;
							});
					
					if (warns.empty ())
						{
							if (target == pl)
								pl->message ("§3You have no warnings§e.");
							else
								pl->message (target_col_nick + " §3has no warnings§e.");
						}
					else
						{
							std::ostringstream ss;
							if (target == pl)
								{
									ss << "§cDisplaying your warnings §4[§7" << warns.size () << " §cwarning" << ((warns.size () == 1) ? "" : "s") << "§4]§f:";
									pl->message (ss.str ());
								}
							else
								{
									ss << "§cDisplaying " << target_col_nick << "§c's warnings §4[§7" << warns.size () << " §cwarning" << ((warns.size () == 1) ? "" : "s") << "§4]§f:";
									pl->message (ss.str ());
								}
							
							for (warn_entry& warn : warns)
								{
									std::time_t now = std::time (nullptr);
									std::time_t then = warn.when;
									std::string rel;
									utils::relative_time_short (now, then, rel);
									
									ss.str (std::string ());
									ss << "§8    #" << warn.num << ") §7by " << sqlops::player_colored_name (sql, warn.warner, srv) << "§7, " << rel << " ago §7- §c" << warn.reason;
									pl->message (ss.str ());
								}
						}
				}
			catch (const std::exception& ex)
				{
					pl->message ("§4 * §cFailed to fetch warn log§4.");
					return;
				}
			
			
		}
	}
}

