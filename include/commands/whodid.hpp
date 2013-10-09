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

#ifndef _hCraft__COMMANDS__WHODID_H_
#define _hCraft__COMMANDS__WHODID_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /whodid
		 * 
		 * Displays modification records for blocks selected by the user.
		 * 
		 * Permissions:
		 *   - command.info.whodid
		 *       Needed to execute the command.
		 */
		class c_whodid: public command
		{
			const char* get_name () { return "whodid"; }
			
			const char**
			get_aliases ()
			{
				static const char* aliases[] =
					{
						"wd",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Displays modification records for blocks selected by the user."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.info.whodid"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

