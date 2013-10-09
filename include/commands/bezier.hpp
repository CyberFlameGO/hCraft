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

#ifndef _hCraft__COMMANDS__BEZIER_H_
#define _hCraft__COMMANDS__BEZIER_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /bezier -
		 * 
		 * Draws a beizer curvee using between user-specified control points.
		 * The curve will pass through the first and last points, but not necessarily
		 * through the rest.
		 * 
		 * Permissions:
		 *   - command.draw.bezier
		 *       Needed to execute the command.
		 */
		class c_bezier: public command
		{
		public:
			const char* get_name () { return "bezier"; }
			
			const char*
			get_summary ()
				{ return "Draws a beizer curve using between user-specified control points."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.bezier"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

