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

#include "commands/kill.hpp"
#include "player/player.hpp"
#include "system/server.hpp"
#include "system/messages.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		//contributed by Juholei1
		/* 
		 * /kill 
		 * 
		 * Kills the executor or a specified player.
		 * 
		 * Permissions:
		 *   - command.misc.kill
		 *       Needed to execute the command.
		 *   - command.misc.kill.others
		 *       Needed to kill others.
		 */
		void
		c_kill::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.misc.kill"))
					return;
		
			if (!reader.parse (this, pl))
					return;
			if (reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			player *target = pl;
			if (reader.arg_count () == 1)
				{
					std::string plname = reader.next ().as_str ();
					target = pl->get_server ().get_players ().find (plname.c_str ());
					if (!target)
						{
							pl->message ("§c * §7No such player§f: §c" + plname);
							return;
						}
				}
			
			if (pl != target)
				{
					if (!pl->has ("command.misc.kill.others"))
						{
							pl->message (messages::not_allowed ());
							return;
						}
					
					std::ostringstream ss;
					ss << target->get_colored_username () << " §chas been killed by you§f.";
					pl->message (ss.str ());
				}
			else
				pl->message ("§cCommitted suicide");
			
			// fixed
			target->kill ();
		}
	}
}

