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

#include "commands/goto.hpp"
#include "system/server.hpp"
#include "player/player.hpp"
#include "world/world.hpp"
#include "util/stringutils.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /goto - 
		 * 
		 * Teleports the player to a requested world.
		 * 
		 * Permissions:
		 *   - command.world.goto
		 *       Needed to execute the command.
		 */
		void
		c_goto::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.goto"))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			if (reader.no_args ())
				{
					pl->message ("§c * §7Usage§f: §e/goto §cworld");
					return;
				}
			else if (reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			std::string world_name = reader.arg (0);
			world *wr = pl->get_server ().get_worlds ().find (world_name.c_str ());
			if (!wr)
				{
					pl->message ("§c * §7Cannot find world§f: §c" + world_name);
					return;
				}
			
			world *prev_world = pl->get_world ();
			if (wr == prev_world)
				{
					pl->message ("§c * §7Already there§c." );
					return;
				}
			
			if (!wr->security ().can_join (pl))
				{
					pl->message ("§4 * §cYou are not allowed to go there§4.");
					return;
				}
			
			pl->join_world (wr);
		}
	}
}
