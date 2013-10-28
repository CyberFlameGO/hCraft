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

#ifndef _hCraft__UUID_H_
#define _hCraft__UUID_H_

#include <string>


namespace hCraft {
	
	/* 
	 * Universally unique identifier.
	 */
	struct uuid_t
	{
		unsigned int data[4];
		
	//---
		std::string to_str ();
		
	//---
		bool operator== (uuid_t other) const;
		bool operator!= (uuid_t other) const;
	};
	
	
	
	/* 
	 * Generates and returns a new UUID.
	 */
	uuid_t generate_uuid ();
}

#endif

