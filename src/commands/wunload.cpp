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

#include "commands/wunload.hpp"
#include "system/server.hpp"
#include "system/scheduler.hpp"
#include "player/player.hpp"
#include "world/world.hpp"
#include <vector>


namespace hCraft {
	namespace commands {
		
		static bool
		_remove_from_autoload (server& srv, const std::string& world_name)
		{
			soci::session sql (srv.sql_pool ());
			int count;
			sql <<  "SELECT count(*) FROM `autoload-worlds` WHERE `name`='"
				<< world_name << "'", soci::into (count);
			if (count == 0)
				return false;
			
			sql.once << "DELETE FROM `autoload-worlds` WHERE `name`='" << world_name << "'";
			return true;
		}
		
		
		namespace {
			struct callback_data {
				server *srv;
				world *w;
				std::vector<player *> pls;
			};
		}
		
		static void
		_callback_func (scheduler_task& task)
		{
			callback_data *data = static_cast<callback_data *> (task.get_context ());
			server& srv = *(data->srv);
			world *wr = data->w;
			
			std::string colname = wr->get_colored_name ();
			wr->stop ();
			wr->save_all ();
			srv.get_worlds ().remove (wr);
			
			std::vector<player *>& pls = data->pls;
			for (player *pl : pls)
				{
					if (pl->got_known_chunks_for (wr))
						pl->disconnect ();
				}
			
			srv.get_players ().message (
				"§4> §cWorld " + colname + " §chas been unloaded§c!");
			delete wr;
			delete data;
		}
		
		
		/* 
		 * /wunload -
		 * 
		 * Saves and removes a requested world from the server's online world world
		 * list, optionally removing it from the autoload list as well.
		 * 
		 * Permissions:
		 *   - command.world.wunload
		 *       Needed to execute the command.
		 */
		void
		c_wunload::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.wunload"))
				return;
			
			reader.add_option ("autoload", "a");
			if (!reader.parse (this, pl))
				return;
			
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			std::string world_name = reader.arg (0);
			world *wr = pl->get_server ().get_worlds ().find (world_name.c_str ());
			if (!wr)
				{
					if (reader.opt ("autoload")->found ())
						{
							if (_remove_from_autoload (pl->get_server (), world_name))
								pl->message ("§eWorld §b" + world_name + " §ehas been removed from the autoload list§f.");
							else
								pl->message ("§cWorld §7" + world_name + " §cis not in the autoload list§7.");
						}
					else
						pl->message ("§c * §7World §b" + world_name + " §7is not loaded§f.");
					return;
				}
			else if (wr == pl->get_server ().get_main_world ())
				{
					pl->message ("§c * §7You can not unload the main world§c!");
					return;
				}
			
			world_name.assign (wr->get_name ());
			
			// transfer all players to the server's main world.
			std::vector<player *> to_transfer;
			wr->get_players ().populate (to_transfer);
			for (player *pl : to_transfer)
				pl->join_world (pl->get_server ().get_main_world ());
			
			// autoload
			if (reader.opt ("autoload")->found ())
				{
					if (_remove_from_autoload (pl->get_server (), world_name))
						pl->message ("§eWorld §b" + world_name + " §ehas been removed from the autoload list§f.");
					else
						pl->message ("§cWorld §7" + world_name + " §cis not in the autoload list§7.");
				}
			
			pl->message ("§cUnloading world... §7[§85 seconds§7]");
			callback_data *data = new callback_data ();
			data->srv = &pl->get_server ();
			data->w = wr;
			data->pls = to_transfer;
			pl->get_server ().get_scheduler ().new_task (_callback_func, data)
				.run_once (5000);
		}
	}
}
