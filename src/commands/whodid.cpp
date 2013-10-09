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

#include "commands/whodid.hpp"
#include "system/server.hpp"
#include "player/player.hpp"
#include "world/world.hpp"
#include "util/stringutils.hpp"
#include "util/utils.hpp"
#include "system/sqlops.hpp"
#include <sstream>
#include <ctime>
#include <map>


namespace hCraft {
	namespace commands {
		
		namespace {
			struct whodid_data {
				int page;
			};
		}
		
		
		static bool
		on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			whodid_data *data = static_cast<whodid_data *> (pl->get_data ("whodid"));
			if (!data) return true; // shouldn't happen
			
			std::map<unsigned int, sqlops::player_info> players;
			
			block_history hist;
			pl->get_world ()->blhi.get (marked[0].x, marked[0].y, marked[0].z, hist);
			if (hist.empty ())
				{
					std::ostringstream ss;
					ss << "§8Whodid§f: §7No block history for [§f" << marked[0].x << "§7, §f" << marked[0].y << "§7, §f" << marked[0].z << "§7]";
					pl->message (ss.str ());
					return true;
				}
			
			std::ostringstream ss;
			ss << "§8Whodid§f: §7Block history records for [§f" << marked[0].x << "§7, §f" << marked[0].y << "§7, §f" << marked[0].z << "§7]:";
			pl->message (ss.str ());
			
			int page_size = 6;
			int page_count = hist.size () / page_size + !!(hist.size () % page_size);
			int page = (data->page >= 0) ? data->page : (page_count - 1);
			
			soci::session sql (pl->get_server ().sql_pool ());
			for (size_t i = (page * page_size); i < (size_t)((page + 1) * page_size) && i < hist.size (); ++i)
				{
					block_history_record& rec = hist[i];
					
					ss.str (std::string ());
					ss << "§f > §c#" << (int)(i + 1) << " ";
					if (rec.newt.id == BT_AIR)
						ss << "§4Destroyed by ";
					else
						ss << "§3Created by ";
					
					// get player
					sqlops::player_info *pinf = nullptr;
					auto p_itr = players.find (rec.pid);
					if (p_itr == players.end ())
						{
							pinf = &players[rec.pid];
							if (!sqlops::player_data (sql, rec.pid, pl->get_server (), *pinf))
								{
									pinf->rnk = pl->get_server ().get_groups ().default_rank;
									pinf->name = "???";
								}
						}
					else
						pinf = &p_itr->second;
					ss << "§" << pinf->rnk.main ()->color << pinf->name << " ";
					
					// types
					block_info *old_inf = block_info::from_id (rec.oldt.id);
					block_info *new_inf = block_info::from_id (rec.newt.id);
					ss << "§e[from §7" << old_inf->name;
					if (rec.oldt.meta != 0)
						ss << ":" << (int)rec.oldt.meta;
					ss << " §eto §7" << new_inf->name;
					if (rec.newt.meta != 0)
						ss << ":" << (int)rec.newt.meta;
					ss << "§e]";
					
					pl->message (ss.str ());
					ss.str (std::string ());
					
					// time
					std::string first_relative;
					utils::relative_time (std::time (nullptr), rec.tm, first_relative);
					ss << "§f   - §b" << first_relative;
					pl->message (ss.str ());
				}
			
			ss.str (std::string ());
			ss << "§6Displayed page §e" << (page + 1) << "§6/§e" << page_count << " §6[§8" << hist.size () << " §7records§6]";
			pl->message (ss.str ());
			
			pl->delete_data ("whodid");
			return true;
		}
		
		
		/* 
		 * /whodid
		 * 
		 * Displays modification records for blocks selected by the user.
		 * 
		 * Permissions:
		 *   - command.info.whodid
		 *       Needed to execute the command.
		 */
		void
		c_whodid::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
		
			reader.add_option ("page", "p", 1, 1);
			if (!reader.parse (this, pl))
				return;
			
			int page = -1;
			auto opt_page = reader.opt ("page");
			if (opt_page->found ())
				{
					auto& arg = opt_page->arg (0);
					if (!arg.is_int () || (page = arg.as_int ()) <= 0)
						{
							pl->message ("§c * §7Option to §4--§cpage §7must be an integer > 0");
							return;
						}
				}
			
			whodid_data *data = new whodid_data {page - 1};
			pl->create_data ("whodid", data,
				[] (void *ptr) { delete static_cast<whodid_data *> (ptr); });
			pl->get_nth_marking_callback (1) += on_blocks_marked;
			
			pl->message ("§7 | §ePlease mark §bone §eblock.");
		}
	}
}

