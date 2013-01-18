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
#include "world.hpp"
#include "world_transaction.hpp"
#include <sstream>
#include <mutex>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /fill -
		 * 
		 * Fills all visible selected regions with a block.
		 * 
		 * Permissions:
		 *   - command.draw.fill
		 *       Needed to execute the command.
		 */
		void
		c_fill::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.fill"))
					return;
			
			reader.add_option ("no-physics", "p");
			if (!reader.parse_args (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 2)
				{ this->show_summary (pl); return; }
			
			bool do_physics = !reader.opt ("no-physics")->found ();
			
			if (!reader.is_arg_block (0))
				{ pl->message ("§c * §eInvalid block§f: §c" + reader.arg (0)); return; }
			
			blocki bd_out, bd_in;
			
			bd_out = reader.arg_as_block (0);
			if (!bd_out.valid ())
				{
					pl->message ("§c * §eInvalid block§f: §c" + reader.arg (0));
					return;
				}
			
			if (reader.arg_count () == 2)
				{
					bd_in = bd_out;
					bd_out = reader.arg_as_block (1);
					if (!bd_out.valid ())
						{
							pl->message ("§c * §eInvalid block§f: §c" + reader.arg (1));
							return;
						}
				}
			else
				{ bd_in.id = 0xFFFF; bd_in.meta = 0; }
			
			int block_counter = 0;
			int selection_counter = 0;
			
			// fill all selections
			{
				world *wr = pl->get_world ();
				for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
					{
						world_selection *sel = itr->second;
						if (!sel->visible ()) continue;
						bool sel_cont = false;
						
						block_pos min_p = sel->min ();
						block_pos max_p = sel->max ();
					
						world_transaction *tr = new world_transaction (min_p, max_p, do_physics);
					
						for (int x = min_p.x; x <= max_p.x; ++x)
							for (int y = min_p.y; y <= max_p.y; ++y)
								for (int z = min_p.z; z <= max_p.z; ++z)
									{
										if (sel->contains (x, y, z))
											{
												block_data bd = wr->get_block (x, y, z);
												if (bd_in.id != 0xFFFF && (bd.id != bd_in.id || bd.meta != bd_in.meta))
													continue;
												if (bd.id == bd_out.id && bd.meta == bd_out.meta)
													continue;
												
												//wr->queue_update_nolock (x, y, z, bd_out.id, bd_out.meta);
												tr->set_id_and_meta (x, y, z, bd_out.id, bd_out.meta);
												
												++ block_counter;
												
												if (!sel_cont)
													++ selection_counter;
												sel_cont = true;
											}
									}
					
						pl->get_world ()->queue_update (tr);
					}
			}
			
			{
				std::ostringstream ss;
				ss << "§a" << block_counter << " §eblock" << ((block_counter == 1) ? "" : "s")
					 << " have been replaced (§c" << selection_counter << " §eselection"
					 << ((selection_counter == 1) ? "" : "s") << ")";
				pl->message (ss.str ());
			}
		}
	}
}

