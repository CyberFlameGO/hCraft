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

#ifndef _hCraft__COMMANDS__MISCELLANEOUS_H_
#define _hCraft__COMMANDS__MISCELLANEOUS_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
	
		/* 
		 * /ping -
		 * 
		 * Displays to the player how much time it takes for the server to both
		 * send and retreive a single keep alive (ping: 0x00) packet (in ms).
		 * 
		 * Permissions:
		 *   - command.misc.ping
		 *       Needed to execute the command.
		 */
		class c_ping: public command
		{
		public:
			const char* get_name () { return "ping"; }
		
			const char*
			get_summary ()
				{ return "Checks how much time it takes (in milliseconds) to ping and "
								 "get a response from a player."; }
		
			const char*
			get_help ()
			{ return
				".TH PING 1 \"/ping\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
				"ping - Ping a player and display the amount of time it took. "
				".PP "
				".SH SYNOPSIS "
				"$g/ping $yPLAYER .LN "
				"$g/ping .LN "
				"$g/ping $yOPTION "
				".PP "
				".SH DESCRIPTION "
				"Pings the player PLAYER and displays the amount of milliseconds it took "
				"the operation to complete. Or more specifically, a $'keep-alive$' packet "
				"is sent to the player and the amount of time it takes for a response ("
				"another $'keep-alive$' packet) to appear is measured. This measuring is "
				"not actually done on demand, but rather every five seconds. If PLAYER is "
				"not specified, the player who executed the command is tested instead. "
				"With OPTION, do as following: "
				".PP "
				"$G\\\\help \\h $gDisplay help "
				".PP "
				"$G\\\\summary \\s $gDisplay a short description "
				".SH BUGS "
				"Trying the command on a player who has been logged in for less than five "
				"seconds will result in an inaccurate measurement. "
				;}
		
			const char* get_exec_permission () { return "command.misc.ping"; }
		
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

