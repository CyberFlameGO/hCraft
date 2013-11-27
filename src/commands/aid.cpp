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

#include "commands/aid.hpp"
#include "player/player.hpp"
#include "world/world.hpp"
#include "util/stringutils.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /aid -
		 * 
		 * Places a block at the player's feet.
		 * 
		 * Permissions:
		 *   - command.draw.aid
		 *       Needed to execute the command.
		 */
		void
		c_aid::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.aid"))
					return;
		
			if (!reader.parse (this, pl))
					return;
			if (reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			blocki bl = {BT_WOOL, 4};
			if (reader.has_next ())
				{
					std::string& str = reader.next ().as_str ();
					if (sutils::iequals (str, "mark") || sutils::iequals (str, "m"))
						{
							block_pos pos = pl->pos;
							if (pl->mark_block (pos.x, pos.y, pos.z))
								{
									std::ostringstream ss;
									ss << "§7 | {§8" << pos.x << "§7, §8" << pos.y << "§7, §8" << pos.z << "§7} has been marked.";
									pl->message (ss.str ());
								}
							else
								pl->message ("§c * §7Nothing to mark");
							return;
						}
					
					if (!sutils::is_block (str))
						{
							pl->message ("§c * §7Invalid block§f: §c" + str);
							return;
						}
			
					bl = sutils::to_block (str);
					if (bl.id == BT_UNKNOWN)
						{
							pl->message ("§c * §7Unknown block§f: §c" + str);
							return;
						}
				}
			
			if (!pl->get_world ()->security ().can_build (pl)
				|| !pl->get_world ()->can_build_at ((int)pl->pos.x, (int)pl->pos.y, (int)pl->pos.z, pl))
				{
					pl->message ("§4 * §cYou are not allowed to build here§4.");
					return;
				}
			
			pl->get_world ()->queue_update ((int)pl->pos.x, (int)pl->pos.y, (int)pl->pos.z,
				bl.id, bl.meta);
			{
				std::ostringstream ss;
				
				ss << "§8A block of §b";
				block_info *binf = block_info::from_id (bl.id);
				if (binf)
					ss << binf->name;
				else
					ss << bl.id;
				if (bl.meta != 0)
					ss << "§3:" << (int)bl.meta;
				ss << " §8has been placed at your feet";
				
				pl->message (ss.str ());
			}
		}
	}
}

