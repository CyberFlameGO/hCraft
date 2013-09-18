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

#ifndef _hCraft__BLOCK_UNDO_H_
#define _hCraft__BLOCK_UNDO_H_

#include <vector>
#include <ctime>
#include <string>
#include <mutex>


namespace hCraft {
	
	struct block_undo_record
	{
		int x;
		int y;
		int z;
		
		unsigned short old_id;
		unsigned char old_meta;
		unsigned char old_extra;
		
		unsigned short new_id;
		unsigned char new_meta;
		unsigned char new_extra;
		
		std::time_t when;
	};
	
	
	
#define BU_PAGE_SIZE		 4096
	
	/* 
	 * An extremely simple block undo file format is implemented at the moment,
	 * that will change though in the future!
	 */
	class block_undo
	{
		void *strm;
		bool _open;
		std::string filename;
		
		std::vector<block_undo_record> buf;
		
		// reading:
		bool _next;
		long off;
		int page_count;
		unsigned char page[BU_PAGE_SIZE];
		struct { bool valid; std::time_t when; long off; } last_fetch;
		
	public:
		inline bool is_open () const { return this->_open; }
		
	public:
		block_undo (const std::string& filename);
		~block_undo ();
		
		
		void set_path (const std::string& filename);
		
		
		
		/* 
		 * Open the undo file located at the given path.
		 * Returns false on error.
		 */
		bool open ();
		
		/* 
		 * Closes the internal stream.
		 */
		void close ();
		
		
		
		/* 
		 * Inserts the specified block undo record into the internal buffer,
		 * flushing if necessary.
		 * 
		 * NOTE: This method does not require the structure to be open, for it
		 *       to work.
		 */
		void insert (block_undo_record rec);
		
		/* 
		 * Saves the internal record buffer onto disk.
		 */
		void flush ();
		
		
		
		/* 
		 * Prepares to read block undo records from disk.
		 */
		void fetch (std::time_t when);
		
		/* 
		 * Returns the next block modification record.
		 * NOTE: fetch() should be called before any consequent next_record ()
		 *       invocations.
		 */
		block_undo_record next_record ();
		
		/* 
		 * Checks whether a call to next_record() would return a valid record.
		 */
		bool has_next ();
		
		
		
		/* 
		 * Removes all block modification records created after the specified
		 * point in time.
		 */
		void remove (std::time_t when);
	};
}

#endif

