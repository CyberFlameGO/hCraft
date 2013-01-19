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

#ifndef _hCraft__COMMANDS__CHAT_H_
#define _hCraft__COMMANDS__CHAT_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
	
		/* 
		 * /me -
		 * 
		 * Broadcasts an IRC-style action message (in the form of: * user1234
		 * <message>).
		 * 
		 * Permissions:
		 *   - command.chat.me
		 *       Needed to execute the command.
		 */
		class c_me: public command
		{
		public:
			const char* get_name () { return "me"; }
			
			const char*
			get_summary ()
				{ return "Broadcasts an IRC-style action message to all players in the "
								"same world."; }
			
			const char*
			get_help ()
			{ return
				".TH ME 1 \"/me\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
				"me - Broadcast IRC-style action messages "
				".PP "
				".SH SYNOPSIS "
				"$g/me $yACTION .LN "
				"$g/me $yOPTION "
				".PP "
				".SH DESCRIPTION "
				"Broadcasts an IRC action message of the form $'* PLAYER ACTION$' with "
				"PLAYER being the name of the player who executed the command and ACTION "
				"being the text passed to the command as an argument. If OPTION is "
				"specified, then execute according to it: "
				".PP "
				"$G\\\\help \\h $gDisplay help "
				".PP "
				"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.chat.me"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
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

