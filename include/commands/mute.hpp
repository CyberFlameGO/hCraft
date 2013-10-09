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

#ifndef _hCraft__COMMANDS__MUTE_H_
#define _hCraft__COMMANDS__MUTE_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/*
		 * /mute
		 * 
		 * Mutes a player for a specified amount of time.
		 * 
		 * Permissions:
		 *   - command.admin.mute
		 *       Needed to execute the command.
		 */
		class c_mute: public command
		{
		public:
			const char* get_name () { return "mute"; }
			
			const char*
			get_summary ()
				{ return "Mutes a player for a specified amount of time."; }
			
			const char*
			get_help ()
			{ return
				".TH MUTE 1 \"/mute\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"mute - Mutes a player for a specified amount of time. "
					".PP "
				".SH SYNOPSIS "
					"$g/mute $yPLAYER TIME $G[REASON] .LN "
					"$g/mute $yOPTION "
					".PP "
				".SH DESCRIPTION "
					"Mutes the player PLAYER for the given amount of time TIME. The format "
					"of TIME should consist of a number followed by an appropriate unit "
					"of time (s - seconds, m - minutes and h - hours) (NOTE: If the time "
					"unit is omitted, it is assumed to be in minutes). If OPTION is specified, "
					"then the action taken will be as follows: "
					".PP "
					"$G\\\\help \\h $gDisplay help "
					".PP "
					"$G\\\\summary \\s $gDisplay a short description "
					".PP "
				".SH EXAMPLES "
					"/mute testdude 10m .LN "
					"/mute testdude2 25s Stop talking .LN "
					"/mute testdude3 "
				;}
			
			const char* get_exec_permission () { return "command.admin.mute"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

