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

#include "commands/worldc.hpp"
#include "server.hpp"
#include "player.hpp"
#include "world.hpp"
#include "stringutils.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>


namespace hCraft {
	namespace commands {
		
		static void
		_world_list (player *pl)
		{
			std::vector<world *> worlds;
			pl->get_server ().get_worlds ().populate (worlds);
			
			std::ostringstream ss;
			if (worlds.size () == 1)
				pl->message ("§eThere is currently only §bone §eworld loaded§f:");
			else
				{
					ss << "§eThere are currently §b" << worlds.size () << " §eworlds loaded§f:";
					pl->message (ss.str ());
					ss.str (std::string ());
				}
				
			// sort list alphabetically
			std::sort (worlds.begin (), worlds.end (),
				[] (const world *a, const world *b) -> bool
					{
						return std::strcmp (a->get_name (), b->get_name ()) < 0;
					});
			
			ss << "§3    ";
			for (world *w : worlds)
				ss << w->get_colored_name () << " ";
			
			pl->message_spaced (ss.str ());
		}
		
		
		/* 
		 * /world - 
		 * 
		 * Teleports the player to a requested world.
		 * 
		 * Permissions:
		 *   - command.world.world
		 *       Needed to execute the command.
		 */
		void
		c_world::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.world"))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			if (reader.no_args ())
				{
					pl->message ("§eYou are currently in§f: §b" + std::string (pl->get_world ()->get_colored_name ()));
					return;
				}
			else if (reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			std::string& world_name = reader.arg (0);
			if (sutils::iequals (world_name, "list"))
				{
					_world_list (pl);
					return;
				}
			
			world *wr = pl->get_server ().get_worlds ().find (world_name.c_str ());
			if (!wr)
				{
					pl->message ("§c * §7Cannot find world§f: §c" + world_name);
					return;
				}
			
			world *prev_world = pl->get_world ();
			if (wr == prev_world)
				{
					pl->message ("§eAlready there§f." );
					return;
				}
			
			if (!pl->has_access (wr->get_join_perms ()))
				{
					pl->message ("§4 * §cYou are not allowed to go there§4.");
					return;
				}
			
			pl->join_world (wr);
		}
	}
}
