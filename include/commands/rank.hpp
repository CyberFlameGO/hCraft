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

#ifndef _hCraft__COMMANDS__RANK_H_
#define _hCraft__COMMANDS__RANK_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
	 	 * /rank
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
	}
}

#endif

