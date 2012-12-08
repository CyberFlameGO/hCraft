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

#include "worldc.hpp"
#include "../server.hpp"
#include "../player.hpp"
#include "stringutils.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /physics -
		 * 
		 * Changes the current state of physics of the player's world.
		 * 
		 * Permissions:
		 *   - command.world.physics
		 *       Needed to execute the command.
		 */
		void
		c_physics::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.physics"))
				return;
			
			if (!reader.parse_args (this, pl))
				return;
			
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_usage (pl); return; }
			
			world *wr = pl->get_world ();
			
			std::string& opt = reader.arg (0);
			if (sutils::iequals (opt, "on") || sutils::iequals (opt, "resume"))
				{
					if (wr->physics_state () == PHY_ON)
						{
							pl->message ("§c * §ePhysics are already on§f.");
							return;
						}
					wr->start_physics ();
					wr->get_players ().message ("§a * §6Physics have been turned §aON§6.");
				}
			else if (sutils::iequals (opt, "off"))
				{
					if (wr->physics_state () == PHY_OFF)
						{
							pl->message ("§c * §ePhysics are already off§f.");
							return;
						}
					wr->stop_physics ();
					wr->get_players ().message ("§a * §6Physics have been turned §cOFF§6.");
				}
			else if (sutils::iequals (opt, "pause"))
				{
					if (wr->physics_state () == PHY_PAUSED)
						{
							pl->message ("§c * §ePhysics are already paused§f.");
							return;
						}
					wr->pause_physics ();
					wr->get_players ().message ("§a * §6Physics have been §7PAUSED§6.");
				}
			else
				{
					pl->message ("§7 * §cInvalid option§7: §e" + opt);
				}
		}
	}
}

