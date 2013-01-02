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

#ifndef _hCraft__COMMANDS__DRAW_H_
#define _hCraft__COMMANDS__DRAW_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /selection -
		 * 
		 * A multipurpose command for changing the current selection(s) managed
		 * by the calling player.
		 * 
		 * Permissions:
		 *   - command.draw.selection
		 *       Needed to execute the command.
		 */
		class c_selection: public command
		{
		public:
			const char* get_name () { return "selection"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"sel",
						"s",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "A multipurpose command for changing the current selection(s) managed by the calling player."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.selection"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /fill -
		 * 
		 * Fills all visible selected regions with a block.
		 * 
		 * Permissions:
		 *   - command.draw.fill
		 *       Needed to execute the command.
		 */
		class c_fill: public command
		{
		public:
			const char* get_name () { return "fill"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"f",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Fills all visible selected regions with a block."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.fill"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

