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

#include "commands/worldc.hpp"
#include "player.hpp"
#include "world.hpp"
#include "stringutils.hpp"
#include "server.hpp"
#include <mutex>
#include <sstream>


namespace hCraft {
	namespace commands {
		
		static void
		_set_property (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f: §e/wconfig set §cproperty ...");
					return;
				}
			
			world *w = pl->get_world ();
			std::string& prop = reader.next ();
			
			if (sutils::iequals (prop, "build-perms"))
				{
					std::string str = reader.rest ();
					w->set_build_perms (str);
					pl->message ("§7 | Property §abuild-perms §7has been modified.");
				}
			else if (sutils::iequals (prop, "join-perms"))
				{
					std::string str = reader.rest ();
					w->set_join_perms (str);
					pl->message ("§7 | Property §ajoin-perms §7has been modified.");
				}
			else if (sutils::iequals (prop, "dimensions"))
				{
					if (reader.arg_count () != 4)
						{
							pl->message ("§c * §7Usage§f: §e/wconfig set dimensions §cwidth depth");
							return;
						}
					
					auto a_width = reader.next ();
					auto a_depth = reader.next ();
					
					if (!a_width.is_int () || !a_depth.is_int ())
						{
							pl->message ("§c * §7The width and depth of the world must be positive integers§c.");
							return;
						}
					
					int width = a_width.as_int ();
					int depth = a_depth.as_int ();
					
					if ((width <= 0) || (depth <= 0))
						{
							pl->message ("§c * §7Invalid dimensions§c.");
							return;
						}
					
					w->set_size (width, depth);
					w->get_players ().all (
						[] (player *pl)
							{
								pl->rejoin_world ();
							});
					
					pl->message ("§7 | Property §adimensions §7has been modified.");
				}
			else if (sutils::iequals (prop, "generator"))
				{
					std::string str = reader.rest ();
					
					world_generator *gen = world_generator::create (str.c_str (), w->get_generator ()->seed ());
					if (!gen)
						{
							pl->message ("§c * §7No such world generator§f: §c" + str);
							return;
						}
					
					w->set_generator (gen);
					
					pl->message ("§7 | Property §agenerator §7has been modified.");
				}
			else if (sutils::iequals (prop, "seed"))
				{
					if (reader.arg_count () != 3)
						{
							pl->message ("§c * §7Usage§f: §e/wconfig set seed §cseed");
							return;
						}
					
					auto a_seed = reader.next ();
					if (!a_seed.is_int ())
						{
							pl->message ("§c * §7The seed value must be an integer§c.");
							return;
						}
					
					long seed = a_seed.as_int ();
					w->get_generator ()->seed (seed);
					
					pl->message ("§7 | Property §aseed §7has been modified.");
				}
			else
				{
					pl->message ("§c * §7Invalid property§f: §c" + prop);
					return;
				}
			
			w->save_meta ();
		}
		
		static void
		_get_property (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f: §e/wconfig get §cproperty");
					return;
				}
			
			world *w = pl->get_world ();
			std::string& prop = reader.next ();
			
			std::ostringstream ss;
			ss << "§7 | " << w->get_colored_name () << "§7's ";
			
			if (sutils::iequals (prop, "build-perms"))
				{
					ss << "§abuild-perms§7: §f" << w->get_build_perms ();
				}
			else if (sutils::iequals (prop, "join-perms"))
				{
					ss << "§ajoin-perms§7: §f" << w->get_join_perms ();
				}
			else if (sutils::iequals (prop, "dimensions"))
				{
					ss << "§adimensions§7: §f" << w->get_width () << "x" << w->get_depth ();
				}
			else if (sutils::iequals (prop, "seed"))
				{
					ss << "§aseed§7: §f" << w->get_generator ()->seed ();
				}
			else if (sutils::iequals (prop, "generator"))
				{
					ss << "§agenerator§7: §f" << w->get_generator ()->name ();
				}
			else
				{
					pl->message ("§c * §7Invalid property§f: §c" + prop);
					return;
				}
				
			
			pl->message (ss.str ());
		}
		
		static void
		_property_list (player *pl, command_reader& reader)
		{
			pl->message ("§4The following are the properties that you may set§c/§4get§f:");
			pl->message ("§c  build-perms §7- §fDefines which players can build in the world.");
			pl->message ("§c  join-perms  §7- §fDefines which players can join the world.");
			pl->message ("§c  dimensions  §7- §fThe size of the world (width and depth).");
			pl->message ("§c  generator   §7- §fThe chunk generator that the world uses.");
			pl->message ("§c  seed        §7- §fThe seed that the chunk generator uses.");
		}
		
		/* 
		 * /wconfig - 
		 * 
		 * Lets the user view or modify world properties.
		 * 
		 * Permissions:
		 *   - command.world.wconfig
		 *       Needed to execute the command.
		 */
		void
		c_wconfig::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			if (reader.no_args ())
				{
					pl->message ("§c * §7Usage§f:");
					pl->message ("§c *   §7(§f1§7) §e/wconfig get §cproperty");
					pl->message ("§c *   §7(§f2§7) §e/wconfig set §cpropetty ...");
					pl->message ("§c *   §7(§f3§7) §e/wconfig list");
					return;
				}
			
			std::string& com = reader.next ();
			if (sutils::iequals (com, "set"))
				{
					_set_property (pl, reader);
				}
			else if (sutils::iequals (com, "get"))
				{
					_get_property (pl, reader);
				}
			else if (sutils::iequals (com, "list"))
				{
					_property_list (pl, reader);
				}
			else
				{
					pl->message ("§c * §7Usage§f: §e/wconfig §cget/set property ...");
				}
		}
	}
}

