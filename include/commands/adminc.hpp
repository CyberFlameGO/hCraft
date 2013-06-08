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

#ifndef _hCraft__COMMANDS__ADMIN_H_
#define _hCraft__COMMANDS__ADMIN_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /gm -
		 * 
		 * Changes the gamemode of the executor or of a specified player.
		 * 
		 * Permissions:
		 *   - command.admin.gm
		 *       Needed to execute the command.
		 */
		class c_gm: public command
		{
		public:
			const char* get_name () { return "gm"; }
			
			const char*
			get_summary ()
				{ return "Changes the gamemode of the executor or of a specified player."; }
			
			const char*
			get_help ()
			{ return
				".TH GM 1 \"/gm\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"me - Changes the gamemode of the executor or of a specified player. "
					".PP "
				".SH SYNOPSIS "
					"$g/gm $G[PLAYER] $yGAMEMODE .LN "
					"$g/gm $yOPTION "
					".PP "
				".SH DESCRIPTION "
					"Changes the gamemode of the player PLAYER (or of the player that "
					"executes the command, if not specified) to GAMEMODE. Possible "
					"gamemodes include: (s)urvival and (c)reative. If OPTION "
					"is specified, then do as follows: "
					".PP "
					"$G\\\\help \\h $gDisplay help "
					".PP "
					"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.admin.gm"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* /rank
		 * 
		 * Changes the rank of a specified player.
		 * 
		 * Permissions:
		 *   - command.admin.rank
		 *       Needed to execute the command.
		 */
		class c_rank: public command
		{
		public:
			const char* get_name () { return "rank"; }
			
			const char*
			get_summary ()
				{ return "Changes the rank of a specified player."; }
			
			const char*
			get_help ()
			{ return
				".TH RANK 1 \"/rank\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"rank - Changes the rank of a specified player. "
					".PP "
				".SH SYNOPSIS "
					"$g/rank $yPLAYER RANK .LN "
					"$g/rank $yOPTION "
					".PP "
				".SH DESCRIPTION "
					"Sets the rank of player PLAYER to RANK. The rank string should "
					"be in the form of $G[@]<group1>[[ladder]];<group2>[[ladder]];... "
					"$ye.g.: $G$'@architect[normal];vip$'$y, $G$'guest$'$y, $G$'admin;@citizen$'$y, "
					"etc... Or, if OPTION is specified, then do as follows: "
					".PP "
					"$G\\\\help \\h $gDisplay help "
					".PP "
					"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.admin.rank"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* /kick
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
		
		
		/* /ban
		 * 
		 * Permanently bans a player from the server.
		 * 
		 * Permissions:
		 *   - command.admin.ban
		 *       Needed to execute the command.
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
					"kick - Permanently bans a player from the server. "
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
		
		
		/* /unban
		 * 
		 * Revokes a permanent ban from a specified player.
		 * 
		 * Permissions:
		 *   - command.admin.unban
		 *       Needed to execute the command.
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
					"kick - Permanently bans a player from the server. "
					".PP "
				".SH SYNOPSIS "
					"$g/ban $yPLAYER $G[REASON] .LN "
					"$g/ban $yOPTION "
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

