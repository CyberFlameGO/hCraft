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

#include "stringutils.hpp"
#include <cctype>


namespace hCraft {
	namespace sutils {
		
		/* 
		 * Removes leading whitespace from the given string.
		 */
		std::string&
		ltrim (std::string& s)
		{
			int j = s.size (), i = 0;
			for (; i < j; ++i)
				if (!std::isspace (s[i]))
					break;
			s.erase (0, i);
			return s;
		}
		
		/* 
		 * Removes trailing whitespace from the given string.
		 */
		std::string&
		rtrim (std::string& s)
		{
			int i;
			for (i = s.size () - 1; i >= 0; --i)
				if (!std::isspace (s[i]))
					break;
			if (i != (int)(s.size () + 1))
				s.erase (i + 1, s.size () - (i + 1));
			return s;
		}
		
		/* 
		 * Removes whitespace from both ends of the given string.
		 */
		std::string&
		trim (std::string& s)
			{ return ltrim (rtrim (s)); }
		
		
		
		/* 
		 * Check whether the specified string will be empty after trimming from
		 * both ends.
		 */
		bool
		is_empty (const std::string& s)
		{
			int char_count = s.size ();
			int i, j;
			
			for (i = 0; i < (int)s.size (); ++i)
				{
					if (s[i] == ' ')
						-- char_count;
					else
						break;
				}
			
			for (j = (i - 1); j >= 0; --j)
				{
					if (s[j] == ' ')
						-- char_count;
					else
						break;
				}
			
			return char_count == 0;
		}
		
		
		/* 
		 * Case-insensitive string equality check.
		 */
		bool
		iequals (const std::string& a, const char *b)
		{
			int len = a.length (), i = 0;
			for (; i < len; ++i)
				{
					char ca = a[i];
					char cb = b[i];
					if (cb == 0)
						return false;
					
					if (std::tolower (ca) != std::tolower (cb))
						return false;
				}
			
			if (b[i] != 0)
				return false;
			return true;
		}
		
		bool
		iequals (const std::string& a, const std::string& b)
		{
			return iequals (a, b.c_str ());
		}
	}
}

