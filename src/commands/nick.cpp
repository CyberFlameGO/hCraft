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

#include "commands/chatc.hpp"
#include "player.hpp"
#include "server.hpp"
#include "rank.hpp"
#include <cstring>
#include <sstream>


namespace hCraft {
	namespace commands {
		
		/* 
		 * /nick -
		 * 
		 * Changes the nickname of a player to the one requested.
		 * 
		 * Permissions:
		 *   - command.chat.nick
		 *       Needed to execute the command.
		 */
		void
		c_nick::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.chat.nick") || !reader.parse (this, pl))
				return;
			
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string target_name = reader.arg (0);
			std::string nickname, prev_nickname;
			
			player *target = pl->get_server ().get_players ().find (target_name.c_str ());
			if (target)
				target_name.assign (target->get_username ());
			auto& conn = pl->get_server ().sql ().pop ();
			auto stmt = conn.query (
				"SELECT * FROM `players` WHERE `name` LIKE ?");
			sql::row row;
			
			stmt.bind (1, target_name.c_str (), sql::pass_transient);
			if (!stmt.step (row))
				{
					pl->message ("§c * §7Unable to find player§f: §c" + target_name);
					return;
				}
			
			rank rnk (row.at (5).as_cstr (), pl->get_server ().get_groups ());
			prev_nickname.assign (row.at (2).as_cstr ());
			if (reader.arg_count () >= 2)
				nickname.assign (reader.all_from (1));
			else
				nickname.assign (row.at (1).as_cstr ());
			if (nickname.empty () || nickname.length () > 36)
				{
					pl->message ("§c * §7A nickname cannot have more than §c36 "
											 "§7characters or be empty§f.");
					return;
				}
			else if (std::strcmp (nickname.c_str (), row.at (2).as_cstr ()) == 0)
				{
					pl->message ("§ePlayer §a" + target_name + " §ealready has that nickname§f.");
					return;
				}
			
			{
				std::ostringstream ss;
				if (target)
					target->set_nickname (nickname.c_str (), false);
				ss << "UPDATE `players` SET `nick`='"
					 << nickname << "' WHERE `name`='"
					 << target_name << "';";
				conn.execute (ss.str ());
						
				ss.clear (); ss.str (std::string ());
				ss << "§" << rnk.main ()->color << prev_nickname
					 << " §eis now known as§f: §" << rnk.main ()->color
					 << nickname << "§f!";
				pl->get_server ().get_players ().message (ss.str ());
			}
			
			pl->get_server ().sql ().push (conn);
		}
	}
}

