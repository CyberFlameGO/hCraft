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

#ifndef _hCraft__COMMANDS__HELP_H_
#define _hCraft__COMMANDS__HELP_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /help -
		 * 
		 * When executed without any arguments, the command displays general tips,
		 * tricks and hints about what the player can do in the server. Otherwise,
		 * it displays detailed information about the supplied command.
		 * 
		 * Permissions:
		 *   - command.info.help
		 *       Needed to execute the command.
		 */
		class c_help: public command
		{
		public:
			const char* get_name () { return "help"; }
			
			const char*
			get_summary ()
				{ return "Displays general information about the server or about a "
								 "specific command."; }
			
			const char*
			get_help ()
			{ return 
				".TH HELP 1 \"/help\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
				"help - Displays in-depth information about server features, commands "
				"and the server itself. "
				".SH SYNOPSIS "
				"$g/help $yCOMMAND PAGE .LN "
				"$g/help .LN "
				"$g/help $yOPTION "
				".PP "
				".SH DESCRIPTION "
				"The first form displays general information, common usage and examples "
				"(if any) about the command COMMAND. Since these manuals can be pretty "
				"lengthy at times, they are displayed in pages. One can go through the "
				"pages by providing the page number after the command name (e.g: $'/help "
				"sel 2$'). The second form, when specified without any arguments at all "
				"prints out information about the server itself and might contain links "
				"to other manuals. The final form - if OPTION is specified, execute "
				"accordingly: "
				".PP "
				"$G\\\\help \\h $gDisplays help "
				".PP "
				"$G\\\\summary \\s $gDisplays a short description "
				;}
			
			const char* get_exec_permission () { return "command.info.help"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

