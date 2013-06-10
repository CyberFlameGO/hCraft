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
#include "player.hpp"
#include "world.hpp"
#include "position.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /wsetspawn -
		 * 
		 * Changes a world's default spawn position.
		 * 
		 * Permissions:
		 *   - command.world.wsetspawn
		 *       Needed to execute the command.
		 */
		void
		c_wsetspawn::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			world *w = pl->get_world ();
			entity_pos dest = pl->pos;
			
			w->set_spawn (dest);
			pl->teleport_to (dest);
			pl->message ("§3Spawn location updated§b.");
		}
	}
}

