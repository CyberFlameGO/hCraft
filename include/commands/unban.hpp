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

#ifndef _hCraft__COMMANDS__UNBAN_H_
#define _hCraft__COMMANDS__UNBAN_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/*
		 * /unban
		 * 
		 * Revokes a permanent ban from a specified player.
		 * 
		 * Permissions:
		 *   - command.admin.unban
		 *       Needed to execute the command.
		 *   - command.admin.unban.ip
		 *       Required to lift IP bans.
		 */
		class c_unban: public command
		{
		public:
			const char* get_name () { return "unban"; }
			
			const char*
			get_summary ()
				{ return "Revokes a permanent ban from a specified player."; }
			
			const char*
			get_help ()
			{ return
				".TH UNBAN 1 \"/unban\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"unban - Revokes a permanent ban from a specified player. "
					".PP "
				".SH SYNOPSIS "
					"$g/unban $yPLAYER $G[REASON] .LN "
					"$g/unban $yOPTION "
					".PP "
				".SH DESCRIPTION "
					"Removes the ban placed on player PLAYER, and logs the optional reason "
					"REASON (the default reason is $'No reason specified$'). "
					"Or, if OPTION is specified, then do as follows: "
					".PP "
					"$G\\\\help \\h $gDisplay help "
					".PP "
					"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.admin.unban"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

