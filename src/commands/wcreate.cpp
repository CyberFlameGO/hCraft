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

#include "commands/wcreate.hpp"
#include "system/server.hpp"
#include "player/player.hpp"
#include "world/world.hpp"
#include "util/stringutils.hpp"
#include "world/providers/worldprovider.hpp"
#include "world/generation/worldgenerator.hpp"
#include <chrono>
#include <functional>
#include <thread>


namespace hCraft {
	namespace commands {
		
		
		static void
		_pregen_worker (player *pl, world *w, bool load)
		{
			pl->message ("§d | §5World generation started");
			
			int x_chunks = w->get_width () / 16;
			int z_chunks = w->get_depth () / 16;
			int done = 0;
			int left = x_chunks * z_chunks;
			
			int upmod = x_chunks * z_chunks / 64 / 10;
			if (upmod == 0) upmod = 1;
			int batches_done = 0;
			
			// populate the world in batches of 8x8 (64) chunks
			for (int xb = 0; xb < x_chunks; xb += 8)
				for (int zb = 0; zb < z_chunks; zb += 8)
					{
						for (int cx = xb; cx < (xb + 8) && cx < x_chunks; ++cx)
							for (int cz = zb; cz < (zb + 8) && cz < z_chunks; ++cz)
								{
									w->load_chunk (cx, cz);
									++ done;
									-- left;
								}
						
						w->clear_chunks (true);
						if (batches_done++ % upmod == 0)
							{
								std::ostringstream ss;
								ss << "§d |   §a%" << (batches_done / (x_chunks * z_chunks / 64.0) * 100.0) << " §5- §a" << done << "§5/§a" << (x_chunks * z_chunks) << " §5chunks done";
								pl->message (ss.str ());
							}
					}
			
			w->prepare_spawn (0, true);
			w->save_all ();
			pl->message ("§d | §5Done");
			
			if (load)
				{
					if (!pl->get_server ().get_worlds ().add (w))
						{
							delete w;
							pl->message ("§cFailed to load world§7.");
						}
					
					w->start ();
					pl->get_server ().get_players ().message (
						std::string ("§3World §b") + w->get_colored_name () + " §3has been loaded§b!");
				}
			else
				{
					delete w;
				}
		}
		
		static void
		_pregen_world (player *pl, world *w, bool load)
		{
			// do this in a separate thread
			std::thread (_pregen_worker, pl, w, load).detach ();
		}
		
		/* 
		 * /wcreate -
		 * 
		 * Creates a new world, and if requested, loads it into the current world
		 * list.
		 * 
		 * Permissions:
		 *   - command.world.wcreate
		 *       Needed to execute the command.
		 */
		void
		c_wcreate::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.wcreate"))
				return;
			
			reader.add_option ("load", "l");
			reader.add_option ("type", "t", true, true);
			reader.add_option ("width", "w", true, true);
			reader.add_option ("depth", "d", true, true);
			reader.add_option ("provider", "p", true, true);
			reader.add_option ("generator", "g", true, true);
			reader.add_option ("seed", "s", true, true);
			reader.add_option ("pre-generate", "a");
			if (!reader.parse (this, pl))
				return;
			
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
		//----
			/* 
			 * Parse arguments:
			 */
			
			// world name
			std::string& world_name = reader.arg (0);
			if (!world::is_valid_name (world_name.c_str ()))
				{
					pl->message ("§c * §7World names must be under §a32 §7characters long and "
											 "may only contain alpha§f-§7numeric characters§f, §7dots§f, "
											 "§7hyphens and underscores§f.");
					return;
				}
			
			bool do_pregen = reader.opt ("pre-generate")->found ();
			
			// world type
			world_type wtyp = WT_NORMAL;
			auto opt_type = reader.opt ("type");
			if (opt_type->found ())
				{
					auto& arg = opt_type->arg (0);
					std::string styp = arg.as_str ();
					if (sutils::iequals (styp, "normal"))
						wtyp = WT_NORMAL;
					else if (sutils::iequals (styp, "light"))
						wtyp = WT_LIGHT;
					else
						{
							pl->message ("§c * §7Invalid world type§f: §c" + styp);
							return;
						}
				}
			
			// world width
			int world_width = 0;
			auto opt_width = reader.opt ("width");
			if (opt_width->found ())
				{
					auto& arg = opt_width->arg (0);
					if (!arg.is_int ())
						{
							pl->message ("§c * §7Argument to flag §c--width §7must be an integer§f.");
							return;
						}
					world_width = arg.as_int ();
					if (world_width < 0)
						world_width = 0;
				}
			
			// world depth
			int world_depth = 0;
			auto opt_depth = reader.opt ("depth");
			if (opt_depth->found ())
				{
					auto& arg = opt_depth->arg (0);
					if (!arg.is_int ())
						{
							pl->message ("§c * §7Argument to flag §c--depth §7must be an integer§f.");
							return;
						}
					world_depth = arg.as_int ();
					if (world_depth < 0)
						world_depth = 0;
				}
			
			// world provider
			std::string provider_name ("hw");
			auto opt_prov = reader.opt ("provider");
			if (opt_prov->found ())
				{
					auto& arg = opt_prov->arg (0);
					provider_name.assign (arg.as_str ());
				}
			
			// world generator
			std::string gen_name ("flatgrass");
			auto opt_gen = reader.opt ("generator");
			if (opt_gen->found ())
				{
					auto& arg = opt_gen->arg (0);
					gen_name.assign (arg.as_str ());
				}
			
			// generator seed
			int gen_seed = std::chrono::duration_cast<std::chrono::milliseconds> (
				std::chrono::high_resolution_clock::now ().time_since_epoch ()).count ()
				& 0x7FFFFFFF;
			auto opt_seed = reader.opt ("seed");
			if (opt_seed->found ())
				{
					auto& arg = opt_seed->arg (0);
					if (arg.is_int ())
						gen_seed = arg.as_int ();
					else
						gen_seed = std::hash<std::string> () (arg.as_str ()) & 0x7FFFFFFF;
				}
			
			// load world
			bool load_world = reader.opt ("load")->found ();
			
		//----
			
			if (do_pregen && ((world_width <= 0) || (world_depth <= 0)))
				{
					pl->message ("§c * §7Infinite worlds cannot be pre-generated§f.");
					return;
				}
			
			if (load_world && (pl->get_server ().get_worlds ().find (world_name.c_str ()) != nullptr))
				{
					pl->message ("§c * §7A world with the same name is already loaded§f.");
					return;
				}
			
			world_generator *gen = world_generator::create (gen_name.c_str (), gen_seed);
			if (!gen)
				{
					pl->message ("§c * §7Invalid world generator§f: §c" + gen_name);
					return;
				}
			
			world_provider *prov = world_provider::create (provider_name.c_str (),
				"data/worlds", world_name.c_str ());
			if (!prov)
				{
					pl->message ("§c * §7Invalid world provider§f: §c" + provider_name);
					delete gen;
					return;
				}
			
			{
				std::ostringstream ss;
				
				pl->message ("§3Creating a new world with the name of §e" + world_name + "§f:");
				ss << "§7 | §eWorld type§f: §9" << ((wtyp == WT_NORMAL) ? "normal" : "light");
				pl->message (ss.str ());
				
				ss.str (std::string ());
				ss << "§7 | §eWorld dimensions§f: §c";
				if (world_width == 0)
					ss << "§binf";
				else
					ss << "§a" << world_width;
				ss << " §ex §a256 §ex ";
				if (world_depth == 0)
					ss << "§binf";
				else
					ss << "§a" << world_depth;
				pl->message (ss.str ());
				
				ss.clear (); ss.str (std::string ());
				if ((world_width == 0) || (world_depth == 0))
					ss << "§7 | §eEstimated size §f(§ewhen full§f): §cinfinite";
				else
					{
						double est_kb = ((world_width * world_depth) / 256) * 7.2375 + 49.7;
						ss.clear (); ss.str (std::string ());
						ss << "§7 | §eEstimated size §f(§ewhen full§f): §c~";
						if (est_kb >= 1024.0)
							ss << (est_kb / 1024.0) << "MB";
						else
							ss << est_kb << "KB";
					}
				pl->message (ss.str ());
				
				ss.clear (); ss.str (std::string ());
				ss << "§7 | §eGenerator§f: §b" << gen_name << "§f, §eProvider§f: §b" << provider_name;
				pl->message (ss.str ());
				
				ss.clear (); ss.str (std::string ());
				ss << "§7 | §eWorld seed§f: §a" << gen_seed;
				pl->message (ss.str ());
			}
			
			world *wr = new world (wtyp, pl->get_server (), world_name.c_str (),
				pl->get_logger (), gen, prov);
			wr->set_width (world_width);
			wr->set_depth (world_depth);
			
			if (do_pregen)
				{
					_pregen_world (pl, wr, load_world);
					return;
				}
			
			wr->prepare_spawn (10, true);
			wr->save_all ();
			pl->message ("§7 | Done.");
			
			if (load_world)
				{
					if (!pl->get_server ().register_world (wr))
						{
							delete wr;
							pl->message ("§cFailed to load world§7.");
						}
					
					wr->start ();
					pl->get_server ().get_players ().message (
						"§9> §3World " + std::string (wr->get_colored_name ()) + " §3has been loaded§9!");
				}
			else
				{
					delete wr;
				}
		}
	}
}

