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

#ifndef _hCraft__COMMANDS__BOT_H_
#define _hCraft__COMMANDS__BOT_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/*
		 * /bot
		 * 
		 * Create and control player bots.
		 * 
		 * Permissions:
		 *   - command.misc.bot
		 *       Needed to execute the command.
		 */
		class c_bot: public command
		{
		public:
			const char* get_name () { return "bot"; }
			
			const char*
			get_summary ()
				{ return "Create and control player bots."; }
			
			const char*
			get_help ()
			  { return ""; }
			
			const char* get_exec_permission () { return "command.misc.bot"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

