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

#include "utils.hpp"
#include <chrono>
#include <zlib.h>
#include <cstring>
#include <cmath>
#include <sstream>
#include <limits>
#include <iomanip>
#include <locale>


namespace hCraft {
	namespace utils {
		
		/* 
		 * The amount of nanoseconds passed since epoch.
		 * Useful to initialize random number generators.
		 */
		unsigned long long
		ns_since_epoch ()
		{
			return std::chrono::duration_cast<std::chrono::nanoseconds> (
				std::chrono::high_resolution_clock::now ().time_since_epoch ()).count ()
				& 0x7FFFFFFF;
		}
		
		
		/* 
		 * Difference between two time_ts in days.
		 */
		int
		day_diff (std::time_t a, std::time_t b)
		{
			double d = (long long)a - (long long)b;
			return std::floor (d / 86400.0);
		}
		
		
		void
		relative_time (std::time_t a, std::time_t b, std::string& out)
		{
			std::ostringstream ss;
			int c = 0;
			
			int d = day_diff (a, b);
			if (d == 0)
				{
					double dd = std::difftime (a, b);
					
					if (dd >= 3600.0)
						{
							int h = dd / 3600.0;
							dd -= h * 3600;
							ss << h << " hour" << ((h == 1) ? "" : "s");
							++ c;
						}
					
					if (dd >= 60.0)
						{
							int m = dd / 60.0;
							dd -= m * 60;
							ss << ((c > 0) ? ", " : "") << m << " minute" << ((m == 1) ? "" : "s");
							++ c;
						}
					
					if (dd >= 1.0)
						{
							int s = dd;
							ss << ((c > 0) ? ", " : "") << s << " second" << ((s == 1) ? "" : "s");
							++ c;
						}
					else if (c == 0)
						{
							out = "Just now";
							return;
						}
					
					ss << " ago";
					out.assign (ss.str ());
					return;
				}
			else if (d == 1)
				{ out = "Yesterday"; return; }
			
			if (d >= 365)
				{
					int y = d / 365;
					d %= 365;
					ss << y << " year" << ((y == 1) ? "" : "s");
					++ c;
				}
			
			if (d >= 30)
				{
					int m = d / 30;
					d %= 30;
					ss << ((c > 0) ? ", " : "") << m << " month" << ((m == 1) ? "" : "s");
					++ c;
				}
			
			if (d >= 7)
				{
					int w = d / 7;
					d %= 7;
					ss << ((c > 0) ? ", " : "") << w << " week" << ((w == 1) ? "" : "s");
					++ c;
				}
			
			if (d > 0)
				{
					ss << ((c > 0) ? ", " : "") << d << " day" << ((d == 1) ? "" : "s");
					++ c;
				}
			
			ss << " ago";
			out.assign (ss.str ());
		}
		
		
		
	//-----
		
		/* 
		 * GZIP compression.
		 */
		long
		gz_compress (unsigned char *src, unsigned long slen,
			unsigned char *dest, int level)
		{
		#define CHUNK 16384
			
			int ret;
			unsigned int have;
			z_stream strm;
			unsigned char out[CHUNK];
			
			strm.zalloc = Z_NULL;
			strm.zfree = Z_NULL;
			strm.opaque = Z_NULL;
			ret = deflateInit2 (&strm, level, Z_DEFLATED, 31, 9, Z_DEFAULT_STRATEGY);
			if (ret != Z_OK)
				return -1;
			
			strm.avail_in = slen;
			strm.next_in = src;
			do
				{
					strm.avail_out = sizeof out;
					strm.next_out = out;
					ret = deflate (&strm, Z_FINISH);
					if (ret == Z_STREAM_ERROR)
						return -1;
					have = sizeof (out) - strm.avail_out;
					std::memcpy (dest, out, have);
					dest += have;
				}
			while (strm.avail_out == 0);
			
			deflateEnd (&strm);
			return strm.total_out;
		}
		
		unsigned char*
		gz_compress (unsigned char *src, unsigned long slen, long& dest_size, int level)
		{
		#define GZIP_HEADER_SIZE 256
			unsigned char *dest = new unsigned char [compressBound (slen)
				+ GZIP_HEADER_SIZE];
			if ((dest_size = gz_compress (src, slen, dest, level)) < 0)
				{
					delete[] dest;
					return nullptr;
				}	
			return dest;
		}
		
		
	//----
		
		namespace {
			class comma_numpunct : public std::numpunct<char>
			{
			protected:
				virtual char
				do_thousands_sep() const
					{ return ','; }

				virtual std::string
				do_grouping() const
					{ return "\03"; }
			};
		}
		
		/* 
		 * Formats a number with commas and optionally decreases\increases the
		 * number of digits after the decimal point.
		 *   563946.4274  ->  563,946.43
		 */
		std::string
		format_number (double num, int trunc)
		{
			std::ostringstream ss;
			std::locale comma_locale (std::locale (), new comma_numpunct ());
			ss.imbue (comma_locale);
			ss << std::fixed << std::setprecision (2) << std::showpoint << num;
			return ss.str ();
		}
	}
}

