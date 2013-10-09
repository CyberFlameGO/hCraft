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

#ifndef _hCraft__COMMANDS__MONEY_H_
#define _hCraft__COMMANDS__MONEY_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
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
	}
}

#endif

