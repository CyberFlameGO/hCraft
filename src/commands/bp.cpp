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
#include "stringutils.hpp"
#include "server.hpp"
#include "physics/physics.hpp"
#include <sstream>

#include <iostream> // DEBUG


namespace hCraft {
	namespace commands {
		
		/* 
		 * /bp -
		 * 
		 * Modifies physics properties for individual blocks.
		 * 
		 * Permissions:
		 *   - command.world.bp
		 *       Needed to execute the command.
		 */
		void
		c_block_physics::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			reader.add_option ("tick-rate", "t", true, true);
			reader.add_option ("expire", "e", true, true);
			if (!reader.parse (this, pl))
				return;
			if (reader.no_args ())
				{
					pl->message ("§c * §7Usage§f: §e/bp §cphysics-string");
					return;
				}
			
			int tick_rate = 250;
			auto opt_tick_rate = reader.opt ("tick-rate");
			if (opt_tick_rate->found ())
				{
					auto &arg = opt_tick_rate->arg (0);
					if (!arg.is_int ())
						{
							pl->message ("§c * §7Argument to §c\\tick-rate §7option must be an integer§c.");
							return;
						}
					
					tick_rate = arg.as_int ();
				}
			tick_rate /= 50; 
			
			int expire = (-1) * 50;
			auto opt_expire = reader.opt ("expire");
			if (opt_expire->found ())
				{
					auto &arg = opt_tick_rate->arg (0);
					if (!arg.is_int ())
						{
							pl->message ("§c * §7Argument to §c\\expire §7option must be an integer§c.");
							return;
						}
					
					expire = arg.as_int ();
				}
			expire /= 50; 
			
			
			if (pl->selections.empty ())
				{ pl->message ("§c * §7You§f'§7re not selecting anything§f."); return; }
			world_selection *sel = pl->curr_sel;
			world *w = pl->get_world ();
			
			// build our physics_params object
			std::string phy_str = reader.rest ();
			physics_params params;
			if (!physics_params::build (phy_str, params, expire))
				{
					pl->message ("§c * §7Invalid physics string§c.");
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
											w->queue_physics (x, y, z, 0, nullptr, tick_rate, &params, nullptr);
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

