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

#include "commands/chatc.hpp"
#include "player.hpp"
#include "stringutils.hpp"
#include "server.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		static void
		global_broadcast (player *pl, command_reader& reader, const std::string& msg)
		{
			if (msg.empty ())
				{
					pl->message ("§c * §7Usage§f: §e/say §cmessage");
					return;
				}
			
			pl->get_server ().get_players ().message (msg);
		}
		
		static void
		imitate_player (player *pl, command_reader &reader, const std::string& name,
			const rank& rnk, const std::string& msg)
		{
			std::ostringstream ss;
			
			group *mgrp = rnk.main ();
			ss << mgrp->mprefix;
			for (group *grp : rnk.groups)
				ss << grp->prefix;
			ss << "§" << mgrp->color << name;
			ss << mgrp->msuffix;
			for (group *grp : rnk.groups)
				ss << grp->suffix;
			ss << " §" << mgrp->text_color << msg;
		
			std::string out = ss.str ();
			pl->get_world ()->get_players ().message (out);
		}
		
		/* 
		 * /say -
		 * 
		 * Broadcasts a message onto global chat.
		 * 
		 * Permissions:
		 *   - command.chat.say
		 *       Needed to execute the command.
		 *   - command.chat.say.imitate
		 *       Needed to imitate other players.
		 */
		void
		c_say::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			reader.add_option ("player", "p", 1, 2);
			if (!reader.parse (this, pl))
				return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string imitate_name;
			rank imitate_rank;
			bool imitate = false;
			
			auto opt_player = reader.opt ("player");
			if (opt_player->found ())
				{
					imitate = true;
					imitate_name = opt_player->arg (0).as_str ();
					
					if (opt_player->arg_count () > 1)
						{
							try
								{
									imitate_rank.set (opt_player->arg (1).as_cstr (), pl->get_server ().get_groups ());
								}
							catch (const std::exception&)
								{
									pl->message ("§c * §7Invalid rank§f: §c" + opt_player->arg (1).as_str ());
									return;
								}
						}
					else
						{
							player *target = pl->get_server ().get_players ().find (imitate_name.c_str ());
							if (!target)
								{
									pl->message ("§c * §7Unknown player§f: §c" + imitate_name);
									return;
								}
							
							imitate_name = target->get_nickname ();
							imitate_rank = target->get_rank ();
						}
				}
			
			if (imitate)
				{
					imitate_player (pl, reader, imitate_name, imitate_rank, reader.rest ());
				}
			else
				{
					global_broadcast (pl, reader, reader.rest ());
				}
		}
	}
}

