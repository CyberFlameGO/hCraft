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

#include "commands/chatc.hpp"
#include "server.hpp"
#include "player.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /me -
		 * 
		 * Broadcasts an IRC-style action message (in the form of: * user1234
		 * <message>).
		 * 
		 * Permissions:
		 *   - command.chat.me
		 *       Needed to execute the command.
		 */
		void
		c_me::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.chat.me"))
				return;
			
			if (!reader.parse (this, pl))
				return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::ostringstream ss;
			ss << "§" << pl->get_rank ().main_group->color << "* "
				 << pl->get_username () << " " << reader.get_arg_string ();
			pl->get_world ()->get_players ().message (ss.str ());
		}
	}
}

