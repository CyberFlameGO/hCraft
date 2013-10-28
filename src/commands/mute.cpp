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

#include "commands/mute.hpp"
#include "system/server.hpp"
#include "player/player.hpp"
#include "util/stringutils.hpp"
#include <sstream>
#include <cmath>


namespace hCraft {
	namespace commands {
		
		static void
		show_usage (player *pl)
		{
			pl->message ("§c * §7Usage§f: §e/mute §cplayer §8[§creason§8]");
		}
		
		static int
		secs_from_time_string (const std::string& str)
		{
			std::istringstream ss {str};
			
			double n;
			ss >> n;
			
			char c = ss.get ();
			if (c == std::istringstream::traits_type::eof ())
				n *= 60.0;
			else
				{
					switch (c)
						{
							case 's': break;
							case 'm': n *= 60.0; break;
							case 'h': n *= 3600.0; break;
							
							default:
								return -1;
						}
				}
			
			return (int)std::round (n);
		}
		
		/*
		 * /mute
		 * 
		 * Mutes a player for a specified amount of time.
		 * 
		 * Permissions:
		 *   - command.admin.mute
		 *       Needed to execute the command.
		 */
		void
		c_mute::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
					return;
			if (!reader.has_next ())
				{ show_usage (pl); return; }
			
			std::string& target_name = reader.next ().as_str ();
			player *target = pl->get_server ().get_players ().find (target_name.c_str ());
			std::string dur_str = "10m";
			if (!target)
				{
					pl->message ("§c * §7Unknown player§f: §c" + target_name);
					return;
				}
			else if (target == pl)
				{
					pl->message ("§c * §7You cannot mute yourself§c.");
					return;
				}
			
			int time = 10 * 60; // ten minutes
			if (reader.has_next ())
				{
					dur_str = reader.next ().as_str ();
					time = secs_from_time_string (dur_str);
					if (time <= 0)
						{
							pl->message ("§c * §7Invalid amount of time§c.");
							pl->message ("§c * §7Format: §ctime§cunit §7[unit = s,m,k]");
							return;
						}
				}
			
			std::string reason = "No reason specified";
			if (reader.has_next ())
				reason = reader.rest ();
			
			server &srv = pl->get_server ();
			if (srv.is_player_muted (target->get_username ()))
				{
					pl->message ("§c * §7Player " + std::string (target->get_colored_nickname ()) + " §7is already muted§c.");
					return;
				}
			
			srv.mute_player (target->get_username (), time);
			{
				std::ostringstream ss;
				ss << "§c > " << target->get_colored_nickname () << " §7has been muted by " <<
					pl->get_colored_nickname () << " §7for §e";
				if (time < 60)
					ss << time << " second" << ((time != 1) ? "s" : "");
				else if (time < 3600)
					ss << (time / 60) << " minute" << (((time / 60) != 1) ? "s" : "");
				else
					ss << (time / 3600) << " hour" << (((time / 3600) != 1) ? "s" : "");
				
				srv.get_players ().message (ss.str ());
			}
			
			srv.get_logger () (LT_SYSTEM) << "Player " << target_name << " has been muted by " << pl->get_username () << "! (duration: " << dur_str << ")" << std::endl;
			if (srv.get_irc ())
				srv.get_irc ()->chan_msg ("§c! " + target_name + " has been muted by " + pl->get_username () + "! §7(duration: §8" + dur_str + "§7)");
		}
	}
}

