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

#ifndef _hCraft__COMMANDS__WORLD_H_
#define _hCraft__COMMANDS__WORLD_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /world - 
		 * 
		 * Lets the user modify or view world related information.
		 * 
		 * Permissions:
		 *   - command.world.world
		 *       Needed to execute the command.
		 *   - command.world.world.change-members
		 *       Needed to add\remove world members.
		 *   - command.world.world.change-owners
		 *       Needed to add\remove world owners.
		 *   - command.world.world.owner.change-members
		 *       If a world owner is allowed to add\remove members.
		 *   - command.world.world.owner.change-owners
		 *       If a world owner is allowed to add\remove owners.
		 *   - command.world.world.set-perms
		 *       Required to set build-perms or join-perms
		 *   - command.world.world.get-perms
		 *       Required to view build-perms or join-perms
		 *   - commands.world.world.def-gm
		 *       Needed to change the world's default gamemode.
		 *   - commands.world.world.resize
		 *       Required to resize the world.
		 *   - commands.world.world.regenerate
		 *       Required to regenerate the world.
		 *   - commands.world.world.save
		 *       Required to save the world.
		 *   - commands.world.world.time
		 *       Required to change the time of the world.
		 *   - commands.world.world.pvp
		 *       Required to turn PvP on or off.
		 */
		class c_world : public command
		{
		public:
			const char* get_name () { return "world"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"w",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Lets the user modify or view world related information."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.world"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

