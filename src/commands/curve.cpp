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

#include "commands/curve.hpp"
#include "player/player.hpp"
#include "system/server.hpp"
#include "util/stringutils.hpp"
#include "util/utils.hpp"
#include "drawing/drawops.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		namespace {
			struct curve_data {
				sparse_edit_stage es;
				std::vector<vector3> points;
				blocki bl;
				
				curve_data (world *wr, blocki bl)
					: es (wr)
				{
					this->bl = bl;
				}
			};
		}
		
		
		static bool
		on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			curve_data *data = static_cast<curve_data *> (pl->get_data ("curve"));
			if (!data) return true; // shouldn't happen
			
			if (!pl->get_world ()->security ().can_build (pl))
				{
					pl->message ("§4 * §cYou are not allowed to build here§4.");
					pl->delete_data ("curve");
					return true;
				}
			
			if (!data->es.get_world ()->can_build_at (marked[0].x, marked[0].y, marked[0].z, pl))
				{
					pl->message ("§4 * §cCannot mark that block§4.");
					return false;
				}
			
			sparse_edit_stage& ses = data->es;
			cond_edit_stage es {ses,
				[] (world *w, int x, int y, int z, void *ctx) -> bool
					{
						return w->can_build_at (x, y, z, static_cast<player *> (ctx));
					}, pl};
			
			std::vector<vector3>& points = data->points;
			points.push_back (marked[0]);
			if (points.size () > 1)
				{
					es.restore_to (pl);
					es.clear ();
				}
			
			vector3 first = points[0];
			if (points.size () > 1)
				{
					draw_ops draw (es);
					draw.curve (points, data->bl);
					
					vector3 last = points[points.size () - 1];
					if ( ((int)first.x == (int)last.x) && 
							 ((int)first.y == (int)last.y) && 
							 ((int)first.z == (int)last.z) )
						es.set (last.x, last.y, last.z, BT_WOOL, 1);
					else
						es.set (last.x, last.y, last.z, BT_WOOL, 14);
				}
			
			es.set (first.x, first.y, first.z, BT_WOOL, 11);
			es.preview_to (pl);
			
			{
				std::ostringstream ss;
				int fb = es.failed_blocks ();
				if (fb > 0)
					{
						ss << "§7 | " << fb << " §czoned block" << ((fb == 1) ? "" : "s") << " could not be replaced.";
						pl->message (ss.str ());
					}
			}
			
			return false;
		}
		
		
		static void
		draw_curve (player *pl)
		{
			curve_data *data = static_cast<curve_data *> (pl->get_data ("curve"));
			if (!data)
				{
					pl->message ("§4 * §cYou are not drawing any curves§4.");
					return;
				}
			
			sparse_edit_stage& ses = data->es;
			cond_edit_stage es {ses,
				[] (world *w, int x, int y, int z, void *ctx) -> bool
					{
						return w->can_build_at (x, y, z, static_cast<player *> (ctx));
					}, pl};
			
			std::vector<vector3>& points = data->points;
			if (points.empty ())
				{
					pl->stop_marking ();
					pl->delete_data ("curve");
					return;
				}
			
			es.restore_to (pl);
			es.clear ();
			
			draw_ops draw (es);
			int blocks = draw.curve (points, data->bl);
			es.commit ();
			
			pl->stop_marking ();
			pl->delete_data ("curve");
			
			std::ostringstream ss;
			ss << "§7 | Curve complete §f- §b" << blocks << " §7blocks modified";
			pl->message (ss.str ());
		}
		
		
		/* 
		 * /curve -
		 * 
		 * Draws a curve between a given set of points.
		 * Unlike /bezier, this command will attempt to make the curve pass through
		 * ALL points except for the first and last.
		 * 
		 * Permissions:
		 *   - command.draw.curve
		 *       Needed to execute the command.
		 */
		void
		c_curve::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
			
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			std::string& str = reader.next ().as_str ();
			if (sutils::iequals (str, "stop"))
				{
					draw_curve (pl);
					return;
				}
			
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
			
			curve_data *data = new curve_data (pl->get_world (), bl);
			pl->create_data ("curve", data,
				[] (void *ptr) { delete static_cast<curve_data *> (ptr); });
			pl->get_nth_marking_callback (1) += on_blocks_marked;
			
			pl->message ("§5Draw§f: §3Catmull-Rom spline §f[§7block§f: §8" + sutils::get_slot_name (bl) + "§f]:");
			pl->message ("§7 | §ePlease mark the required points§f.");
			pl->message ("§7 | §eType §c/curve stop §eto stop§f."); 
		}
	}
}

