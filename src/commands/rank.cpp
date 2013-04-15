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

#include "adminc.hpp"
#include "player.hpp"
#include "server.hpp"
#include "sqlops.hpp"
#include "rank.hpp"


namespace hCraft { 
	namespace commands {
		
		/* /rank
		 * 
		 * Changes the rank of a specified player.
		 * 
		 * Permissions:
		 *   - command.admin.rank
		 *       Needed to execute the command.
		 */
		void
		c_rank::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
		
			if (!reader.parse (this, pl))
					return;
			if (reader.arg_count () != 2)
				{ this->show_summary (pl); return; }
			
			player *target = nullptr;
			std::string target_name = reader.next ();
			
			std::string rank_str = reader.next ();
			rank new_rank;
			try
				{
					new_rank.set (rank_str.c_str (), pl->get_server ().get_groups ());
				}
			catch (const std::exception& str)
				{
					pl->message ("§c * §7Invalid rank§f: §c" + rank_str);
						return;
				}
			
			sqlops::player_info pinf;
			{
				auto& conn = pl->get_server ().sql ().pop ();
				if (!sqlops::player_data (conn, target_name.c_str (), pl->get_server (), pinf))
					{
						pl->message ("§c * §7Unknown player§f: §c" + target_name);
						return;
					}
				
				target_name.assign (pinf.name);
				target = pl->get_server ().get_players ().find (target_name.c_str ());
				
				if (!pl->is_op ())
					{
						if (target_name == pl->get_username ())
							{
								pl->message ("§c * §7You cannot change your own rank§c.");
								return;
							}
						else if (pinf.rnk <= pl->get_rank ())
							{
								pl->message ("§c * §7You cannot change the rank of someone higher than you§c.");
								return;
							}
						else if (new_rank >= pl->get_rank ())
							{
								pl->message ("§c * §7You cannot give a player a rank that is higher than yours§c.");
								return;
							}
					}
				
				sqlops::modify_player_rank (conn, target_name.c_str (), rank_str.c_str ());
				if (target)
					{
						target->set_rank (new_rank);
					}
				
				pl->get_server ().sql ().push (conn);
			}
		}
	}
}

