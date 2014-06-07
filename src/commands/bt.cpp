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

#include "commands/bt.hpp"
#include "player/player.hpp"
#include "world/world.hpp"
#include "util/stringutils.hpp"
#include "system/server.hpp"
#include "physics/physics.hpp"
#include "util/cistring.hpp"
#include <sstream>
#include <unordered_map>


namespace hCraft {
	namespace commands {
		
		static int
		_extra_from_string (const std::string& str)
		{
			static std::unordered_map<cistring, int> _map {
				{ "door", BE_DOOR },
				{ "portal", BE_PORTAL },
			};
			
			auto itr = _map.find (str.c_str ());
			if (itr == _map.end ())
				return -1;
			return itr->second;
		}
		
		/* 
		 * /block-type -
		 * 
		 * Modifies the type of a selected block.
		 * 
		 * Permissions:
		 *   - command.world.block-type
		 *       Needed to execute the command.
		 */
		void
		c_block_type::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			if (reader.arg_count () != 1)
				{
					pl->message ("§c * §7Usage§f: §e/bt §ctype");
					return;
				}
			
			if (pl->selections.empty ())
				{ pl->message ("§c * §7You§f'§7re not selecting anything§f."); return; }
			world_selection *sel = pl->curr_sel;
			world *w = pl->get_world ();
			
			std::string tstr = reader.arg (0);
			int ex = _extra_from_string (tstr);
			if (ex == -1)
				{
					pl->message ("§c * §7Invalid block type§f: §c" + tstr);
					return;
				}
			
			int blocks = 0;
			
			block_pos smin = sel->min (), smax = sel->max ();
			if (smin.y < 0) smin.y = 0;
			if (smin.y > 255) smin.y = 255;
			if (smax.y < 0) smax.y = 0;
			if (smax.y > 255) smax.y = 255;
			for (int x = smin.x; x <= smax.x; ++x)
				for (int y = smin.y; y <= smax.y; ++y)
					for (int z = smin.z; z <= smax.z; ++z)
						{
							if (!w->in_bounds (x, y, z)) continue;
							if (sel->contains (x, y, z))
								{
									int id = w->get_id (x, y, z);
									if (id != BT_AIR)
										{
											w->set_extra (x, y, z, ex);
											++ blocks;
										}
								}
						}
			
			{
				std::ostringstream ss;
				ss << "§7 | §b" << blocks << " §7blocks have been modified.";
				pl->message (ss.str ());
			}
		}
	}
}

