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

#include "commands/drawc.hpp"
#include "player.hpp"
#include <iostream> // DEBUG


namespace hCraft {
	namespace commands {
		
		/* 
		 * /fill -
		 * 
		 * Fills all visible selected regions with a block.
		 * 
		 * Permissions:
		 *   - command.draw.fill
		 *       Needed to execute the command.
		 */
		void
		c_fill::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.fill"))
					return;
		
			if (!reader.parse_args (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			if (!reader.is_arg_block (0))
				{ pl->message ("§c * §eInvalid block§f: §c" + reader.arg (0)); return; }
			
			block_data out_bd = reader.arg_as_block (0);
			if (out_bd.id > 145 || out_bd.meta > 15)
				{
					pl->message ("§c * §eInvalid block§f: §c" + reader.arg (0));
					return;
				}
			
			// fill all selections
			world *wr = pl->get_world ();
			for (world_selection *sel : pl->selections)
				{
					if (!sel->visible ()) continue;
					
					block_pos min_p = sel->min ();
					block_pos max_p = sel->max ();
					
					for (int x = min_p.x; x <= max_p.x; ++x)
						for (int y = min_p.y; y <= max_p.y; ++y)
							for (int z = min_p.z; z <= max_p.z; ++z)
								{
									if (sel->contains (x, y, z))
										{
											wr->queue_update (x, y, z, out_bd.id, out_bd.meta);
										}
								}
				}
		}
	}
}

