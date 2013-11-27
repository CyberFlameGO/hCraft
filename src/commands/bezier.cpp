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

#include "commands/bezier.hpp"
#include "player/player.hpp"
#include "system/server.hpp"
#include "util/stringutils.hpp"
#include "util/position.hpp"
#include "drawing/drawops.hpp"
#include <sstream>
#include <vector>


namespace hCraft {
	namespace commands {
		
		namespace {
			struct bezier_data {
				std::vector<vector3> points;
				sparse_edit_stage es;
				blocki bl;
				int order;
				
				bezier_data (world *w, blocki bl, int order)
					: es (w)
				{
					this->bl = bl;
					this->order = order;
				}
			};
		}
		
		
		static bool
		on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			bezier_data *data = static_cast<bezier_data *> (pl->get_data ("bezier"));
			if (!data) return true; // shouldn't happen
			
			if (!data->es.get_world ()->can_build_at (marked[0].x, marked[0].y, marked[0].z, pl))
				{
					pl->message ("§4 * §cCannot mark that block§4.");
					return false;
				}
			
			edit_stage &ses = data->es;
			cond_edit_stage es {ses,
				[] (world *w, int x, int y, int z, void *ctx) -> bool
					{
						return w->can_build_at (x, y, z, static_cast<player *> (ctx));
					}, pl};
			
			if (es.get_world () != pl->get_world ())
				{
					pl->message ("§4 * §cWorlds changed§4.");
					pl->delete_data ("bezier");
					return true;
				}
			
			if (!pl->get_world ()->security ().can_build (pl))
				{
					pl->message ("§4 * §cYou are not allowed to build here§4.");
					pl->delete_data ("bezier");
					return true;
				}
			
			std::vector<vector3>& points = data->points;
			points.push_back (marked[0]);
			if (points.size () == 1)
				{
					es.set (marked[0].x, marked[0].y, marked[0].z, data->bl.id, data->bl.meta);
					es.preview_to (pl);
					return false;
				}
			
			if (points.size () != 2)
				{
					es.restore_to (pl);
					es.clear ();
				}
			
			draw_ops draw (es);
			int blocks = draw.bezier (points, data->bl);
			
			{
				std::ostringstream ss;
				int fb = es.failed_blocks ();
				if (fb > 0)
					{
						ss << "§7 | " << fb << " §czoned block" << ((fb == 1) ? "" : "s") << " could not be replaced.";
						pl->message (ss.str ());
					}
			}
			
			if (points.size () < (size_t)(data->order + 1))
				{
					es.preview_to (pl);
					return false;
				}
			
			es.commit ();
			pl->delete_data ("bezier");
			
			std::ostringstream ss;
			ss << "§7 | Bezier curve complete §f - §7~§b" << blocks << " §7blocks modified";
			pl->message (ss.str ());
			return true;
		}
		
		/* 
		 * /bezier -
		 * 
		 * Draws a beizer curvee using between user-specified control points.
		 * The curve will pass through the first and last points, but not necessarily
		 * through the rest.
		 * 
		 * Permissions:
		 *   - command.draw.bezier
		 *       Needed to execute the command.
		 */
		void
		c_bezier::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.bezier"))
					return;
		
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 2)
				{ this->show_summary (pl); return; }
			
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
			
			int order = 2;
			if (reader.has_next ())
				{
					command_reader::argument arg = reader.next ();
					if (!arg.is_int ())
						{
							pl->message ("§c * §7Usage§f: §e/bezier §cblock §8[§corder§8]");
							return;
						}
					
					order = arg.as_int ();
					if (order <= 0 || order > 20)
						{
							pl->message ("§c * §7Invalid bezier curve order §f(§7Must be between §b1-20§f)");
							return;
						}
				}
			
			bezier_data *data = new bezier_data (pl->get_world (), bl, order);
			pl->create_data ("bezier", data,
				[] (void *ptr) { delete static_cast<bezier_data *> (ptr); });
			pl->get_nth_marking_callback (1) += on_blocks_marked;
			
			std::ostringstream ss;
			ss << "§5Draw§f: §3bezier curve §f[§7order§f: §8" << order << "§f, §7block§f: §8"
				 << block_info::from_id (bl.id)->name;
			if (bl.meta != 0)
				ss << ':' << (int)bl.meta;
			ss << "§f]:";
			pl->message (ss.str ());
			
			ss.str (std::string ()); ss.clear ();
			ss << "§7 | §ePlease mark §b" << (order + 1) << " §eblocks§f.";
			pl->message (ss.str ());
		}
	}
}

