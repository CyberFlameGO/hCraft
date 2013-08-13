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

#ifndef _hCraft__COMMANDS__INFORMATION_H_
#define _hCraft__COMMANDS__INFORMATION_H_

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
		
		
		/* 
		 * /status
		 * 
		 * Displays information about a specified player.
		 * 
		 * Permissions:
		 *   - command.info.status
		 *       Needed to execute the command.
		 *
		 *   - command.info.status.nick
		 *   - command.info.status.ip
		 *   - command.info.status.rank
		 *   - command.info.status.logins
		 *   - command.info.status.blockstats
		 *   - command.info.status.balance
		 */
		class c_status: public command
		{
		public:
			const char* get_name () { return "status"; }
			
			const char*
			get_summary ()
				{ return "Displays information about a specified player."; }
			
			const char*
			get_help ()
			{ return
				".TH STATUS 1 \"/status\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"status - Displays information about a specified player. "
					".PP "
				".SH SYNOPSIS "
					"$g/status $yPLAYER .LN "
					"$g/status $yOPTION "
					".PP "
				".SH DESCRIPTION "
					"Displays some basic information about the specified player, such as "
					"when they first logged in, last logout date, rank; along with some "
					"miscellaneous data that includes, amount of blocks placed, blocks "
					"destroyed, current money balance, etc...  If the user has the required "
					"permissions, administrative information will be included as well. "
					"If OPTION is specified, then do as follows: "
					".PP "
					"$G\\\\help \\h $gDisplay help "
					".PP "
					"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.info.status"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /money -
		 * 
		 * Displays the amount of money a player has.
		 * 
		 * Permissions:
		 *   - command.info.money
		 *       Needed to execute the command.
		 *   - command.info.money.others
		 *       Required to display the amount of money _other_ players have.
		 *   - command.info.money.pay
		 *       Whether the player is allowed to send money out of their own account.
		 *   - command.info.money.give
		 *       Whether the player is allowed to give money to players _wihout_
		 *       taking it out of their own account.
		 *   - command.info.money.set
		 *       Required to modify a player's balance directly.
		 */
		class c_money: public command
		{
		public:
			const char* get_name () { return "money"; }
			
			const char*
			get_summary ()
				{ return "Displays the amount of money a player has."; }
			
			const char*
			get_help ()
			{ return
				".TH MONEY 1 \"/money\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"money - Displays the amount of money a player has. "
					".PP "
				".SH SYNOPSIS "
					"$g/money $G[PLAYER] .LN "
					"$g/money pay $y[PLAYER] AMOUNT .LN "
					"$g/money give $y[PLAYER] AMOUNT .LN "
					"$g/money set $y[PLAYER] AMOUNT .LN "
					"$g/money $yOPTION "
					".PP "
				".SH DESCRIPTION "
					"In the same order that they are described in the SYNOPSIS section "
					"above, here the are detailed explanations for the subcommands: "
					".PP "
					"$G/money $g[PLAYER] "
						".TP Displays the amount of money the player PLAYER has, or, if "
						"PLAYER is omitted, the amount of the executor is displayed instead. .PP "
					".PP "
					"$G\\\\help \\h $gDisplay help "
					".PP "
					"$G\\\\summary \\s $gDisplay a short description "
				;}
			
			const char* get_exec_permission () { return "command.info.money"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
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

