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

#include "util/uuid.hpp"
#include "util/utils.hpp"
#include <mutex>
#include <random>


namespace hCraft {
	
	/* 
	 * Generates and returns a new UUID.
	 */
	uuid_t
	generate_uuid ()
	{
		static std::mt19937 _rnd {};
		static std::mutex _rnd_mut {};
		static std::uniform_int_distribution<> _dis (0, 0xF);
		static bool _init = false;
		
		if (!_init)
			{
				_init = true;
				_rnd.seed (utils::ns_since_epoch ());
			}
		
		uuid_t uuid;
		std::lock_guard<std::mutex> guard {_rnd_mut};
		
		for (int i = 0; i < 4; ++i)	
			{
				uuid.data[i] = 0;
				for (int j = 0; j < 8; ++j)
					uuid.data[i] |= (_dis (_rnd) << (j * 4));
			}
		
		uuid.data[1] &= ~(0xF << 12);
		uuid.data[1] |= 0x4 << 12;
		
		uuid.data[2] &= ~(0xF << 28);
		uuid.data[2] |= 0xA << 28;
		
		return uuid;
	}
	
	
	
	std::string
	uuid_t::to_str ()
	{
		std::string str;
		
		static const char *hex_digits = "0123456789abcdef";
		for (int i = 0; i < 4; ++i)	
			for (int j = 7; j >= 0; --j)
				str.push_back (hex_digits[(this->data[i] >> (j * 4)) & 0xF]);
		
		str.insert (8, 1, '-');
		str.insert (13, 1, '-');
		str.insert (18, 1, '-');
		str.insert (23, 1, '-');
		
		return str;
	}
	
	
	
	bool
	uuid_t::operator== (uuid_t other) const
	{
		for (int i = 0; i < 4; ++i)
			if (other.data[i] != this->data[i])
				return false;
		return true;
	}
	
	bool
	uuid_t::operator!= (uuid_t other) const
	{
		return !this->operator== (other);
	}
}

