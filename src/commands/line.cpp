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

#include "commands/line.hpp"
#include "player/player.hpp"
#include "system/server.hpp"
#include "util/stringutils.hpp"
#include "util/utils.hpp"
#include "drawing/drawops.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		namespace {
			struct line_data {
				player *pl;
				sparse_edit_stage es;
				std::vector<vector3> points;
				blocki bl;
				bool cont;
				int modified;
				
				line_data (player *pl, world *wr, blocki bl, bool cont)
					: es (wr)
				{
					this->bl = bl;
					this->cont = cont;
					this->modified = 0;
					
					pl->es_add (&this->es);
				}
			};
		}
		
		
		static bool
		on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			line_data *data = static_cast<line_data *> (pl->get_data ("line"));
			if (!data) return true; // shouldn't happen
			
			if (!pl->get_world ()->security ().can_build (pl))
				{
					pl->message ("§4 * §cYou are not allowed to build here§4.");
					pl->delete_data ("line");
					return true;
				}
			
			sparse_edit_stage& es = data->es;
			if (es.get_world () != pl->get_world ())
				{
					pl->es_remove (&data->es);
					pl->delete_data ("line");
					pl->message ("§c * §7Worlds changed, drawing cancelled§c.");
					return true;
				}
			
			int blocks;
			if (data->cont)
				{
					std::vector<vector3>& points = data->points;
					points.push_back (marked[0]);
					if (points.size () > 1)
						{
							es.restore_to (pl);
							es.clear ();
						}
					
					if (points.size () == 1)
						{
							es.set (marked[0].x, marked[0].y, marked[0].z, data->bl.id, data->bl.meta);
							data->modified = 1;
						}
					else
						{
							draw_ops draw (es);
							int r;
							for (int i = 0; i < ((int)points.size () - 1); ++i)
								{
									r = draw.line (points[i], points[i + 1], data->bl);
									if (r == -1 || ((data->modified + (r - 1)) > pl->get_rank ().fill_limit ()))
										{
											pl->message (messages::over_fill_limit (pl->get_rank ().fill_limit ()));
											pl->es_remove (&data->es);
											pl->delete_data ("line");
											return true;
										}
								}
							
							data->modified += r - 1;
						}
					
					es.preview_to (pl);
					return false;
				}
			
			draw_ops draw (es);
			blocks = draw.line (marked[0], marked[1], data->bl, pl->get_rank ().fill_limit ());
			if (blocks == -1)
				{
					pl->message (messages::over_fill_limit (pl->get_rank ().fill_limit ()));
					pl->delete_data ("line");
					return true;
				}
			es.commit ();
			
			std::ostringstream ss;
			ss << "§7 | Line complete §f- §b" << blocks << " §7blocks modified";
			pl->message (ss.str ());
			
			pl->es_remove (&data->es);
			pl->delete_data ("line");
			return true;
		}
		
		
		static void
		draw_line (player *pl)
		{
			line_data *data = static_cast<line_data *> (pl->get_data ("line"));
			if (!data)
				{
					pl->message ("§4 * §cYou are not drawing any lines§4.");
					return;
				}
			
			if (data->es.get_world () != pl->get_world ())
				{
					pl->es_remove (&data->es);
					pl->delete_data ("line");
					pl->message ("§c * §7Worlds changed, drawing cancelled§c.");
					return;
				}
			
			if (!data->cont)
				{
					pl->stop_marking ();
					pl->es_remove (&data->es);
					pl->delete_data ("line");
					return;
				}
			
			sparse_edit_stage& es = data->es;
			std::vector<vector3>& points = data->points;
			if (points.empty ())
				{
					pl->stop_marking ();
					pl->es_remove (&data->es);
					pl->delete_data ("line");
					return;
				}
			
			es.restore_to (pl);
			es.clear ();
			
			draw_ops draw (es);
			for (int i = 0; i < ((int)points.size () - 1); ++i)
				draw.line (points[i], points[i + 1], data->bl);
			es.commit ();
			
			pl->stop_marking ();
			pl->es_remove (&data->es);
			
			std::ostringstream ss;
			ss << "§7 | Line complete §f- §b" << data->modified << " §7blocks modified";
			pl->message (ss.str ());
			
			pl->delete_data ("line");
			return;
		}
		
		
		/* 
		 * /line -
		 * 
		 * Draws a line between two selected points.
		 * 
		 * Permissions:
		 *   - command.draw.line
		 *       Needed to execute the command.
		 */
		void
		c_line::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.line"))
					return;
			
			reader.add_option ("cont", "c");
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			bool do_cont = reader.opt ("cont")->found ();
			
			std::string& str = reader.next ().as_str ();
			if (sutils::iequals (str, "stop"))
				{
					draw_line (pl);
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
			
			line_data *data = new line_data (pl, pl->get_world (), bl, do_cont);
			pl->create_data ("line", data,
				[] (void *ptr) { delete static_cast<line_data *> (ptr); });
			pl->get_nth_marking_callback (do_cont ? 1 : 2) += on_blocks_marked;
			
			std::ostringstream ss;
			ss << "§5Draw§f: §3" << (do_cont ? "continuous " : "")
				 << "line §f[§7block§f: §8" << str << "§f]:";
			pl->message (ss.str ());
			if (do_cont)
				{
					pl->message ("§7 | §ePlease mark the required points§f.");
					pl->message ("§7 | §eType §c/line stop §eto stop§f."); 
				}
			else
				pl->message ("§7 | §ePlease mark §btwo §eblocks§f.");
		}
	}
}

