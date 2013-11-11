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

#include "commands/top.hpp"
#include "system/server.hpp"
#include "player/player.hpp"
#include "world/world.hpp"
#include "util/position.hpp"
#include "world/chunk.hpp"
#include "util/stringutils.hpp"
#include "system/messages.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /top -
		 * 
		 * Teleports the player to the topmost non-air block in their current position.
		 * 
		 * Permissions:
		 *   - command.world.top
		 *       Needed to execute the command.
		 */
		void
		c_top::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.top"))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			if (!reader.no_args ())
				{ this->show_summary (pl); return; }
			else
				{
					/* 
					 * Teleport to the topmost non-air block
					 *   /top
					 */
					entity_pos curr_pos = pl->pos;
					world* wr = pl->get_world();
					int x, y, z;
					
					x = curr_pos.x - (curr_pos.x < 0 ? 1 : 0);
					y = curr_pos.y;
					z = curr_pos.z - (curr_pos.z < 0 ? 1 : 0);
					
					chunk* c = wr->get_chunk_at(x, z);
					if(!c)
						return; // Might happen, not sure \o/
					
					int cx, cz;
					cx = x % 16;
					cz = z % 16;
					if(cx < 0)
						cx += 16;
					if(cz < 0)
						cz += 16;
					
					int ny;
					for(ny = 256 ; ny>0 ;)
						if(c->get_block(cx, --ny, cz).id != 0)
							break;
					ny += 1;
					
					std::ostringstream ss;
					ss << "§eTeleporting §9"
						 << (ny - y) << "§e blocks vertically";
					pl->message (ss.str ());
					
					entity_pos target = entity_pos (block_pos (x, ny, z))
						.set_rot (curr_pos.r, curr_pos.l);
					target.x += .5;
					target.z += .5;
					pl->teleport_to (target);
				}
		}
	}
}

