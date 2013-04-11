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

#include "worldc.hpp"
#include "../server.hpp"
#include "../player.hpp"
#include "stringutils.hpp"
#include "cistring.hpp"
#include <sstream>
#include <unordered_map>


namespace hCraft {
	namespace commands {
		
		static void
		handle_on (player *pl, command_reader& reader)
		{
			world *wr = pl->get_world ();
			if (wr->physics_state () == PHY_ON)
				{
					pl->message ("§c * §7Physics are already on§f.");
					return;
				}
			wr->start_physics ();
			wr->get_players ().message ("§a * §6Physics have been turned §aON§6.");
		}
		
		static void
		handle_off (player *pl, command_reader& reader)
		{
			world *wr = pl->get_world ();
			if (wr->physics_state () == PHY_OFF)
				{
					pl->message ("§c * §7Physics are already off§f.");
					return;
				}
			wr->stop_physics ();
			wr->get_players ().message ("§a * §6Physics have been turned §cOFF§6.");
		}
		
		static void
		handle_pause (player *pl, command_reader& reader)
		{
			world *wr = pl->get_world ();
			if (wr->physics_state () == PHY_PAUSED)
				{
					pl->message ("§c * §7Physics are already paused§f.");
					return;
				}
			wr->pause_physics ();
			wr->get_players ().message ("§a * §6Physics have been §7PAUSED§6.");
		}
		
		static void
		handle_threads (player *pl, command_reader& reader)
		{
			world *wr = pl->get_world ();
			
			if (!reader.has_next ())
				{
					std::ostringstream ss;
					ss << "§7This world is currently utilizing §b" << wr->physics.get_thread_count ()
						 << " §7thread" << ((wr->physics.get_thread_count () == 1) ? "" : "s");
					pl->message (ss.str ());
					return;
				}
		
			command_reader::argument narg = reader.next ();
			if (!narg.is_int ())
				{
					pl->message ("§c * §7Syntax§f: §e/physics threads §c<count>");
					return;
				}
			
			int tc = narg.as_int ();
			if (tc < 0 || tc > 24)
				{
					pl->message ("§c * §7Thread count must be in the range of §c0-24");
					return;
				}
			
			wr->physics.set_thread_count (tc);
			pl->message ("§ePhysics thread count as been set to §a" + narg.as_str ());
		}
		
		
		
		/* 
		 * /physics -
		 * 
		 * Changes the current state of physics of the player's world.
		 * 
		 * Permissions:
		 *   - command.world.physics
		 *       Needed to execute the command.
		 */
		void
		c_physics::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.physics"))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			if (reader.no_args () || reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string& opt = reader.next ();
			static std::unordered_map<cistring, void (*)(player *, command_reader &)>
				funs {
						{ "on", handle_on },
						{ "off", handle_off },
						{ "pause", handle_pause },
						{ "threads", handle_threads },
					};
			
			auto itr = funs.find (opt.c_str ());
			if (itr == funs.end ())
				{
					pl->message ("§c * §7Invalid option§f: §c" + opt);
					return;
				}
			
			itr->second (pl, reader);
		}
	}
}

