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

#include "drawc.hpp"
#include "player.hpp"
#include "position.hpp"
#include "stringutils.hpp"
#include <vector>
#include <sstream>

#include "selection/cuboid_selection.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /selection -
		 * 
		 * A multipurpose command for changing the current selection(s) managed
		 * by the calling player.
		 * 
		 * Permissions:
		 *   - command.draw.selection
		 *       Needed to execute the command.
		 */
		void
		c_selection::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.selection"))
					return;
		
			if (!reader.parse_args (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			if (reader.arg_count () == 1)
				{
					if (reader.arg_is_int (0))
						{
							/* 
							 * /sel <n>
							 * 
							 * Sets the @{n}th point of the current selection to the player's
							 * position.
							 */
							
							int num = reader.arg_as_int (0);
							if (num <= 0)
								{ pl->message ("§c * §eInvalid position number§f."); return; }
					
							if (pl->selections.empty ())
								{ pl->message ("§c * §eYou§f'§ere not selecting anything§f."); return; }
							world_selection *selection = pl->selections[pl->sel_index];
							if (num > selection->needed_points ())
								{
									std::ostringstream ss;
									ss << "§c * §eThe selection that you§f'§ere making requires only §f"
										 << selection->needed_points ()<< " §epoints§f.";
									pl->message (ss.str ());
									return;
								}
					
							block_pos curr_pos = pl->get_pos ();
							selection->set (num - 1, curr_pos);
					
							{
								std::ostringstream ss;
								ss << "§a * §ePoint §c#" << num << " §ehas been set to {§ax: §c"
									 << curr_pos.x << "§e, §ay: §c" << curr_pos.y << "§e, §az: §c"
									 << curr_pos.z << "§e}§f.";
								pl->message (ss.str ());
							}
						}
					else
						{
							/* 
							 * /sel <type>
							 * 
							 * Creates a new selection of the type <type>.
							 */
							
							block_pos curr_pos = pl->get_pos ();
							std::string& arg   = reader.arg (0);
							
							if (sutils::iequals (arg, "cuboid"))
								{
									pl->message ("- §eSwitched to a new selection of type§f: §acuboid");
									pl->selections.push_back (new cuboid_selection (
										curr_pos, curr_pos));
									pl->sel_index = pl->selections.size () - 1;
								}
							else
								{
									pl->message ("§c * §eInvalid selection type§f: §c" + arg);
									return;
								}
						}
				}
		}
	}
}

