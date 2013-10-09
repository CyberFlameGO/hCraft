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

#include "commands/rules.hpp"
#include "system/server.hpp"
#include "player/player.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /rules
		 * 
		 * Displays important server rules.
		 * 
		 * Permissions:
		 *   - command.info.rules
		 *       Needed to execute the command.
		 */
		void
		c_rules::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			for (const std::string& str : pl->get_server ().msgs.rules)
				pl->message (messages::compile (str, messages::environment (pl)));
		}
	}
}

