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

#include "commands/worlds.hpp"
#include "player/player.hpp"
#include "world/world.hpp"
#include "util/stringutils.hpp"
#include "system/server.hpp"
#include <sstream>
#include <algorithm>
#include <cstring>


namespace hCraft {
	namespace commands {
		
		/*
		 * /worlds
		 * 
		 * Displays a list of all loaded worlds.
		 * 
		 * Permissions:
		 *   - command.world.worlds
		 *       Needed to execute the command.
		 */
		void
		c_worlds::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
		
			if (!reader.parse (this, pl))
					return;
			
			server& srv = pl->get_server ();
			std::vector<world *> worlds;
			srv.get_worlds ().populate (worlds);
			
			// sort alphabetically
			std::sort (worlds.begin (), worlds.end (),
				[] (world *a, world *b) -> bool
					{
						return (std::strcmp (a->get_name (), b->get_name ()) < 0);
					});
			
			std::ostringstream ss;
			ss << "§3There " << (worlds.size () == 1 ? "is" : "are") << " currently §e"
				<< worlds.size () << " §3world" << (worlds.size () == 1 ? "" : "s") << " loaded§e:";
			pl->message (ss.str ());
			
			ss.str (std::string ());
			ss << "§7   ";
			for (world *w : worlds)
				{
					ss << " " << w->get_colored_name ();
				}
			pl->message_spaced (ss.str ());
		}
	}
}

