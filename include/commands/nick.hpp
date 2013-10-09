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

#ifndef _hCraft__COMMANDS__NICK_H_
#define _hCraft__COMMANDS__NICK_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /nick -
		 * 
		 * Changes the nickname of a player to the one requested.
		 * 
		 * Permissions:
		 *   - command.chat.nick
		 *       Needed to execute the command.
		 */
		class c_nick: public command
		{
		public:
			const char* get_name () { return "nick"; }
			
			const char*
			get_summary ()
				{ return "Changes the nickname of a player to the one requested."; }
			
			const char*
			get_help ()
			{ return
				".TH NICK 1 \"/nick\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
				"nick - Modifies a player's nickname "
				".PP "
				".SH SYNOPSIS "
				"$g/nick $yPLAYER $yNICKNAME .LN "
				"$g/nick $yOPTION "
				".PP "
				".SH DESCRIPTION "
				"Changes the nickname of player PLAYER to NICKNAME. If NICKNAME is not "
				"specified, the player's nickname will be removed. If OPTION is "
				"specified, execute according to it: "
				".PP "
				"$G\\\\help \\h $gDisplay help "
				".PP "
				"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.chat.nick"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

