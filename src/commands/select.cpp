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
#include "cistring.hpp"
#include <vector>
#include <sstream>
#include <utility>

#include "selection/cuboid_selection.hpp"
#include "selection/block_selection.hpp"
#include "selection/sphere_selection.hpp"


namespace hCraft {
	namespace commands {
		
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
					pl->message ("§c * §7Usage§f: §e/sel new §8[§b@§cname§8] §ctype");
					return;
				}
			
			block_pos curr_pos = pl->pos;
			std::string& narg = reader.next ().as_str ();
			std::string sel_type;
			std::string name;
			
			if (narg[0] == '@')
				{
					name = narg.substr (1);
					if (name.empty ())
						{
							pl->message ("§c * §7Invalid selection name§f.");
							return;
						}
					
					if (!reader.has_next ())
						{
							pl->message ("§c * §7Usage§f: §e/sel new §8[§b@§cname§8] §ctype");
							return;
						}
					
					sel_type = reader.next ().as_str ();
					
					// make sure no name collision will arrise from this
					auto itr = pl->selections.find (name.c_str ());
					if (itr != pl->selections.end ())
						{
							pl->message ("§c * §7Name already taken§f: §c" + name);
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
			else if (sutils::iequals (sel_type, "sphere") || sutils::iequals (sel_type, "s"))
				{
					world_selection *sel = new sphere_selection (curr_pos, curr_pos);
					pl->selections[name.c_str ()] = sel;
					pl->curr_sel = sel;
					pl->message ("§eCreated new selection §b@§9" + name + " §eof type§f: §9sphere");
				}
			else
				{
					pl->message ("§c * §7Invalid selection type§f: §c" + sel_type);
					return;
				}
		}
		
		static void
		handle_delete (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f: §e/sel delete §c§b@§cname");
					return;
				}
			
			std::string name = reader.next ().as_str ();
			if (name[0] != '@' || (name = name.substr (1)).empty ())
				{
					pl->message ("§c * §7Invalid selection name§f.");
					return;
				}
			
			auto itr = pl->selections.find (name.c_str ());
			if (itr == pl->selections.end ())
				{
					pl->message ("§c * §7No such selection§f: §c" + name);
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
			pl->sb_commit ();
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
							pl->message ("§c * §7Invalid parameter for §aclear§f: §c" + sarg);
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
			pl->sb_commit ();
			
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
					pl->message ("§c * §7Usage§f: §e/sel move §4[§cx§4/§cy§4/§cz units§4]§c...");
					return;
				}
			
			enum axis { A_X = 1, A_Y = 2, A_Z = 4 };
			struct ax_mod { int ax; int units; };
			std::vector<ax_mod> changes;
			
			while (reader.has_next ())
				{
					std::string ax_str    = reader.next ();
					if (!reader.has_next ())
						{
							pl->message ("§c * §7Usage§f: §e/sel move §4[§cx§4/§cy§4/§cz units§4]§c...");
							return;
						}
					std::string units_str = reader.next ();
					if (!sutils::is_int (units_str))
						{
							pl->message ("§c * §7Invalid number§f: §c" + units_str);
							return;
						}
					
					int units = sutils::to_int (units_str);
					if ((units > 100000) || (units < -100000))
						{
							pl->message ("§c * §7The §cunits §evalue is too high/low");
							return;
						}
					
					int ax = 0;
					for (char c : ax_str)
						{
							switch (c)
								{
									case 'x': ax |= A_X; break;
									case 'y': ax |= A_Y; break;
									case 'z': ax |= A_Z; break;
									default:
										pl->message ("§c * §7Invalid direction§f, §emust be §cxyz");
										return;
								}
						}
					
					changes.push_back (ax_mod {ax, units});
				}
			
			std::unordered_set<world_selection *> sels;
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
				{
					world_selection *sel = itr->second;
					if (!sel->visible ())
						continue;
					
					sels.insert (sel);
					sel->hide (pl);
				}
			
			for (ax_mod m : changes)
				{
					int xx = (m.ax & A_X) ? m.units : 0;
					int yy = (m.ax & A_Y) ? m.units : 0;
					int zz = (m.ax & A_Z) ? m.units : 0;
					
					for (world_selection *sel : sels)
						{
							if (xx > 0)
								sel->move (DI_EAST, xx);
							else if (xx < 0)
								sel->move (DI_WEST, -xx);
							if (yy > 0)
								sel->move (DI_UP, yy);
							else if (yy < 0)
								sel->move (DI_DOWN, -yy);
							if (zz > 0)
								sel->move (DI_SOUTH, zz);
							else if (zz < 0)
								sel->move (DI_NORTH, -zz);
						}
				}
			
			for (world_selection *sel : sels)
				sel->show (pl);
			pl->sb_commit ();
			
			std::ostringstream ss;
			ss << "§eAll §9visible §eselections have been §bmoved §e(§c"
				 << sels.size () << " §eselections)";
			pl->message (ss.str ());
		}
		
		static void
		handle_mark (player *pl, command_reader& reader, const std::string& arg)
		{
			int num;
			if (!sutils::is_int (arg) || ((num = sutils::to_int (arg)) <= 0))
				{ pl->message ("§c * §7Invalid position number§f."); return; }
	
			if (pl->selections.empty ())
				{ pl->message ("§c * §7You§f'§7re not selecting anything§f."); return; }
			world_selection *selection = pl->curr_sel;
			if (num > selection->needed_points ())
				{
					std::ostringstream ss;
					ss << "§c * §7The selection that you§f'§7re making requires only §f"
						 << selection->needed_points ()<< " §7points§f.";
					pl->message (ss.str ());
					return;
				}
	
			block_pos curr_pos = pl->pos;
			selection->set_update (num - 1, curr_pos, pl);
			pl->sb_commit ();
	
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
			std::vector<blocki> blocks;
			enum {
				R_INCLUDE,
				R_EXCLUDE,
			} state = R_INCLUDE;
			
			std::string& n = reader.peek_next ();
			if (sutils::iequals (n, "but") || sutils::iequals (n, "except"))
				{
					state = R_EXCLUDE;
					reader.next ();
				}
			
			while (reader.has_next ())
				{
					command_reader::argument arg = reader.next ();
					if (!arg.is_block ())
						{
							std::ostringstream ss;
							ss << "§c * §7Invalid block§f: §c" << n;
							pl->message (ss.str ());
							return;
						}
					
					blocks.push_back (arg.as_block ());
				}
			
			if (blocks.empty ())
				{
					pl->message ("§c * §7Please specify which blocks to select§f.");
					pl->message ("§c   > §eUsage§f: §e/sel all §7but§8/§7except §cblocks...");
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
			
			if (ey < 0) ey = 0;
			if (ey > 255) ey = 255;
			if (sy < 0) sy = 0;
			if (sy > 255) sy = 255;
			
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
							if (smin.y < 0) smin.y = 0;
							if (smin.y > 255) smin.y = 255;
							if (smax.y < 0) smax.y = 0;
							if (smax.y > 255) smax.y = 255;
							for (y = smin.y; y <= smax.y; ++y)
								{
									for (x = smin.x; x <= smax.x; ++x)
										for (z = smin.z; z <= smax.z; ++z)
											{
												if (wr->is_out_of_bounds (x, y, z)) continue;
												if (sel->contains (x, y, z))
													{
														if (state == R_INCLUDE)
															{
																block_data bd = wr->get_block (x, y, z);
																bool found = false;
																for (blocki ibd : blocks)
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
																for (blocki ibd : blocks)
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
				}
			
			if (counter == 0)
				{
					pl->message ("§bNothing §ehas been selected");
					return;
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
			pl->sb_commit ();
		}
		
		
		
		static void
		handle_expand_contract (player *pl, command_reader& reader, bool do_expand)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f: §e/sel §cexpand§4/§ccontract §4[§cx§4/§cy§4/§cz units§4]§c...");
					return;
				}
			
			enum axis { A_X = 1, A_Y = 2, A_Z = 4 };
			struct ax_mod { int ax; int units; };
			std::vector<ax_mod> changes;
			
			while (reader.has_next ())
				{
					std::string ax_str    = reader.next ();
					if (!reader.has_next ())
						{
							pl->message ("§c * §7Usage§f: §e/sel §cexpand§4/§ccontract §4[§cx§4/§cy§4/§cz units§4]§c...");
							return;
						}
					std::string units_str = reader.next ();
					if (!sutils::is_int (units_str))
						{
							pl->message ("§c * §7Invalid number§f: §c" + units_str);
							return;
						}
					
					int units = sutils::to_int (units_str);
					if ((units > 100000) || (units < -100000))
						{
							pl->message ("§c * §7The §cunits §7value is too high/low");
							return;
						}
					
					int ax = 0;
					for (char c : ax_str)
						{
							switch (c)
								{
									case 'x': ax |= A_X; break;
									case 'y': ax |= A_Y; break;
									case 'z': ax |= A_Z; break;
									default:
										pl->message ("§c * §7Invalid direction§f, §7must be §cxyz");
										return;
								}
						}
					
					changes.push_back (ax_mod {ax, units});
				}
			
			std::unordered_set<world_selection *> sels;
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
				{
					world_selection *sel = itr->second;
					if (!sel->visible ())
						continue;
					
					sels.insert (sel);
					sel->hide (pl);
				}
			
			for (ax_mod m : changes)
				{
					int xx = (m.ax & A_X) ? m.units : 0;
					int yy = (m.ax & A_Y) ? m.units : 0;
					int zz = (m.ax & A_Z) ? m.units : 0;
					
					for (world_selection *sel : sels)
						{
							if (do_expand)
								sel->expand (xx, yy, zz);
							else
								sel->contract (xx, yy, zz);
						}
				}
			
			for (world_selection *sel : sels)
				sel->show (pl);
			pl->sb_commit ();
			
			std::ostringstream ss;
			ss << "§eAll §9visible §eselections have been §b"
				 << (do_expand ? "expanded" : "contracted") << " §e(§c"
				 << sels.size () << " §eselections)";
			pl->message (ss.str ());
		}
		
		
		
		static void
		handle_show_hide (player *pl, command_reader& reader, bool do_show)
		{
			std::unordered_set<world_selection *> sel_list;
			bool list_except = false, list_all = false;
			
			if (reader.has_next ())
				{
					std::string str = reader.peek_next ();
					if (str == "but" || str == "except")
						{
							list_except = true;
							reader.next ();
						}
					else if (str == "all")
						{
							list_all = true;
							list_except = true;
							reader.next ();
						}
				}
			
			if (!list_all)
				{
					if (!reader.has_next ())
						{
							pl->message ("§c * §7Usage§f: §e/sel show/hide §call");
							pl->message ("§c           §e/sel show/hide §7but§8/§7except §cselections...");
							return;
						}
					
					while (reader.has_next ())
						{
							std::string str = reader.next ();
							if (str[0] != '@')
								{
									pl->message ("§c * §7Invalid selection name§f: §c" + str);
									return;
								}
							str.erase (str.begin ());
							auto itr = pl->selections.find (str.c_str ());
							if (itr == pl->selections.end ())
								{
									pl->message ("§c * §7No such selection§f: §c" + str);
									return;
								}
							
							sel_list.insert (itr->second);
						}
				}
			
			// collect selections
			std::unordered_set<world_selection *> final_set;
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
				{
					world_selection *sel = itr->second;
					auto i2 = sel_list.find (sel);
					if (list_except) {
						if (i2 == sel_list.end ())
							final_set.insert (sel);
					} else {
						if (i2 != sel_list.end ())
							final_set.insert (sel);
					}
				}
			
			int count = 0;
			for (auto itr = final_set.begin (); itr != final_set.end (); ++ itr)
				{
					world_selection *sel = *itr;
					if (sel->visible () != do_show)
						{
							++ count;
							if (do_show)
								sel->show (pl);
							else
								sel->hide (pl);
						}
				}
			
			pl->sb_commit ();
			
			// message
			std::ostringstream ss;
			ss << "§c" << count << " §eselection"
				 << (count == 1 ? " " : "s ")
		   	 << (count == 1 ? "has" : "have") << " been made "
				 << (do_show ? "§9visible" : "§8hidden");
			pl->message (ss.str ());
		}
		
		
		static void
		handle_sb (player *pl, command_reader& reader)
		{
			if (!reader.has_next () || !sutils::is_block (reader.arg (1)))
				{
					pl->message ("§c * §7Usage§f: §e/sel sb §cblock");
					return;
				}
			
			blocki blk = sutils::to_block (reader.arg (1));
			if (!blk.valid ())
				{
					pl->message ("§c * §7Invalid block§f: §c" + reader.arg (1));
					return;
				}
			
			pl->sb_block = blk;
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
				{
					world_selection *sel = itr->second;
					if (sel->visible ())
						{
							sel->hide (pl);
							sel->show (pl);
						}
				}
			pl->sb_commit ();
			
			block_info *binf = block_info::from_id (blk.id);
			std::ostringstream ss;
			ss << "§eSelection block type changed to §a" << binf->name << "§f:§a" << (int)blk.meta;
			pl->message (ss.str ());
		}
		
		
		static void
		handle_volume (player *pl, command_reader& reader)
		{
			int total_vol = 0, sel_count = 0;
			bool check_all = false;
			
			if (reader.has_next ())
				{
					command_reader::argument narg = reader.next ();
					if (sutils::iequals (narg.as_str (), "all"))
						check_all = true;
					else
						{
							pl->message ("§c * §7Invalid option§f: §c" + narg.as_str ());
							return;
						}
				}
			
			for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
				{
					world_selection *sel = itr->second;
					if (check_all || sel->visible ())
						{
							total_vol += sel->volume ();
							++ sel_count;
						}
				}
			
			std::ostringstream ss;
			ss << "§eApproximate volume (§c" << sel_count << " §eselections)§f: §a" << total_vol << " §eblocks";
			pl->message (ss.str ());
		}
		
		
		
		/* 
		 * /select -
		 * 
		 * A multipurpose command for changing the current selection(s) managed
		 * by the calling player.
		 * 
		 * Permissions:
		 *   - command.draw.select
		 *       Needed to execute the command.
		 */
		void
		c_select::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.select"))
					return;
		
			if (!reader.parse (this, pl))
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
			else if (sutils::iequals (str, "expand"))
				{
					handle_expand_contract (pl, reader, true);
				}
			else if (sutils::iequals (str, "contract"))
				{
					handle_expand_contract (pl, reader, false);
				}
			else if (sutils::iequals (str, "show"))
				{
					handle_show_hide (pl, reader, true);
				}
			else if (sutils::iequals (str, "hide"))
				{
					handle_show_hide (pl, reader, false);
				}
			else if (sutils::iequals (str, "sb"))
				{
					handle_sb (pl, reader);
				}
			else if (sutils::iequals (str, "vol") || sutils::iequals (str, "volume"))
				{
					handle_volume (pl, reader);
				}
			else
				{
					handle_mark (pl, reader, str);
				}
		}
	}
}

