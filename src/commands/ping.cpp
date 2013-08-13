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

#include "commands/miscc.hpp"
#include "server.hpp"
#include "player.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /ping -
		 * 
		 * Displays to the player how much time it takes for the server to both
		 * send and retreive a single keep alive (ping: 0x00) packet (in ms).
		 * 
		 * Permissions:
		 *   - command.misc.ping
		 *       Needed to execute the command.
		 */
		void
		c_ping::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.misc.ping"))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			if (reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			if (reader.has_args ())
				{
					player *target = pl->get_server ().get_players ().find (reader.arg (0).c_str ());
					if (!target)
						{
							pl->message ("§c * §7No such player §f: §c" + reader.arg (0) + "§f.");
							return;
						}
					
					std::ostringstream ss;
					ss << "§a" << target->get_username () << "§e's ping§f: §c" << target->get_ping () << " §emilliseconds§f.";
					pl->message (ss.str ());
					return;
				}
			
			std::ostringstream ss;
			ss << "§ePing§f: §c" << pl->get_ping () << " §emilliseconds§f.";
			pl->message (ss.str ());
		}
	}
}

