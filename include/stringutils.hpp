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

#ifndef _hCraft__STRINGUTILS_H_
#define _hCraft__STRINGUTILS_H_

#include <string>
#include "blocks.hpp"


namespace hCraft {
	
	/* 
	 * String-related utility and helper functions.
	 */
	namespace sutils {
		
		/* 
		 * Removes leading whitespace from the given string.
		 */
		std::string& ltrim (std::string& s);
		
		/* 
		 * Removes trailing whitespace from the given string.
		 */
		std::string& rtrim (std::string& s);
		
		/* 
		 * Removes whitespace from both ends of the given string.
		 */
		std::string& trim (std::string& s);
		
		
		/* 
		 * Check whether the specified string will be empty after trimming from
		 * both ends.
		 */
		bool is_empty (const std::string& s);
		
		
		/* 
		 * Case-insensitive string equality check.
		 */
		bool iequals (const std::string& a, const char *b);
		bool iequals (const std::string& a, const std::string& b);
		
		
		bool is_int (const std::string& str);
		int  to_int (const std::string& str);
		
		bool is_float (const std::string& str);
		double to_float (const std::string& str);
		
		bool is_block (const std::string& str);
		blocki to_block (const std::string& str);
		
		
		
		/* 
		 * Checks whether the specified string is an IPv4 address.
		 */
		bool is_ip_addr (const std::string& str);
		
		
		/* 
		 * MC usernames may only contain digits, letters and underscores.
		 */
		bool is_valid_username (const std::string& str);
		
		
		
		/* 
		 * Converts color codes from &X to §X.
		 */
		std::string convert_colors (const std::string& str);
	}
}

#endif

