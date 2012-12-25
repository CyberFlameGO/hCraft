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

#ifndef _hCraft__MANUAL_H_
#define _hCraft__MANUAL_H_

#include <vector>
#include <string>


namespace hCraft {
	namespace man {
		
		/* 
		 * Converts the specified string written in a MAN-like markup\macro language
		 * to pretty Minecraft text (as a vector of lines).
		 * 
		 * Format:
		 * 
		 * .TH [name of the thing being described] [section number] [center footer]
		 *     [left footer] [header]
		 */
		void format (std::vector<std::string>& output, int max_line,
			const std::string& input);
	}
}

#endif

