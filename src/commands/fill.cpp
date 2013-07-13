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
#include "stringutils.hpp"
#include "utils.hpp"
#include <sstream>
#include <mutex>
#include <random>
#include <stack>

#include <iostream> // DEBUG


namespace hCraft {
	namespace commands {
		
	//----
		namespace {
			struct ff_data {
				blocki bl;
				int max;
			};
			
			struct ff_node {
				int x, y, z;
			};
		}
		
		static bool
		ff_on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			ff_data *data = static_cast<ff_data *> (pl->get_data ("flood-fill"));
			if (!data) return true; // shouldn't happen
			
			world *w = pl->get_world ();
			dense_edit_stage es (w);
			chunk_link_map cmap {*w, w->get_chunk_at (marked[0].x, marked[0].z),
				marked[0].x >> 4, marked[0].z >> 4};
			
			block_data target = w->get_block (marked[0].x, marked[0].y, marked[0].z);
			blocki     replace = data->bl;
			
			int count = 0;
			
			std::stack<ff_node> q;
			q.push ({marked[0].x, marked[0].y, marked[0].z});
			while (!q.empty ())
				{
					if (data->max > 0 && count == data->max)
						break;
					
					ff_node n = q.top ();
					q.pop ();
					
					if (cmap.get_id (n.x, n.y, n.z) == target.id &&
							cmap.get_meta (n.x, n.y, n.z) == target.meta)
						{
							cmap.set (n.x, n.y, n.z, replace.id, replace.meta);
							es.set (n.x, n.y, n.z, replace.id, replace.meta);
							++ count;
							
							q.push ({n.x - 1, n.y, n.z});
							q.push ({n.x + 1, n.y, n.z});
							if (n.y > 0) q.push ({n.x, n.y - 1, n.z});
							if (n.y < 255) q.push ({n.x, n.y + 1, n.z});
							q.push ({n.x, n.y, n.z - 1});
							q.push ({n.x, n.y, n.z + 1});
							
						}
				}
			
			es.commit ();
			
			pl->delete_data ("flood-fill");
			
			std::ostringstream ss;
			if (!q.empty ())
				ss << "§cPartial §3flood fill complete";
			else
				ss << "§3Flood fill complete";
			ss << " (§b" << count << " §3blocks)";
			pl->message (ss.str ());
			return true;
		}
		
		static void
		flood_fill (player *pl, command_reader &reader, blocki bd, int max)
		{
			if (reader.arg_count () != 1)
				{
					pl->message ("§c * §7Usage§f: §e/fill \\f §cblock");
					return;
				}
			
			ff_data *data = new ff_data {bd, max};
			pl->create_data ("flood-fill", data,
				[] (void *ptr) { delete static_cast<ff_data *> (ptr); });
			pl->get_nth_marking_callback (1) += ff_on_blocks_marked;
			
			block_info *binf = block_info::from_id (bd.id);
			std::ostringstream ss;
			
			ss << "§8Flood fill §7(§8Block§7: §b";
			if (binf)
				ss << binf->name;
			else
				ss << bd.id;
			ss << "§7):";
			
			pl->message (ss.str ());
			pl->message ("§8 * §7Please mark §b1 §7block§7.");
		}
		
	//----
		
		
		
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
			reader.add_option ("hollow", "o");
			reader.add_option ("flood", "f", 0, 1);
			reader.add_option ("random", "r", 1, 1);
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 2)
				{ this->show_summary (pl); return; }
			
			bool do_physics = !reader.opt ("no-physics")->found ();
			bool do_hollow  = reader.opt ("hollow")->found ();
			
			// randomness
			std::minstd_rand rnd (utils::ns_since_epoch ());
			std::uniform_real_distribution<> dis (0, 100);
			double rprec = 100.0;
			auto rand_opt = reader.opt ("random");
			if (rand_opt->found () && rand_opt->got_args ())
				{
					auto& arg = rand_opt->arg (0);
					rprec = arg.as_float ();
					if (rprec < 0.0) rprec = 0.0;
					else if (rprec > 100.0) rprec = 100.0;
				}
			bool is_rand = (rprec != 100.0);
			
			if (!sutils::is_block (reader.arg (0)))
				{ pl->message ("§c * §7Invalid block§f: §c" + reader.arg (0)); return; }
			
			blocki bd_out, bd_in;
			
			bd_out = sutils::to_block (reader.arg (0));
			if (!bd_out.valid ())
				{
					pl->message ("§c * §7Invalid block§f: §c" + reader.arg (0));
					return;
				}
			
			if (reader.arg_count () == 2)
				{
					bd_in = bd_out;
					bd_out = sutils::to_block (reader.arg (1));
					if (!bd_out.valid ())
						{
							pl->message ("§c * §7Invalid block§f: §c" + reader.arg (1));
							return;
						}
				}
			else
				{ bd_in.id = 0xFFFF; bd_in.meta = 0; }
			
			// flood fill?
			auto f_opt = reader.opt ("flood");
			if (f_opt->found ())
				{
					int max = 50000;
					if (f_opt->got_args ())
						{
							max = f_opt->arg (0).as_int ();
						}
					
					flood_fill (pl, reader, bd_out, max);
					return;
				}
			
			int block_counter = 0;
			int selection_counter = 0;
			
			// fill all selections
			{
				world *wr = pl->get_world ();
				dense_edit_stage es (wr);
				for (auto itr = pl->selections.begin (); itr != pl->selections.end (); ++itr)
					{
						world_selection *sel = itr->second;
						if (!sel->visible ()) continue;
						bool sel_cont = false;
						
						world_selection *sel_inner = nullptr;
						if (do_hollow)
							{
								sel_inner = sel->copy ();
								sel_inner->contract (1, 1, 1);
							}
						
						block_pos min_p = sel->min ();
						block_pos max_p = sel->max ();
						
						if (min_p.y < 0) min_p.y = 0;
						if (min_p.y > 255) min_p.y = 255;
						if (max_p.y < 0) max_p.y = 0;
						if (max_p.y > 255) max_p.y = 255;
						for (int y = max_p.y; y >= min_p.y; --y)
							for (int x = min_p.x; x <= max_p.x; ++x)
								for (int z = min_p.z; z <= max_p.z; ++z)
									{
										if (!wr->in_bounds (x, y, z)) continue;
										if (!sel->contains (x, y, z)) continue;
										if (do_hollow && sel_inner->contains (x, y, z)) continue;
										
										block_data bd = wr->get_block (x, y, z);
										if (bd_in.id != 0xFFFF && (bd.id != bd_in.id || bd.meta != bd_in.meta))
											continue;
										if (bd.id == bd_out.id && bd.meta == bd_out.meta)
											continue;
										
										if (is_rand)
											{
												if (dis (rnd) < rprec)
													{
														es.set (x, y, z, bd_out.id, bd_out.meta);
														++ block_counter;
													}
											}
										else
											{
												es.set (x, y, z, bd_out.id, bd_out.meta);
												++ block_counter;
											}
								
										if (!sel_cont)
											++ selection_counter;
										sel_cont = true;
									}
					
						es.commit (do_physics);
						if (do_hollow)
							delete sel_inner;
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

