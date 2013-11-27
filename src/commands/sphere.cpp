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

#include "commands/sphere.hpp"
#include "system/server.hpp"
#include "player/player.hpp"
#include "util/stringutils.hpp"
#include "util/position.hpp"
#include "drawing/drawops.hpp"
#include <sstream>
#include <vector>
#include <cmath>


namespace hCraft {
	namespace commands {
		
		namespace {
			struct sphere_data {
				blocki bl;
				int radius;
				bool fill;
			};
		}
		
		
		static bool
		on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			sphere_data *data = static_cast<sphere_data *> (pl->get_data ("sphere"));
			if (!data) return true; // shouldn't happen
			
			if (!pl->get_world ()->security ().can_build (pl))
				{
					pl->message ("§4 * §cYou are not allowed to build here§4.");
					pl->delete_data ("sphere");
					return true;
				}
			
			int radius = data->radius;
			if (radius == -1)
				{
					radius = std::round ((vector3 (marked[1]) - vector3 (marked[0])).magnitude ());
				}
			
			dense_edit_stage des (pl->get_world ());
			cond_edit_stage es {des,
				[] (world *w, int x, int y, int z, void *ctx) -> bool
					{
						return w->can_build_at (x, y, z, static_cast<player *> (ctx));
					}, pl};
			
			int blocks;
			draw_ops draw (es);
			if (data->fill)
				blocks = draw.filled_sphere (marked[0], radius, data->bl);
			else
				blocks = draw.sphere (marked[0], radius, data->bl);
			es.commit ();
			
			std::ostringstream ss;
			{
				int fb = es.failed_blocks ();
				if (fb > 0)
					{
						ss << "§7 | " << fb << " §czoned block" << ((fb == 1) ? "" : "s") << " could not be replaced.";
						pl->message (ss.str ());
						ss.str (std::string ());
					}
			}
			ss << "§7 | Circle complete §f- §b" << blocks << " §7blocks modified";
			pl->message (ss.str ());
			
			pl->delete_data ("sphere");
			return true;
		}
		
		/* 
		 * /sphere -
		 * 
		 * Draws a three-dimensional sphere centered at a point.
		 * 
		 * Permissions:
		 *   - command.draw.sphere
		 *       Needed to execute the command.
		 */
		void
		c_sphere::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
		
			reader.add_option ("fill", "f");
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 3)
				{ this->show_summary (pl); return; }
			
			bool do_fill = reader.opt ("fill")->found ();
			
			std::string& str = reader.next ().as_str ();
			if (!sutils::is_block (str))
				{
					pl->message ("§c * §7Invalid block§f: §c" + str);
					return;
				}
			
			blocki bl = sutils::to_block (str);
			if (bl.id == BT_UNKNOWN)
				{
					pl->message ("§c * §7Unknown block§f: §c" + str);
					return;
				}
			
			int radius = -1;
			if (reader.has_next ())
				{
					command_reader::argument arg = reader.next ();
					if (!arg.is_int ())
						{
							pl->message ("§c * §7Usage§f: §e/sphere §cblock §8[§cradius§8]");
							return;
						}
					
					radius = arg.as_int ();
					if (radius <= 0)
						{
							pl->message ("§c * §7Radius must be greater than zero.§f");
							return;
						}
				}
			
			sphere_data *data = new sphere_data {bl, radius, do_fill};
			pl->create_data ("sphere", data,
				[] (void *ptr) { delete static_cast<sphere_data *> (ptr); });
			pl->get_nth_marking_callback ((radius == -1) ? 2 : 1) += on_blocks_marked;
			
			std::ostringstream ss;
			ss << "§5Draw§f: §3" << (do_fill ? "filled" : "hollow")
				 << " sphere §f[§7block§f: §8" << str << "§f, §7radius§f: §8";
			if (radius == -1)
				ss << "?";
			else
				ss << radius;
			ss << "§f]:";
			pl->message (ss.str ());
			
			ss.str (std::string ());
			ss << "§7 | §ePlease mark §b" << ((radius == -1) ? "two" : "one") << " §eblock"
				 << ((radius == -1) ? "s" : "") << "§f.";
			pl->message (ss.str ());
		}
	}
}

