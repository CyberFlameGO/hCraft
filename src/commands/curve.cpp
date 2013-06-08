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
#include "server.hpp"
#include "stringutils.hpp"
#include "utils.hpp"
#include "drawops.hpp"
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
			
			sparse_edit_stage& es = data->es;
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
					draw.draw_curve (points, data->bl);
					
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
			return false;
			
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
			
			sparse_edit_stage& es = data->es;
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
			draw.draw_curve (points, data->bl);
			es.commit ();
			
			pl->stop_marking ();
			pl->delete_data ("curve");
			pl->message ("§3Curve complete");
			return;
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
			
			pl->message ("§8Curve §7(§8Block§7: §b" + str + "§7):");
			pl->message ("§8 * §7Please mark the required points§8.");
			pl->message ("§8 * §7Type §c/curve stop §7to stop§8."); 
		}
	}
}

