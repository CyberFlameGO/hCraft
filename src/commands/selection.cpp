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
#include <utility>

#include "selection/cuboid_selection.hpp"
#include "selection/block_selection.hpp"


namespace hCraft {
	namespace commands {
		
		direction
		get_direction (const std::string& dir_str)
		{
			if (sutils::iequals (dir_str, "x+") || sutils::iequals (dir_str, "west"))
				return DI_EAST;
			else if (sutils::iequals (dir_str, "x-") || sutils::iequals (dir_str, "east"))
				return DI_WEST;
			else if (sutils::iequals (dir_str, "z+") || sutils::iequals (dir_str, "north"))
				return DI_SOUTH;
			else if (sutils::iequals (dir_str, "z-") || sutils::iequals (dir_str, "south"))
				return DI_NORTH;
			else if (sutils::iequals (dir_str, "y+") || sutils::iequals (dir_str, "up"))
				return DI_UP;
			else if (sutils::iequals (dir_str, "y-") || sutils::iequals (dir_str, "down"))
				return DI_DOWN;
			
			return DI_UNKNOWN;
		}
		
		static int
		get_next_selection_number (player *pl)
		{
			int num = 1;
			std::ostringstream ss;
			
			for (;;)
				{
					ss << num;
					auto itr = pl->selections.find (ss.str ().c_str ());
					if (itr == pl->selections.end ())
						return num;
					
					ss.clear (); ss.str (std::string ());
					++ num;
				}
		}
		
		
		static void
		handle_new (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §eUsage§f: §e/sel new §c[§b@§cname] <type>");
					return;
				}
			
			block_pos curr_pos = pl->get_pos ();
			std::string narg = reader.next ();
			std::string sel_type;
			std::string name;
			
			if (narg[0] == '@')
				{
					name = narg.substr (1);
					if (name.empty ())
						{
							pl->message ("§c * §eInvalid selection name§f.");
							return;
						}
					
					if (!reader.has_next ())
						{
							pl->message ("§c * §eUsage§f: §e/sel new §c[§b@§cname] <type>");
							return;
						}
					
					sel_type = reader.next ();
					
					// make sure no name collision will arrise from this
					auto itr = pl->selections.find (name.c_str ());
					if (itr != pl->selections.end ())
						{
							pl->message ("§c * §eName already taken§f: §c" + name);
							return;
						}
				}
			else
				{
					std::ostringstream ss;
					ss << get_next_selection_number (pl);
					name = ss.str ();
					
					sel_type = narg;
				}
			
			if (sutils::iequals (sel_type, "cuboid") || sutils::iequals (sel_type, "c"))
				{
					world_selection *sel = new cuboid_selection (curr_pos, curr_pos);
					pl->selections[name.c_str ()] = sel;
					pl->curr_sel = sel;
					pl->message ("§eCreated new selection §b@§9" + name + " §eof type§f: §9cuboid");
				}
			else
				{
					pl->message ("§c * §eInvalid selection type§f: §c" + sel_type);
					return;
				}
		}
		
		static void
		handle_delete (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §eUsage§f: §e/sel delete §c<§b@§cname>");
					return;
				}
			
			std::string name = reader.next ();
			if (name[0] != '@' || (name = name.substr (1)).empty ())
				{
					pl->message ("§c * §eInvalid selection name§f.");
					return;
				}
			
			auto itr = pl->selections.find (name.c_str ());
			if (itr == pl->selections.end ())
				{
					pl->message ("§c * §eNo such selection§f: §c" + name);
					return;
				}
			
			bool adjust_curr_sel = false;
			if (itr->second == pl->curr_sel)
				adjust_curr_sel = true;
			if (itr->second->visible ())
				itr->second->hide (pl);
			delete itr->second;
			pl->message ("§eSelection §b@§9" + std::string (itr->first.c_str ()) + " §ehas been deleted§f.");
			pl->selections.erase (itr);
			
			if (adjust_curr_sel)
				pl->curr_sel = pl->selections.empty () ? nullptr
					: pl->selections.begin ()->second;
		}
		
		static void
		handle_clear (player *pl, command_reader& reader)
		{
			enum { VISIBLE, HIDDEN, ALL } op = VISIBLE;
			if (reader.has_next ())
				{
					std::string sarg = reader.next ();
					if (sutils::iequals (sarg, "all"))
						op = ALL;
					else if (sutils::iequals (sarg, "visible"))
						op = VISIBLE;
					else if (sutils::iequals (sarg, "hidden"))
						op = HIDDEN;
					else
						{
							pl->message ("§c * §eInvalid parameter for §aclear§f: §c" + sarg);
							return;
						}
				}
			
			// clear all selections
			bool curr_deleted = false;
			for (auto itr = pl->selections.begin (); itr != pl->selections.end ();)
				{
					world_selection *sel = itr->second;
					if ((op == VISIBLE && !sel->visible ()) || 
							(op == HIDDEN && sel->visible ()))
						{ ++itr; continue; }
					if (sel->visible ())
						sel->hide (pl);
					if (pl->curr_sel == sel)
						curr_deleted = true;
					delete sel;
					
					itr = pl->selections.erase (itr);
				}
			
			if (curr_deleted)
				{
					if (pl->selections.empty ())
						pl->curr_sel = nullptr;
					else
						pl->curr_sel = pl->selections.begin ()->second;
				}
			
			switch (op)
				{
					case VISIBLE: pl->message ("§eAll §9visible §eselections have been cleared"); break;
					case HIDDEN: pl->message ("§eAll §8hidden §eselections have been cleared"); break;
					case ALL: pl->message ("§eAll selections have been cleared"); break;
				}
		}
		
		static void
		handle_move (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §eUsage§f: §e/sel move §c<<x-/x+/z-/z+/y-/z+> <units>>...");
					return;
				}
			
			std::vector<std::pair<direction, int>> moves;
			
			while (reader.has_next ())
				{
					std::string dir_str = reader.next ();
					direction   dir     = get_direction (dir_str);
					if (dir == DI_UNKNOWN)
						{
							pl->message ("§c * §eInvalid direction§f: §c" + dir_str);
							return;
						}
					
					int units = 1;
					if (reader.has_next ())
						{
							std::string unit_str = reader.next ();
							if (get_direction (unit_str) == DI_UNKNOWN)
								{
									std::istringstream ss (unit_str);
									ss >> units;
									if (ss.fail ())
										{
											pl->message ("§c * §eInvalid number§f: §c" + unit_str);
											return;
										}
								}
						}
					
					moves.push_back (std::make_pair (dir, units));
				}
			
			std::vector<world_selection *> to_move;
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
				{
					world_selection *sel = itr->second;
					if (sel->visible ())
						{
							sel->hide (pl);
							to_move.push_back (sel);
						}
				}
			for (auto& p : moves)
				for (world_selection *sel : to_move)
					{
						sel->move (p.first, p.second);
					}
			for (world_selection *sel : to_move)
				{
					sel->show (pl);
				}
			
			{
				std::ostringstream ss;
				ss << "§a" << to_move.size () << " §eselections have been moved§f.";
				pl->message (ss.str ());
			}
		}
		
		static void
		handle_mark (player *pl, command_reader& reader, const std::string& arg)
		{
			int num;
			if (!sutils::is_int (arg) || ((num = sutils::to_int (arg)) <= 0))
				{ pl->message ("§c * §eInvalid position number§f."); return; }
	
			if (pl->selections.empty ())
				{ pl->message ("§c * §eYou§f'§ere not selecting anything§f."); return; }
			world_selection *selection = pl->curr_sel;
			if (num > selection->needed_points ())
				{
					std::ostringstream ss;
					ss << "§c * §eThe selection that you§f'§ere making requires only §f"
						 << selection->needed_points ()<< " §epoints§f.";
					pl->message (ss.str ());
					return;
				}
	
			block_pos curr_pos = pl->get_pos ();
			selection->set_update (num - 1, curr_pos, pl);
	
			{
				std::ostringstream ss;
				ss << "§eSetting point §b#" << num << " §eto {§9x: §a"
					 << curr_pos.x << "§e, §9y: §a" << curr_pos.y << "§e, §9z: §a"
					 << curr_pos.z << "§e}";
				pl->message (ss.str ());
			}
		}
		
		
		static void
		handle_all (player *pl, command_reader& reader)
		{
			std::vector<block_data> blocks;
			enum {
				R_INCLUDE,
				R_EXCLUDE,
			} state = R_INCLUDE;
			
			std::string n = reader.peek_next ();
			if (sutils::iequals (n, "but") || sutils::iequals (n, "except"))
				{
					state = R_EXCLUDE;
					reader.next ();
				}
			
			while (reader.has_next ())
				{
					n = reader.next ();
					if (!reader.is_arg_block (reader.offset () - 1))
						{
							std::ostringstream ss;
							ss << "§c * §eInvalid block§f: §c" << n;
							pl->message (ss.str ());
							return;
						}
					
					blocks.push_back (reader.arg_as_block (reader.offset () - 1));
				}
			
			if (blocks.empty ())
				{
					pl->message ("§c * §ePlease specify which blocks to select§f.");
					pl->message ("§c   > §eUsage§f: §e/sel all §b[but/except] §c<blocks>...");
					return;
				}
			
			int sx, sy, sz, ex, ey, ez;
			sx = sy = sz =  2147483647;
			ex = ey = ez = -2147483648;
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
				{
					world_selection *sel = itr->second;
					if (sel->visible ())
						{
							block_pos pmin = sel->min (), pmax = sel->max ();
							if (pmin.x < sx)
								sx = pmin.x;
							if (pmin.y < sy)
								sy = pmin.y;
							if (pmin.z < sz)
								sz = pmin.z;
							if (pmax.x > ex)
								ex = pmax.x;
							if (pmax.y > ey)
								ey = pmax.y;
							if (pmax.z > ez)
								ez = pmax.z;
						}
				}
			
			{
				std::ostringstream ss;
				ss << "§eSelection boundaries§f: §b{" << sx << ", " << sy << ", " << sz
					 << "} §eto §b{" << ex << ", " << ey << ", " << ez << "}§f.";
				pl->message (ss.str ());
			}
			
			int counter = 0;
			world *wr = pl->get_world ();
			block_selection *bsel = new block_selection (
				block_pos (sx, sy, sz), block_pos (ex, ey, ez));
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
				{
					world_selection *sel = itr->second;
					if (sel->visible ())
						{
							int x, y, z;
							block_pos smin = sel->min (), smax = sel->max ();
							for (x = smin.x; x <= smax.x; ++x)
								for (y = smin.y; y <= smax.y; ++y)
									for (z = smin.z; z <= smax.z; ++z)
										{
											if (sel->contains (x, y, z))
												{
													if (state == R_INCLUDE)
														{
															block_data bd = wr->get_block (x, y, z);
															bool found = false;
															for (block_data ibd : blocks)
																if (ibd.id == bd.id && ibd.meta == bd.meta)
																	{ found = true; break; }
															if (found)
																{
																	bsel->set_block (x, y, z, true);
																	++ counter;
																}
														}
													else
														{
															block_data bd = wr->get_block (x, y, z);
															bool found = false;
															for (block_data ibd : blocks)
																if (ibd.id == bd.id && ibd.meta == bd.meta)
																	{ found = true; break; }
															if (!found)
																{
																	bsel->set_block (x, y, z, true);
																	++ counter;
																}
														}
												}
										}
						}
				}
			
			// clear all visible selections
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); )
				{
					world_selection *sel = itr->second;
					if (sel->visible ())
						{
							sel->hide (pl);
							itr = pl->selections.erase (itr);
							delete sel;
						}
					else
						++ itr;
				}
			
			std::ostringstream ss;
			ss << "@" << get_next_selection_number (pl);
			std::string bsel_name = ss.str ();
			pl->selections[bsel_name.c_str ()] = bsel;
			pl->curr_sel = bsel;
			
			ss.str (std::string ()); ss.clear ();
			ss << "§eCreated new selection §9" << bsel_name << "§f: §a" << counter << " §eblocks";
			pl->message (ss.str ());
			
			bsel->show (pl);
		}
		
		
		
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
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string str = reader.next ();
			if (sutils::iequals (str, "new"))
				{
					handle_new (pl, reader);
				}
			else if (sutils::iequals (str, "delete"))
				{
					handle_delete (pl, reader);
				}
			else if (sutils::iequals (str, "clear"))
				{
					handle_clear (pl, reader);
				}
			else if (sutils::iequals (str, "all"))
				{
					handle_all (pl, reader);
				}
			else if (sutils::iequals (str, "move"))
				{
					handle_move (pl, reader);
				}
			else
				{
					handle_mark (pl, reader, str);
				}
		}
	}
}

