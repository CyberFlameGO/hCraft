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

#include "commands/worldc.hpp"
#include "server.hpp"
#include "player.hpp"
#include "world.hpp"
#include "stringutils.hpp"
#include "utils.hpp"


namespace hCraft {
	namespace commands {
		
		static world*
		_create_realm (player *pl, const std::string& realm_name)
		{
			int world_width = 64;
			int world_depth = 64;
			const char *gen_name = "flatgrass";
			const char *prov_name = "hw";
			long gen_seed = utils::ns_since_epoch ();
			
			world_generator *gen = world_generator::create (gen_name, gen_seed);
			world_provider *prov = world_provider::create (prov_name,
				"data/worlds", realm_name.c_str ());
			
			world *wr = new world (WT_LIGHT, pl->get_server (), realm_name.c_str (),
				pl->get_logger (), gen, prov);
			wr->set_width (world_width);
			wr->set_depth (world_depth);
			wr->prepare_spawn (10, true);
			wr->save_all ();
			
			wr->security ().add_owner (pl->pid ());
			
			if (!pl->get_server ().register_world (wr))
				{
					delete wr;
					pl->message ("§cFailed to load world§7.");
				}
			
			wr->start ();
			return wr;
		}
		
		
		static world*
		_load_realm (player *pl, const std::string& realm_name)
		{
			try
				{
					world *wr = world::load_world (pl->get_server (), realm_name.c_str ());
					return wr;
				}
			catch (const std::exception& ex)
				{
					pl->message (std::string ("§c * §7Failed to load realm§f: §c") + ex.what ());
				}
			
			return nullptr;
		}
		
		
		/* 
		 * /realm - 
		 * 
		 * Inspired by WOM realms.
		 * Lets players visit realms (worlds).
		 * If a player tries to access a realm/world that matches their username,
		 * it will be created automatically if it did not already exist.
		 * 
		 * Permissions:
		 *   - command.world.realm
		 *       Needed to execute the command.
		 */
		void
		c_realm::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			if (reader.arg_count () > 1)
				{
					pl->message ("§c * §7Usage§f: §e/realm §crealm-name");
					return;
				}
			
			std::string realm_name = reader.no_args () ? pl->get_username () : reader.next ().as_str ();
			
			world *wr = pl->get_server ().get_worlds ().find (realm_name.c_str ());
			if (!wr)
				{
					if (sutils::iequals (realm_name, pl->get_username ()))
						wr = _create_realm (pl, realm_name);
					else
						{
							// try to load it
							wr = _load_realm (pl, realm_name);
							if (!wr)
								{
									pl->message ("§c * §7Cannot find realm§f: §c" + realm_name);
									return;
								}
						}
				}
			
			if (wr == pl->get_world ())
				{
					pl->message ("§c * §7Already there§c." );
					return;
				}
			
			if (!wr->security ().can_join (pl))
				{
					pl->message ("§4 * §cYou are not allowed to go there§4.");
					return;
				}
			
			pl->join_world (wr);
		}
	}
}
