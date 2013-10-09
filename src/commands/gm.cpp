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

#include "commands/gm.hpp"
#include "player/player.hpp"
#include "util/stringutils.hpp"
#include "system/server.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /gm -
		 * 
		 * Changes the gamemode of the executor or of a specified player.
		 * 
		 * Permissions:
		 *   - command.admin.gm
		 *       Needed to execute the command.
		 */
		void
		c_gm::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.admin.gm"))
					return;
		
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 2)
				{ this->show_summary (pl); return; }
			
			player *target = pl;
			if (reader.arg_count () == 2)
				{
					std::string& plname = reader.next ().as_str ();
					target = pl->get_server ().get_players ().find (plname.c_str ());
					if (!target)
						{
							pl->message ("§c * §7No such player§f: §c" + plname);
							return;
						}
				}
			
			std::string& gmstr = reader.next ().as_str ();
			gamemode_type gm   = GT_SURVIVAL;
			if (gmstr.size () == 1)
				{
					switch (gmstr[0])
						{
						case 's': break;
						case 'c': gm = GT_CREATIVE; break;
						//case 'a': gm = GT_ADVENTURE; break;
						
						default:
							pl->message ("§c * §7Invalid gamemode type§f: §c" + gmstr);
							return;
						}
				}
			else
				{
					if (sutils::iequals (gmstr, "survival"))
						;
					else if (sutils::iequals (gmstr, "creative"))
						gm = GT_CREATIVE;
					//else if (sutils::iequals (gmstr, "adventure"))
					//	gm = GT_ADVENTURE;
					else
						{
							pl->message ("§c * §7Invalid gamemode type§f: §c" + gmstr);
							return;
						}
				}

			if (gm == target->gamemode ())
				{
					if (pl == target)
						target->message ("§c * §7You already have that gamemode set§f.");
					else
						{
							std::ostringstream ss;
							ss << "§c * §7" << target->get_colored_username ()
								 << " §7already has that gamemode set§f.";
							target->message (ss.str ());
						}
					return;
				}
			
			const char *gm_name = (gm == GT_SURVIVAL) ? "survival"
				: ((gm == GT_CREATIVE) ? "creative" : "adventure");
			if (pl != target)
				{
					std::ostringstream ss;
					ss << target->get_colored_username () << " §egamemode has been set to§f: §4" << gm_name;
					pl->message (ss.str ());
				}
			
			target->change_gamemode (gm);
			
			/*
			// Commented out due to Minecraft's ugly "Your gamemode has been updated."
			// message.
			
			std::ostringstream ss;
			ss << "§6Your gamemode has been set to§f: §c" << gm_name;
			target->message (ss.str ());
			*/
		}
	}
}

