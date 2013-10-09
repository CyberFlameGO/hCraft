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

#ifndef _hCraft__COMMANDS__KICK_H_
#define _hCraft__COMMANDS__KICK_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/*
		 * /kick
		 * 
		 * Kicks a player from the server.
		 * 
		 * Permissions:
		 *   - command.admin.kick
		 *       Needed to execute the command.
		 */
		class c_kick: public command
		{
		public:
			const char* get_name () { return "kick"; }
			
			const char*
			get_summary ()
				{ return "Kicks a player from the server."; }
			
			const char*
			get_help ()
			{ return
				".TH KICK 1 \"/kick\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"kick - Kicks a player from the server. "
					".PP "
				".SH SYNOPSIS "
					"$g/kick $yPLAYER $G[REASON] .LN "
					"$g/kick $yOPTION "
					".PP "
				".SH DESCRIPTION "
					"Kicks the player PLAYER from the server, with an optional reason REASON. "
					"Or, if OPTION is specified, then do as follows: "
					".PP "
					"$G\\\\help \\h $gDisplay help "
					".PP "
					"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.admin.kick"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

