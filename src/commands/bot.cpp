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

#include "commands/bot.hpp"
#include "player/player.hpp"
#include "entities/player_bot.hpp"


namespace hCraft {
  namespace commands {
    
    static void
    _handle_create (player *pl, command_reader& reader)
    {
      if (!reader.has_next ())
        {
          pl->message ("§c * §7Usage§f: §e/bot create §cname");
          return;
        }
      
      std::string botname = reader.next ();
      player_bot *bot = new player_bot (pl->get_server (), botname);
      bot->pos = pl->pos;
      pl->get_world ()->spawn_entity (bot);
      
      pl->message ("§eSpawned bot§f: §1" + botname);
    }
    
    
    
    /*
	   * /bot
	   * 
	   * Create and control player bots.
	   * 
	   * Permissions:
	   *   - command.misc.bot
	   *       Needed to execute the command.
	   */
    void
    c_bot::execute (player *pl, command_reader& reader)
    {
      if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
      if (!reader.has_next ())
        {
          this->show_summary (pl);
          return;
        }
      
      std::string subc = reader.next ();
      if (subc == "create")
        _handle_create (pl, reader);
      else
        {
          pl->message ("§c * §7Invalid subcommand§f: §c" + subc);
        }
    }
  }
}

