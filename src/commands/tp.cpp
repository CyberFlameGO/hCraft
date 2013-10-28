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

#include "commands/tp.hpp"
#include "system/server.hpp"
#include "player/player.hpp"
#include "util/stringutils.hpp"
#include "system/messages.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /tp -
		 * 
		 * Teleports the player to a requested location.
		 * 
		 * Permissions:
		 *   - command.world.tp
		 *       Needed to execute the command.
		 *   - command.world.tp.coords
		 *       Allows the user to teleport to specific xyz coordinates.
		 *   - command.world.tp.others
		 *       Allows the user to teleport players other than themselves.
		 */
		void
		c_tp::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.tp"))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			entity_pos curr_pos = pl->pos;
			
			if (reader.no_args () || reader.arg_count () > 3)
				{ this->show_summary (pl); return; }
			else if (reader.arg_count () == 1)
				{
					/* 
					 * Teleport to a player.
					 *   /tp <player>
					 */
					
					const std::string& target_name = reader.arg (0);
					player *target = pl->get_server ().get_players ().find (
						target_name.c_str ());
					if (!target)
						{
							pl->message ("§c * §7Player §c" + target_name + " §7is not online§f.");
							return;
						}
					else if (target == pl)
						{
							pl->message ("§eAlready there§f.");
							return;
						}
					
					// Teleport to the target player's world (if necessary).
					if (target->get_world () != pl->get_world ())
						{
							if (!pl->has ("command.world.goto"))
								{
									pl->message ("§c * §7Player " + std::string (
										target->get_colored_username ()) + " §7is in another world§f.");
									return;
								}
							
							if (!target->get_world ()->security ().can_join (pl))
								{
									pl->message ("§4 * §cYou are not allowed to go into that player's world§4.");
									return;
								}
							
							pl->join_world_at (target->get_world (), target->pos);
							return;
						}
					else
						{
							pl->teleport_to (target->pos);
						}
					
					pl->message ("§eTeleported to " + std::string (target->get_colored_username ()));
				}
			else if (reader.arg_count () == 2)
				{
					/* 
					 * Teleport player to player.
					 *   /tp <src> <dest>
					 */
					
					if (!pl->has ("command.world.tp.others"))
						{
							pl->message (messages::not_allowed ());
							return;
						}
					
					// src
					const std::string& src_name = reader.arg (0);
					player *src = pl->get_server ().get_players ().find (src_name.c_str ());
					if (!src)
						{
							pl->message ("§c * §7Player §c" + src_name + " §7is not online§f.");
							return;
						}
					
					// dest
					const std::string& dest_name = reader.arg (0);
					player *dest = pl->get_server ().get_players ().find (dest_name.c_str ());
					if (!dest)
						{
							pl->message ("§c * §7Player §c" + dest_name + " §7is not online§f.");
							return;
						}
					
					// Teleport to the target player's world (if necessary).
					if (src->get_world () != dest->get_world ())
						{
							if (!pl->has ("command.world.goto"))
								{
									pl->message ("§c * §7Players are in different worlds§f.");
									return;
								}
							
							if (!dest->get_world ()->security ().can_join (src))
								{
									pl->message ("§c * " + std::string (src->get_colored_username ()) + " §7is not allowed in that world§c.");
									return;
								}
							
							src->join_world_at (dest->get_world (), dest->pos);
							return;
						}
					else
						{
							src->teleport_to (dest->pos);
						}
					
					src->message ("§eYou have been teleported to " + std::string (dest->get_colored_username ()));
					if (pl != src)
						pl->message (std::string (src->get_colored_username ()) + " §ehas been teleported to "
							+ dest->get_colored_username ());
				}
			else if (reader.arg_count () == 3)
				{
					/* 
					 * Teleport to exact (integer) coordinates.
					 *   /tp <x> <y> <z>
					 */
					int x, y, z;
					
					if (!sutils::is_int (reader.arg (0)) ||
							!sutils::is_int (reader.arg (1)) ||
							!sutils::is_int (reader.arg (2)))
						{
							pl->message ("§c * §7Invalid coordinates §f(§7must be integers§f).");
							return;
						}
					
					x = sutils::to_int (reader.arg (0));
					y = sutils::to_int (reader.arg (1));
					z = sutils::to_int (reader.arg (2));
					
					std::ostringstream ss;
					ss << "§eTeleporting to (§9"
						 << x << "§f, §9"
						 << y << "§f, §9"
						 << z << "§e)";
					pl->message (ss.str ());
					
					pl->teleport_to (entity_pos (block_pos (x, y, z))
						.set_rot (curr_pos.r, curr_pos.l));
				}
		}
	}
}

