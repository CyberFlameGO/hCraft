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

#ifndef _hCraft__COMMANDS__BAN_H_
#define _hCraft__COMMANDS__BAN_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/*
		 * /ban
		 * 
		 * Permanently bans a player from the server.
		 * 
		 * Permissions:
		 *   - command.admin.ban
		 *       Needed to execute the command.
		 *   - command.admin.ban.ip
		 *       Required to issue IP bans.
		 *   - command.admin.ban.ip*
		 *       Required to issue regular bans on accounts that belong to a certain
		 *       IP address.
		 */
		class c_ban: public command
		{
		public:
			const char* get_name () { return "ban"; }
			
			const char*
			get_summary ()
				{ return "Permanently bans a player from the server."; }
			
			const char*
			get_help ()
			{ return
				".TH BAN 1 \"/ban\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"ban - Permanently bans a player from the server. "
					".PP "
				".SH SYNOPSIS "
					"$g/ban $yPLAYER $G[REASON] .LN "
					"$g/ban $yOPTION "
					".PP "
				".SH DESCRIPTION "
					"Bans the player PLAYER from the server, with an optional reason REASON. "
					"Or, if OPTION is specified, then do as follows: "
					".PP "
					"$G\\\\help \\h $gDisplay help "
					".PP "
					"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.admin.ban"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

