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
#include "server.hpp"
#include "stringutils.hpp"
#include "position.hpp"
#include "drawops.hpp"
#include <sstream>
#include <vector>
#include <cmath>


namespace hCraft {
	namespace commands {
		
		namespace {
			struct polygon_data {
				std::vector<vector3> points;
				sparse_edit_stage es;
				blocki bl;
				bool fill;
				
				polygon_data (world *wr, blocki bl, bool fill)
					: es (wr)
				{
					this->bl = bl;
					this->fill = fill;
				}
			};
		}
		
		
		static bool
		on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			polygon_data *data = static_cast<polygon_data *> (pl->get_data ("polygon"));
			if (!data) return true; // shouldn't happen
			
			if (data->es.get_world () != pl->get_world ())
				{
					pl->message ("§4 * §cWorlds changed§4.");
					pl->delete_data ("polygon");
					return true;
				}
			
			vector3 pt = marked[0];
			if (!data->points.empty ())
				pt.y = data->points[0].y;
			data->points.push_back (pt);
			
			// preview
			if (data->points.size () > 1)
				{
					data->es.restore_to (pl);
					data->es.reset ();
				}
			draw_ops draw (data->es);
			draw.draw_polygon (data->points, data->bl);
			data->es.preview_to (pl);
			
			return false;
		}
		
		
		static void
		draw_polygon (player *pl)
		{
			polygon_data *data = static_cast<polygon_data *> (pl->get_data ("polygon"));
			if (!data)
				{
					pl->message ("§4 * §cYou are not drawing any polygons§4.");
					return;
				}
			
			if (data->es.get_world () != pl->get_world ())
				{
					pl->message ("§4 * §cWorlds changed§4.");
					pl->stop_marking ();
					pl->delete_data ("polygon");
					return;
				}
			else if (data->points.empty ())
				{
					pl->stop_marking ();
					pl->delete_data ("polygon");
					return;
				}
			
			sparse_edit_stage &es = data->es;
			es.restore_to (pl);
			es.reset ();
			
			draw_ops draw (es);
			draw.draw_polygon (data->points, data->bl);
			es.commit ();
			
			pl->stop_marking ();
			pl->delete_data ("polygon");
			pl->message ("§3Polygon complete");
		}
		

		
		/* 
		 * /polygon -
		 * 
		 * Creates a wireframe rendering of the polygon specified by marked points.
		 * 
		 * Permissions:
		 *   - command.draw.polygon
		 *       Needed to execute the command.
		 */
		void
		c_polygon::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
		
			reader.add_option ("fill", "f");
			if (!reader.parse_args (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 3)
				{ this->show_summary (pl); return; }
			
			bool do_fill = reader.opt ("fill")->found ();
			
			std::string& str = reader.next ().as_str ();
			if (sutils::iequals (str, "stop"))
				{
					draw_polygon (pl);
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
			
			polygon_data *data = new polygon_data (pl->get_world (), bl, do_fill);
			pl->create_data ("polygon", data,
				[] (void *ptr) { delete static_cast<polygon_data *> (ptr); });
			pl->get_nth_marking_callback (1) += on_blocks_marked;
			
			std::ostringstream ss;
			ss << "§8Polygon §7(§8Block§7: §b" << str << "§7):";
			pl->message (ss.str ());
			
			pl->message ("§8 * §7Please mark the required points§8.");
			pl->message ("§8 * §7Type §c/polygon stop §7to stop§8.");
		}
	}
}

