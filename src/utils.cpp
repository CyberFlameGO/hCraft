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
	}
}

