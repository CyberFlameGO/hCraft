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

#ifndef _hCraft__UTILS_H_
#define _hCraft__UTILS_H_

#include <string>
#include <ctime>


namespace hCraft {
	
	/* 
	 * Utility functions and classes.
	 */
	namespace utils {
		
		inline int
		iabs (int x)
		{
			return (x < 0) ? -x : x;
		}
		
		inline double
		abs (double x)
		{
			return (x < 0.0) ? -x : x;
		}
		
		
		inline int
		div (int x, int y)
		{
			if (-13 / 5 == -2 && (x < 0) != (y < 0) && x % y != 0)
	  		return x / y - 1;
			return x / y;
		}
		
		inline int
		mod (int x, int y)
		{
			if (-13 / 5 == -2 && (x < 0) != (y < 0) && x % y != 0)
	  		return x % y + y;
			return x % y;
		}
		
		
		inline int
		zsgn (int x)
		{
			return (x < 0) ? (-1) : ((x > 0) ? 1 : 0);
		}
		
		
		
		inline int
		min (int a, int b)
			{ return (a < b) ? a : b; }
		
		inline int
		max (int a, int b)
			{ return (a > b) ? a : b; }
		
		
		inline int
		floor (double x)
			{ return (x >= 0.0) ? (int)x : ((int)x - 1); }
		
		
		// integer rotation
		inline int
		int_rot (float v)
		{
			v = v - (((int)v / 360) * 360);
			return v * 0.71111111111;
		}
		
		
		
		inline void
		write_short (unsigned char *data, unsigned short num)
		{
			data[0] = num & 0xFF;
			data[1] = (num >> 8) & 0xFF;
		}
		
		inline void
		write_int (unsigned char *data, unsigned int num)
		{
			data[0] = num & 0xFF;
			data[1] = (num >> 8) & 0xFF;
			data[2] = (num >> 16) & 0xFF;
			data[3] = (num >> 24) & 0xFF;
		}
		
		inline void
		write_long (unsigned char *data, unsigned long long num)
		{
			data[0] = num & 0xFF;
			data[1] = (num >> 8) & 0xFF;
			data[2] = (num >> 16) & 0xFF;
			data[3] = (num >> 24) & 0xFF;
			data[4] = (num >> 32) & 0xFF;
			data[5] = (num >> 40) & 0xFF;
			data[6] = (num >> 48) & 0xFF;
			data[7] = (num >> 56) & 0xFF;
		}
		
		inline void
		write_double (unsigned char *data, double num)
			{ write_long (data, *((long long *)&num)); }
		
		
		inline unsigned int
		read_int (const unsigned char *data)
		{
			return (unsigned int)(data[0])
				| ((unsigned int)(data[1]) << 8)
				| ((unsigned int)(data[2]) << 16)
				| ((unsigned int)(data[3]) << 24);
		}
		
		static double
		read_double (const unsigned char *data)
		{
			unsigned long long v = ((unsigned long long)(data[0])
				| ((unsigned long long)(data[1]) << 8)
				| ((unsigned long long)(data[2]) << 16)
				| ((unsigned long long)(data[3]) << 24)
				| ((unsigned long long)(data[4]) << 32)
				| ((unsigned long long)(data[5]) << 40)
				| ((unsigned long long)(data[6]) << 48)
				| ((unsigned long long)(data[7]) << 56));
			return *((double *)&v);
		}
		
		
		
	//----
		
		/* 
		 * The amount of nanoseconds passed since epoch.
		 * Useful to initialize random number generators.
		 */
		unsigned long long ns_since_epoch ();
		
		/* 
		 * Difference between two time_ts in days.
		 */
		int day_diff (std::time_t a, std::time_t b);
		
		void relative_time (std::time_t a, std::time_t b, std::string& out);
		void relative_time_short (std::time_t a, std::time_t b, std::string& out);
		
		/* 
		 * Input examples:
		 *   30, 12s, 100s, 4m2s 12h 3d
		 * Returns -1 if invalid.
		 */
		int seconds_from_time_str (const std::string& str);
		
		
		
	//----
		
		/* 
		 * GZIP compression.
		 */
		long gz_compress (unsigned char *src, unsigned long slen,
			unsigned char *dest, int level = 9);
		unsigned char* gz_compress (unsigned char *src, unsigned long slen,
			long& dest_size, int level = 9);
			
		
	//----
		
		/* 
		 * Formats a number with commas and optionally decreases\increases the
		 * number of digits after the decimal point.
		 *   563946.4274  ->  563,946.43
		 */
		std::string format_number (double num, int trunc = 2);
	}
}

#endif

