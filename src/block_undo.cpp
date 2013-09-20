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

#include "block_undo.hpp"
#include <cstdio>
#include <cstring>
#include <cstdio>
#include <unistd.h>

#include <iostream> // DEBUG


namespace hCraft {
	
#define BU_RECORD_SIZE		 20
#define BU_MAX_BUFFER		 1024
#define BU_SIX_HOURS		21600 // in seconds
	
	
	block_undo::block_undo (const std::string& filename)
		: filename (filename)
	{
		this->_open = false;
		this->page_count = 0;
		this->_next = false;
	}
	
	block_undo::~block_undo ()
	{
		this->close ();
	}
	
	
	
	void
	block_undo::set_path (const std::string& filename)
	{
		this->filename = filename;
	}
	
	
	
	static void
	_write_short (unsigned char *d, unsigned short n)
	{
		d[0] = n & 0xFF;
		d[1] = (n >> 8) & 0xFF;
	}
	
	static void
	_write_int (unsigned char *d, unsigned int n)
	{
		d[0] = n & 0xFF;
		d[1] = (n >> 8) & 0xFF;
		d[2] = (n >> 16) & 0xFF;
		d[3] = (n >> 24) & 0xFF;
	}
	
	static void
	_write_long (unsigned char *d, unsigned long long n)
	{
		d[0] = n & 0xFF;
		d[1] = (n >> 8) & 0xFF;
		d[2] = (n >> 16) & 0xFF;
		d[3] = (n >> 24) & 0xFF;
		d[4] = (n >> 32) & 0xFF;
		d[5] = (n >> 40) & 0xFF;
		d[6] = (n >> 48) & 0xFF;
		d[7] = (n >> 56) & 0xFF;
	}
	
	
	static unsigned short
	_read_short (const unsigned char *d)
	{
		return ((unsigned short)d[0])
				 | ((unsigned short)d[1] << 8);
	}
	
	static unsigned int
	_read_int (const unsigned char *d)
	{
		return ((unsigned int)d[0])
				 | ((unsigned int)d[1] << 8)
				 | ((unsigned int)d[2] << 16)
				 | ((unsigned int)d[3] << 24);
	}
	
	static unsigned long long
	_read_long (const unsigned char *d)
	{
		return ((unsigned long long)d[0])
				 | ((unsigned long long)d[1] << 8)
				 | ((unsigned long long)d[2] << 16)
				 | ((unsigned long long)d[3] << 24)
				 | ((unsigned long long)d[4] << 32)
				 | ((unsigned long long)d[5] << 40)
				 | ((unsigned long long)d[6] << 48)
				 | ((unsigned long long)d[7] << 56);
	}
	
	
	static void
	_write_zeroes (FILE *f, unsigned int n)
	{
		static const unsigned char d[4096] = { 0 };
		while (n > 4096)
			{ std::fwrite (d, 1, 4096, f); n -= 4096; }
		std::fwrite (d, 1, n, f);
	}
	
	
	
	static FILE*
	_load_file (const char *filename)
	{
		FILE *f = std::fopen (filename, "r+b");
		if (!f)
			{
				f = std::fopen (filename, "w+b");
				if (!f)
					return nullptr;
				
				// create empty page
				_write_zeroes (f, BU_PAGE_SIZE);
			}
		
		return f;
	}
	
	/* 
	 * Open the undo file located at the given path.
	 * Returns false on error.
	 */
	bool
	block_undo::open ()
	{
		if (this->_open)
			return false;
		
		this->strm = _load_file (this->filename.c_str ());
		if (!this->strm)
			return false;
		
		this->_open = true;
		return true;
	}
	
	/* 
	 * Closes the internal stream.
	 */
	void
	block_undo::close ()
	{
		if (!this->_open)
			{
				if (!this->buf.empty ())
					this->flush ();
				return;
			}
		
		this->_next = false;
		this->flush ();
		std::fclose ((FILE *)this->strm);
		this->_open = false;
	}
	
	
	
	/* 
	 * Inserts the specified block undo record into the internal buffer,
	 * flushing if necessary.
	 * 
	 * NOTE: This method does not require the structure to be open, for it
	 *       to work.
	 */
	void
	block_undo::insert (block_undo_record rec)
	{
		this->buf.push_back (rec);
		if (this->buf.size () >= BU_MAX_BUFFER)
			this->flush ();
	}
	
	
	// returns vector offset
	static int
	_allocate_page (FILE *strm, std::vector<block_undo_record>& recs, int start)
	{
		unsigned long long tm = (unsigned long long)std::time (nullptr);
		unsigned long long page_time = tm / BU_SIX_HOURS;
		
		if ((unsigned long long)recs[start].when / BU_SIX_HOURS > page_time)
			return start;
		
		std::fseek (strm, 0, SEEK_END);
		
		unsigned char buf[BU_PAGE_SIZE];
		_write_long (buf, page_time);
		_write_int (buf + 12, 0);			// padding
		
		unsigned int max_recs = (BU_PAGE_SIZE - 16) / BU_RECORD_SIZE;
		int n = 16;
		size_t i = start;
		for (; i < recs.size () && (i - start) < max_recs; ++i)
			{
				block_undo_record rec = recs[i];
				if ((unsigned long long)recs[i].when / BU_SIX_HOURS > page_time)
					break;
				
				_write_int (buf + n + 0, rec.x);
				buf[n + 4] = rec.y;
				_write_int (buf + n + 5, rec.z);
				_write_short (buf + n + 9, rec.old_id);
				buf[n + 11] = rec.old_meta;
				buf[n + 12] =  rec.old_extra;
				_write_short (buf + n + 13, rec.new_id);
				buf[n + 15] = rec.new_meta;
				buf[n + 16] = rec.new_extra;
				_write_short (buf + n + 17, (unsigned long long)recs[i].when % BU_SIX_HOURS);
				n += BU_RECORD_SIZE;
			}
		
		// pad the rest
		std::memset (buf + n, 0x00, BU_PAGE_SIZE - n);
		
		// record count
		_write_int (buf + 8, i - start);
		
		std::fwrite (buf, 1, BU_PAGE_SIZE, strm);
		return i;
	}
	
	static int
	_write_to_page (FILE *strm, std::vector<block_undo_record>& recs, int start)
	{
		unsigned char buf[BU_PAGE_SIZE];
		auto off = std::ftell (strm);
		if (off < BU_PAGE_SIZE || (recs.size () - start) == 0)
			return start;
		
		std::fread (buf, 1, BU_PAGE_SIZE, strm);
		
		unsigned long long page_time = _read_long (buf);
		int rec_count = _read_int (buf + 8);
		
		if ((unsigned long long)recs[start].when / BU_SIX_HOURS > page_time)
			return start;
		
		unsigned int max_recs = (BU_PAGE_SIZE - 16) / BU_RECORD_SIZE - rec_count;
		int n = 16 + (rec_count * BU_RECORD_SIZE);
		size_t i = start;
		for (; i < recs.size () && (i - start) < max_recs; ++i)
			{
				block_undo_record rec = recs[i];
				if ((unsigned long long)recs[i].when / BU_SIX_HOURS > page_time)
					break;
				
				_write_int (buf + n + 0, rec.x);
				buf[n + 4] = rec.y;
				_write_int (buf + n + 5, rec.z);
				_write_short (buf + n + 9, rec.old_id);
				buf[n + 11] = rec.old_meta;
				buf[n + 12] =  rec.old_extra;
				_write_short (buf + n + 13, rec.new_id);
				buf[n + 15] = rec.new_meta;
				buf[n + 16] = rec.new_extra;
				_write_short (buf + n + 17, (unsigned long long)recs[i].when % BU_SIX_HOURS);
				n += BU_RECORD_SIZE;
			}
		
		// record count
		_write_int (buf + 8, rec_count + (i - start));
		
		std::fseek (strm, off, SEEK_SET);
		std::fwrite (buf, 1, BU_PAGE_SIZE, strm);
		return i;
	}
	
	
	/* 
	 * Saves the internal record buffer to disk.
	 */
	void
	block_undo::flush ()
	{
		bool was_open = this->_open;
		if (!this->_open)
			if (!this->open ())
				return;
		
		FILE *f = (FILE *)this->strm;
		
		std::fseek (f, -BU_PAGE_SIZE, SEEK_END);
		int i = _write_to_page (f, this->buf, 0);
		while (i < (int)this->buf.size ())
			{
				i = _allocate_page (f, this->buf, i);
			}
		
		this->buf.clear ();
		std::fflush (f);
		
		if (!was_open)
			this->close ();
	}
	
	
	
	/* 
	 * Prepares to read block undo records from disk.
	 */
	void
	block_undo::fetch (std::time_t when)
	{
		if (!this->_open)
			return;
		if (!this->buf.empty ())
			this->flush ();
		
		this->_next = false;
		FILE *f = (FILE *)this->strm;
		std::fseek (f, 0, SEEK_END);
		unsigned int fsize = std::ftell (f);
		
		this->page_count = (fsize / BU_PAGE_SIZE) - 1; // excluding initial reserved page
		if (this->page_count == 0)
			return;
		
		std::fseek (f, -BU_PAGE_SIZE, SEEK_CUR); // go one page backward
		
		// find last record in this page
		std::fread (this->page, 1, BU_PAGE_SIZE, f);
		
		unsigned long long page_time = _read_long (this->page);
		int rec_count = _read_int (this->page + 8);
		this->off = fsize - BU_PAGE_SIZE + 16 + ((rec_count - 1) * BU_RECORD_SIZE);
		
		this->_next = (page_time * BU_SIX_HOURS + _read_short (this->page + (this->off % BU_PAGE_SIZE) + 17))
			>= (unsigned long long)when;
		this->tm = when;
	}
	
	/* 
	 * Returns the next block modification record.
	 * NOTE: fetch() should be called before any consequent next_record ()
	 *       invocations.
	 */
	block_undo_record
	block_undo::next_record ()
	{
		if (!this->_open || this->page_count == 0)
			return {};
		
		unsigned long long page_time = _read_long (this->page);
		int loc_off = this->off % BU_PAGE_SIZE;
		
		block_undo_record rec;
		rec.x = _read_int (this->page + loc_off + 0);
		rec.y = this->page[loc_off + 4];
		rec.z = _read_int (this->page + loc_off + 5);
		rec.old_id = _read_short (this->page + loc_off + 9);
		rec.old_meta = this->page[loc_off + 11];
		rec.old_extra = this->page[loc_off + 12];
		rec.new_id = _read_short (this->page + loc_off + 13);
		rec.new_meta = this->page[loc_off + 15];
		rec.new_extra = this->page[loc_off + 16];
		rec.when = (std::time_t)(page_time * BU_SIX_HOURS + _read_short (this->page + loc_off + 17));
		
		if (this->off % BU_PAGE_SIZE == 16)
			{
				// fetch next page
				int page_ind = this->off / BU_PAGE_SIZE;
				if (page_ind == 1)
					{
						this->_next = false;
					}
				else
					{
						this->off = ((this->off / BU_PAGE_SIZE) - 1) * BU_PAGE_SIZE;
						FILE *f = (FILE *)this->strm;
						std::fseek (f, this->off, SEEK_SET);
						std::fread (this->page, 1, BU_PAGE_SIZE, f);
				
						page_time = _read_long (this->page);
						int rec_count = _read_int (this->page + 8);
						this->off += 16 + ((rec_count - 1) * BU_RECORD_SIZE);
						this->_next = (page_time * BU_SIX_HOURS + _read_short (this->page + (this->off % BU_PAGE_SIZE) + 17))
							>= (unsigned long long)this->tm;
					}
			}
		else
			{
				this->off -= BU_RECORD_SIZE;
				this->_next = (rec.when >= this->tm);
			}
		
		return rec;
	}
	
	/* 
	 * Checks whether a call to next_record() would return a valid record.
	 */
	bool
	block_undo::has_next ()
	{
		if (!this->_open)
			return false;
		return this->_next;
	}
	
	
	
	/* 
	 * Removes all block modification records created after the specified
	 * point in time.
	 */
	void 
	block_undo::remove (std::time_t when)
	{
		if (!this->_open)
			return;
		if (!this->buf.empty ())
			this->flush ();

		FILE *f = (FILE *)this->strm;
				
		{
			// position self
			// skip initial page
			std::fseek (f, BU_PAGE_SIZE, SEEK_SET);
			this->off = BU_PAGE_SIZE;
	
			unsigned long long when_reduced = (unsigned long long)when / BU_SIX_HOURS;
			for (int p = 0; p < this->page_count; ++p)
				{
					std::fread (this->page, 1, BU_PAGE_SIZE, f);
			
					unsigned long long tm = _read_long (this->page);
					if (tm > when_reduced)
						{
							if ((this->off / BU_PAGE_SIZE) > 1)
								{
									// go back one page
									this->off -= BU_PAGE_SIZE;
									std::fseek (f, this->off, SEEK_SET);
									std::fread (this->page, 1, BU_PAGE_SIZE, f);
									if (this->off == 0)
										{
											this->page_count = 0; // nothing in here
										}
								}
					
							break;
						}
					else if (tm == when_reduced)
						break;
				}
	
			int rec_count = _read_int (this->page + 8);
			unsigned long long page_time = _read_long (this->page);
			unsigned short when_mod = (unsigned long long)when % BU_SIX_HOURS;
			this->off += 16; // position self at first record in page
			for (int i = 0; ; ++i)
				{
					if (i >= rec_count)
						{
							// next page
					
							if (this->off % BU_PAGE_SIZE != 0)
								this->off += BU_PAGE_SIZE - (this->off % BU_PAGE_SIZE);
							int page_ind = this->off / BU_PAGE_SIZE - 1;
							if (page_ind >= this->page_count)
								break;
						
							std::fread (this->page, 1, BU_PAGE_SIZE, f);
							this->off += 16;
							i = 0;
						}
			
					if ((page_time * BU_SIX_HOURS + _read_short (this->page + (this->off % BU_PAGE_SIZE) + 17))
						>= (when_reduced * BU_SIX_HOURS + when_mod))
						{
							break;
						}
			
					this->off += BU_RECORD_SIZE;
				}
	
			this->_next = (this->off % BU_PAGE_SIZE != 0);
		}
		
		if (!this->_next)
			return;
		
		unsigned int data_start = this->off - this->off % BU_PAGE_SIZE;
		
		int rec_count = _read_int (this->page + 8);
		this->_next = !(((this->off % BU_PAGE_SIZE) - 16) / BU_RECORD_SIZE >= rec_count);
		if (!this->has_next ())
			return;
		
		int new_recs = ((this->off - 16) % BU_PAGE_SIZE) / BU_RECORD_SIZE;
		int new_size = data_start;
		if (new_recs > 0)
			{
				new_size += BU_PAGE_SIZE;
				_write_int (this->page + 8, new_recs);
				std::fseek (f, data_start, SEEK_SET);
				std::fwrite (this->page, 1, BU_PAGE_SIZE, f);
			}
		
		std::fflush (f);
		ftruncate (fileno (f), new_size);
	}
}

