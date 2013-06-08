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

#include "commands/worldc.hpp"
#include "server.hpp"
#include "player.hpp"
#include "world.hpp"
#include <string>
#include <sstream>


namespace hCraft {
	namespace commands {
		
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
					pl->message ("§eYou are currently in§f: §b" + std::string (pl->get_world ()->get_name ()));
					return;
				}
			else if (reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			std::string& world_name = reader.arg (0);
			world *wr = pl->get_server ().find_world (world_name.c_str ());
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
			
			pl->join_world (wr);
			
			std::ostringstream leave_ss;
			leave_ss << "§c << §" << pl->get_colored_nickname () << " §7has departed to §9" << wr->get_name ();
			std::string leave_msg = leave_ss.str ();
			prev_world->get_players ().all (
				[&leave_msg] (player *pl)
					{
						pl->message (leave_msg);
					});
			
			std::ostringstream enter_ss;
			enter_ss << "§a >> §" << pl->get_colored_nickname () << " §7has entered the world";
			std::string enter_msg = enter_ss.str ();
			wr->get_players ().all (
				[&enter_msg] (player *pl)
					{
						pl->message (enter_msg);
					}, pl);
			
			enter_ss.clear (); enter_ss.str (std::string ());
			enter_ss << "§a >> §" << pl->get_colored_nickname () << " §7has entered the world §b" << wr->get_name ();
			pl->message (enter_ss.str ());
		}
	}
}
