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
		
		static void
		show_usage (player *pl)
		{
			pl->message ("§c * §7Usage§f: §e/unmute §cplayer");
		}
		
		/*
		 * /unmute
		 * 
		 * Unmutes a player.
		 * 
		 * Permissions:
		 *   - command.admin.unmute
		 *       Needed to execute the command.
		 */
		void
		c_unmute::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
					return;
			if (reader.arg_count () != 1)
				{ show_usage (pl); return; }
			
			std::string& target_name = reader.next ().as_str ();
			player *target = pl->get_server ().get_players ().find (target_name.c_str ());
			if (!target)
				{
					pl->message ("§c * §7Unknown player§f: §c" + target_name);
					return;
				}
			
			server &srv = pl->get_server ();
			if (!srv.is_player_muted (target->get_username ()))
				{
					pl->message ("§c * §7Player " + std::string (target->get_colored_nickname ()) + " §7is not muted§c.");
					return;
				}
			
			srv.unmute_player (target->get_username ());
			pl->message ("§7 | " + std::string (target->get_colored_username ()) + " §ehas been unmuted§7.");
			target->message ("§9You are no longer muted§3.");
		}
	}
}

