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

#include "providers/hwprovider.hpp"
#include "world_security.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "position.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cctype>
#include <zlib.h>
#include <stdexcept>
#include <iostream>


namespace hCraft {

	#define HW_SUPERBLOCK_TABLE_OFFSET		 1024	
		
	#define HW_LAYER_SIZE										 40
	#define HW_LAYER_TABLE_OFFSET					50176
	#define HW_LAYER_PAGE_SIZE						 1024
	#define HW_LAYER_PAGE_DATA_SIZE  			 1020
	
	#define HW_CURR_REV												2
	
	
	inline int
	fast_floor (double x)
		{ return (x >= 0.0) ? (int)x : ((int)x - 1); }
	
	
	
	static unsigned int
	jenkins_hash (const unsigned char *data, unsigned int len)
	{
		unsigned int hash, i;
		for (hash = i = 0; i < len; ++i)
			{
				hash += data[i];
				hash += (hash << 10);
				hash ^= (hash >> 6);
			}
		
		hash += (hash << 3);
		hash ^= (hash >> 11);
		hash += (hash << 15);
		
		return hash;
	}
	
	static unsigned int
	hash_coords (int x, int z)
	{
		unsigned char buf[8];
		
		buf[0] = (x) & 0xFF;
		buf[1] = (x >> 8) & 0xFF;
		buf[2] = (x >> 16) & 0xFF;
		buf[3] = (x >> 24) & 0xFF;
		buf[4] = (z) & 0xFF;
		buf[5] = (z >> 8) & 0xFF;
		buf[6] = (z >> 16) & 0xFF;
		buf[7] = (z >> 24) & 0xFF;
		
		return jenkins_hash (buf, 8);
	}
	
	
	
//----
	
	class binary_reader
	{
		std::istream& strm;
		
	public:
		binary_reader (std::istream& strm)
			: strm (strm)
			{ }
		
		//---
		inline void
		seek (std::ostream::off_type off, std::ios_base::seekdir dir = std::ios_base::beg)
			{
				this->strm.seekg (off, dir);
			}
		
		inline std::ostream::pos_type
		tell ()
			{ return this->strm.tellg (); }
		
		//---
		inline unsigned char
		read_byte ()
			{ return strm.get (); }
		
		inline unsigned short
		read_short ()
			{ return ((unsigned short)read_byte ())
						 | ((unsigned short)read_byte () << 8); }
		
		inline unsigned int
		read_int ()
			{ return ((unsigned int)read_byte ())
						 | ((unsigned int)read_byte () << 8)
						 | ((unsigned int)read_byte () << 16)
						 | ((unsigned int)read_byte () << 24); }
		
		inline unsigned long long
		read_long ()
			{ return ((unsigned long long)read_byte ())
						 | ((unsigned long long)read_byte () << 8)
						 | ((unsigned long long)read_byte () << 16)
						 | ((unsigned long long)read_byte () << 24)
						 | ((unsigned long long)read_byte () << 32)
						 | ((unsigned long long)read_byte () << 40)
						 | ((unsigned long long)read_byte () << 48)
						 | ((unsigned long long)read_byte () << 56); }
		
		inline float
		read_float ()
			{ unsigned int num = read_int (); return *((float *)&num); }
		
		inline double
		read_double ()
			{ unsigned long long num = read_long (); return *((double *)&num); }
		
		inline std::string
		read_string ()
		{
			short len = this->read_short ();
			std::string str;
			str.reserve (len + 1);
			
			for (int i = 0; i < len; ++i)
				{
					str.push_back (this->read_byte ());
				}
			
			return str;
		}
		
		inline std::string
		read_fixed_string (int max_len)
		{
			std::string str;
			str.reserve (max_len);
			
			int i, c;
			for (i = 0; i < max_len; ++i)
				{
					c = this->read_byte ();
					if (c == 0) break;
					str.push_back (c);
				}
			
			// skip the rest
			for (; i < max_len; ++i)
				this->read_byte ();
			
			return str;
		}
		
		inline void
		read_bytes (unsigned char *data, unsigned int len)
		{
			this->strm.read ((char *)data, len);
		}
	};
	
	
	
//----
	
	class binary_writer
	{
		std::ostream& strm;
		int written;
		
	public:
		binary_writer () : strm (std::cout) { }
		binary_writer (std::ostream& strm)
			: strm (strm), written (0)
			{ }
		
		//---
		inline void
		seek (std::ostream::off_type off, std::ios_base::seekdir dir = std::ios_base::beg)
			{
				this->strm.seekp (off, dir);
				if (dir == std::ios_base::end)
					this->written = this->tell ();
			}
		
		inline std::ostream::pos_type
		tell ()
			{ return this->strm.tellp (); }
		
		inline void
		flush ()
			{ this->strm.flush (); }
		
		//---
		inline void
		write_byte (unsigned char val)
			{ this->strm.put (val); ++ written; }
		
		inline void
		write_short (unsigned short val)
			{ write_byte (val & 0xFF);
				write_byte ((val >> 8) & 0xFF); }
		
		inline void
		write_int (unsigned int val)
			{ write_byte (val & 0xFF);
				write_byte ((val >> 8) & 0xFF);
				write_byte ((val >> 16) & 0xFF);
				write_byte ((val >> 24) & 0xFF); }
		
		inline void
		write_long (unsigned long long val)
			{ write_byte (val & 0xFF);
				write_byte ((val >> 8) & 0xFF);
				write_byte ((val >> 16) & 0xFF);
				write_byte ((val >> 24) & 0xFF);
				write_byte ((val >> 32) & 0xFF);
				write_byte ((val >> 40) & 0xFF);
				write_byte ((val >> 48) & 0xFF);
				write_byte ((val >> 56) & 0xFF);}
		
		inline void
		write_float (float val)
			{ write_int (*(unsigned int *)&val); }
		
		inline void
		write_double (double val)
			{ write_long (*(unsigned long long *)&val); }
		
		inline void
		write_string (const char *str)
			{
				int len = std::strlen (str);
				write_short (std::strlen (str));
				for (int i = 0; i < len; ++i)
					write_byte (str[i]);
			}
		
		inline void
		write_fixed_string (const char *str, int max_len)
		{
			int len = std::strlen (str);
			if (len > max_len) len = max_len;
			for (int i = 0; i < len; ++i)
				write_byte (str[i]);
			for (int i = 0; i < (max_len - len); ++i)
				write_byte (0);
		}
		
		inline void
		write_bytes (const unsigned char *data, unsigned int len)
		{
			this->strm.write ((const char *)data, len);
			this->written += len;
		}
			
	//----
		inline int
		pad_to (int mul)
		{
			if (this->written % mul == 0)
				return 0;
			
			int next = (this->written / mul + 1) * mul;
			int need = next - this->written;
			for (next = 0; next < need; ++next)
				this->write_byte (0);
			return need;
		}
	};
	
	
	
//----
		
	static void read_file (world_information&, hw_superblock **, binary_reader); // forward def
	
	/* 
	 * Constructs a new world provider for the HWv1 format.
	 */
	hw_provider::hw_provider (const char *path, const char *world_name)
		: out_path (path), inf ()
	{
		if (this->out_path[this->out_path.size () - 1] != '/')
			this->out_path.push_back ('/');
		this->out_path.append (hw_provider_naming ().make_name (world_name));
		
		for (int i = 0; i < 4096; ++i)
			this->sblocks[i] = nullptr;
		
		// read tables if the world file already exists
		{
			std::fstream strm (this->out_path, std::ios_base::in
				| std::ios_base::binary);
			if (strm.is_open ())
				{
					binary_reader reader {strm};
					read_file (this->inf, this->sblocks, reader);
					this->read_layer_table (strm);
					strm.close ();
				}
		}
	}
	
	/* 
	 * Class destructor.
	 */
	hw_provider::~hw_provider ()
	{
		for (int i = 0; i < 4096; ++i)
			delete this->sblocks[i];
		for (hw_layer& ly : this->layers)
			delete[] ly.offsets;
		this->close ();
	}
	
	
	
	/* 
	 * Returns the filesystem path to the world file.
	 */
	const char*
	hw_provider::get_path ()
	{
		return this->out_path.c_str ();
	}
	
	
	
	/* 
	 * Opens the underlying file stream for reading\writing.
	 * By using open () and close (), multiple chunks can be read\written
	 * without reopening the world file everytime.
	 */
 	void
 	hw_provider::open (world &wr)
 	{
 		if (this->strm.is_open ())
 			return;
 		
 		this->strm.open (this->out_path, std::ios_base::binary | std::ios_base::in
 			| std::ios_base::out);
 		if (!this->strm)
 			{
 				this->save_empty (wr);
 				this->strm.open (this->out_path, std::ios_base::binary | std::ios_base::in
 					| std::ios_base::out);
 			}
 	}
	
	/* 
	 * Closes the underlying file stream.
	 */
	void
	hw_provider::close ()
	{
		if (this->strm.is_open ())
			{
				this->strm.flush ();
				this->strm.close ();
			}
	}
	
	
	
	/* 
	 * Adds required prefixes, suffixes, etc... to the specified world name so
	 * that the importer's claims_name () function returns true when passed to
	 * it.
	 */
	std::string
	hw_provider_naming::make_name (const char *world_name)
	{
		std::string out;
		out.reserve (std::strlen (world_name) + 4);
		
		int c;
		while ((c = (int)(*world_name++)))
			{
				if (c == ' ')
					out.push_back ('_');
				else
					out.push_back (std::tolower (c));
			}
		
		out.append (".hw"); // extension
		
		return out;
	}
	
	/* 
	 * Checks whether the specified path name meets the format required by this
	 * exporter (could be a name prefix, suffix, extension, etc...).
	 */
	bool
	hw_provider_naming::claims_name (const char *path)
	{
		int len = std::strlen (path);
		return ((len > 4) && ((path[len - 3] == '.') && (path[len - 2] == 'h')
			&& (path[len - 1] == 'w')));
	}
	
	/* 
	 * Opens the file located at path @{path} and performs a check to see if it
	 * is of the same format created by this exporter.
	 */
	bool
	hw_provider::claims (const char *path)
	{
		std::ifstream strm (path);
		if (!strm)
			return false;
		
		binary_reader reader (strm);
		bool cl = (reader.read_int () == 0x31765748);
		
		strm.close ();
		return cl;
	}
	
	
	
//----
	
	static hw_superblock*
	find_or_create_superblock (int x, int z, hw_superblock **sblocks,
		binary_writer writer, bool create = true)
	{
		unsigned int hash = hash_coords (x, z);
		unsigned int hash_m = hash & 0xFFF;
		
		hw_superblock *sblock = sblocks[hash_m];
		if (sblock != nullptr)
			{
				if (sblock->x == x && sblock->z == z)
					return sblock;
				
				// linear probe
				int i;
				for (i = 0; i < 4096; ++i)
					{
						int m = (hash_m + i) & 0xFFF;
						sblock = sblocks[m];
						if (sblock == nullptr || (sblock->x == x && sblock->z == z))
							break;
					}
				if (i == 4096)
					return nullptr;
				hash_m = (hash_m + i) & 0xFFF;
			}
		if (sblock) return sblock;
		
		if (create)
			{
				sblocks[hash_m] = new hw_superblock (x, z);
				sblock = sblocks[hash_m];
		
				// create the superblock
				writer.seek (0, std::ios_base::end);
				sblock->offset = writer.tell () / 512;
				for (int i = 0; i < 64; ++i)
					{
						writer.write_int (0); // x
						writer.write_int (0); // z
						writer.write_int (0xFFFFFFFFU); // offset
					}
				writer.pad_to (512);
		
				// update file
				writer.seek (HW_SUPERBLOCK_TABLE_OFFSET + (12 * hash_m));
				writer.write_int (x);
				writer.write_int (z);
				writer.write_int (sblock->offset);
			}
		
		return sblock;
	}
	
	static hw_block*
	find_or_create_block (int x, int z, hw_superblock **sblocks,
		binary_writer writer, bool create = true)
	{
		hw_superblock *sblock = find_or_create_superblock (
			fast_floor (x / 8.0), fast_floor (z / 8.0), sblocks,writer, create);
		if (!sblock) return nullptr;
		
		unsigned int hash = hash_coords (x, z);
		unsigned int hash_m = hash & 0x3F;
		
		hw_block *block = sblock->blocks[hash_m];
		if (block != nullptr)
			{
				if (block->x == x && block->z == z)
					return block;
				
				// linear probe
				int i;
				for (i = 0; i < 64; ++i)
					{
						int m = (hash_m + i) & 0x3F;
						block = sblock->blocks[m];
						if (block == nullptr || (block->x == x && block->z == z))
							break;
					}
				if (i == 64)
					return nullptr;
				hash_m = (hash_m + i) & 0x3F;
			}
		if (block) return block;
		
		if (create)
			{
				sblock->blocks[hash_m] = new hw_block (x, z);
				block = sblock->blocks[hash_m];
		
				// create the block
				writer.seek (0, std::ios_base::end);
				block->offset = writer.tell () / 512;
				for (int i = 0; i < 1024; ++i)
					{
						writer.write_int (0); // x
						writer.write_int (0); // z
						writer.write_int (0xFFFFFFFFU); // offset
					}
				writer.pad_to (512);
		
				// update file
				writer.seek ((sblock->offset * 512) + (12 * hash_m));
				writer.write_int (x);
				writer.write_int (z);
				writer.write_int (block->offset);
			}
		
		return block;
	}
	
	static hw_region*
	find_or_create_region (int x, int z, hw_superblock **sblocks,
		binary_writer writer, bool create = true)
	{
		hw_block *block = find_or_create_block (
			fast_floor (x / 32.0), fast_floor (z / 32.0), sblocks, writer, create);
		if (!block) return nullptr;
		
		unsigned int hash = hash_coords (x, z);
		unsigned int hash_m = hash & 0x3FF;
		
		hw_region *region = block->regions[hash_m];
		if (region != nullptr)
			{
				if (region->x == x && region->z == z)
					return region;
				
				// linear probe
				int i;
				for (i = 0; i < 1024; ++i)
					{
						int m = (hash_m + i) & 0x3FF;
						region = block->regions[m];
						if (region == nullptr || (region->x == x && region->z == z))
							break;
					}
				if (i == 1024)
					return nullptr;
				hash_m = (hash_m + i) & 0x3FF;
			}
		if (region) return region;
		
		if (create)
			{
				block->regions[hash_m] = new hw_region (x, z);
				region = block->regions[hash_m];
		
				// create the region
				writer.seek (0, std::ios_base::end);
				region->offset = writer.tell () / 512;
				for (int i = 0; i < 1024; ++i)
					{
						writer.write_int (0); // x
						writer.write_int (0); // z
						writer.write_int (0xFFFFFFFFU); // offset
					}
				writer.pad_to (512);
		
				// update file
				writer.seek ((block->offset * 512) + (12 * hash_m));
				writer.write_int (x);
				writer.write_int (z);
				writer.write_int (region->offset);
			}
		
		return region;
	}
	
	static hw_chunk*
	find_or_create_chunk (int x, int z, hw_superblock **sblocks,
		binary_writer writer, bool create = true, bool* got_created = nullptr)
	{
		if (got_created) *got_created = false;
		hw_region *region = find_or_create_region (
			fast_floor (x / 32.0), fast_floor (z / 32.0), sblocks, writer, create);
		if (!region) return nullptr;
		
		unsigned int hash = hash_coords (x, z);
		unsigned int hash_m = hash & 0x3FF;
		
		hw_chunk *ch = region->chunks[hash_m];
		if (ch != nullptr)
			{
				if (ch->x == x && ch->z == z)
					return ch;
				
				// linear probe
				int i;
				for (i = 0; i < 1024; ++i)
					{
						int m = (hash_m + i) & 0x3FF;
						ch = region->chunks[m];
						if (ch == nullptr || (ch->x == x && ch->z == z))
							break;
					}
				if (i == 1024)
					return nullptr;
				hash_m = (hash_m + i) & 0x3FF;
			}
		if (ch) return ch;
		
		if (create)
			{
				if (got_created) *got_created = true;
				region->chunks[hash_m] = new hw_chunk (x, z);
				ch = region->chunks[hash_m];
		
				// create the chunk
				writer.seek (0, std::ios_base::end);
				ch->offset = writer.tell () / 512;
				
				writer.write_int (ch->size);
				for (int j = 0; j < 256; ++j)
					writer.write_int (ch->sector_table[j]);
					
				writer.pad_to (512);
		
				// update file
				writer.seek ((region->offset * 512) + (12 * hash_m));
				writer.write_int (x);
				writer.write_int (z);
				writer.write_int (ch->offset);
			}
		
		return ch;
	}
	
	
	
//----
	
	static int
	_write_short (unsigned char *data, unsigned short val)
	{
		data[0] = val & 0xFF;
		data[1] = (val >> 8) & 0xFF;
		return 2;
	}
	
	static int
	_write_int (unsigned char *data, unsigned int val)
	{
		data[0] = val & 0xFF;
		data[1] = (val >> 8) & 0xFF;
		data[2] = (val >> 16) & 0xFF;
		data[3] = (val >> 24) & 0xFF;
		return 4;
	}
	
	static int
	_write_float (unsigned char *data, float val)
	{
		unsigned int v = *((unsigned int *)&val);
		data[0] = v & 0xFF;
		data[1] = (v >> 8) & 0xFF;
		data[2] = (v >> 16) & 0xFF;
		data[3] = (v >> 24) & 0xFF;
		return 4;
	}
	
	static int
	_write_double (unsigned char *data, double val)
	{
		unsigned long long v = *((unsigned long long *)&val);
		data[0] = v & 0xFF;
		data[1] = (v >> 8) & 0xFF;
		data[2] = (v >> 16) & 0xFF;
		data[3] = (v >> 24) & 0xFF;
		data[4] = (v >> 32) & 0xFF;
		data[5] = (v >> 40) & 0xFF;
		data[6] = (v >> 48) & 0xFF;
		data[7] = (v >> 56) & 0xFF;
		return 8;
	}
	
	static int
	_write_string (unsigned char *data, const std::string& str)
	{
		data[0] =  str.size () & 0xFF;
		data[1] = (str.size () >> 8) & 0xFF;
		int n = 2;
		
		for (char c : str)
			data[n++] = c;
		
		return n;
	}
	
	
	
	static unsigned short
	_read_short (const unsigned char *data, unsigned int& pos)
	{
		pos += 2;
		return (unsigned short)(data[0])
				| ((unsigned short)(data[1]) << 8);
	}
	
	static int
	_read_int (const unsigned char *data, unsigned int& pos)
	{
		pos += 4;
		return (unsigned int)(data[0])
				| ((unsigned int)(data[1]) << 8)
				| ((unsigned int)(data[2]) << 16)
				| ((unsigned int)(data[3]) << 24);
	}
	
	static float
	_read_float (const unsigned char *data, unsigned int& pos)
	{
		pos += 4;
		unsigned int v = ((unsigned int)(data[0])
			| ((unsigned int)(data[1]) << 8)
			| ((unsigned int)(data[2]) << 16)
			| ((unsigned int)(data[3]) << 24));
		return *((float *)&v);
	}
	
	static double
	_read_double (const unsigned char *data, unsigned  int& pos)
	{
		pos += 8;
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
	
	static std::string
	_read_string (const unsigned char *data, unsigned int& pos)
	{
		unsigned short len = ((unsigned short)data[0]) | ((unsigned short)(data[1]) << 8);
		pos += 2 + len;
		std::string str;
		str.reserve (len);
		for (int i = 0; i < len; ++i)
			str.push_back (data[2 + i]);
		return str;
	}
	
	
	
//----
	
	static unsigned int
	_make_chunk_ly_signs (chunk *ch, unsigned char *data, unsigned int n)
	{
		{
			std::lock_guard<std::mutex> guard {ch->ly_signs.lock};
			n += _write_int (data + n, ch->ly_signs.signs.size ());
			for (auto itr = ch->ly_signs.signs.begin (); itr != ch->ly_signs.signs.end (); ++itr)
				{
					block_pos pos = itr->first;
					auto& sign = itr->second;
					
					n += _write_int (data + n, pos.x);
					data[n++] = pos.y;
					n += _write_int (data + n, pos.z);
					
					n += _write_string (data + n, sign.l1.c_str ());
					n += _write_string (data + n, sign.l2.c_str ());
					n += _write_string (data + n, sign.l3.c_str ());
					n += _write_string (data + n, sign.l4.c_str ());
				}
		}
		
		return n;
	}
	
	
	static int
	_chunk_layers_size (chunk *ch)
	{
		unsigned int s = 4; // sign count
		
		// signs
		{
			std::lock_guard<std::mutex> guard {ch->ly_signs.lock};
			for (auto itr = ch->ly_signs.signs.begin (); itr != ch->ly_signs.signs.end (); ++itr)
				{
					auto& sign = itr->second;
					
					s += 9; // x, y, z
					s += 2 + sign.l1.length ();
					s += 2 + sign.l2.length ();
					s += 2 + sign.l3.length ();
					s += 2 + sign.l4.length ();
				}
		}
		
		return s;
	}
	
	
	static unsigned char*
	make_chunk_data (chunk *ch, unsigned int *out_size)
	{
		unsigned short primary_bitmap = 0, add_bitmap = 0;
		unsigned int   data_size = 0;
		int i;
		
		// calculate size needed for array and create bitmaps
		for (i = 0; i < 16; ++i)
			{
				subchunk *sub = ch->get_sub (i);
				if (sub && !sub->all_air ())
					{
						data_size += 14336;
						primary_bitmap |= (1 << i);
						
						if (sub->has_add ())
							{
								add_bitmap |= (1 << i);
								data_size += 2048;
							}
					}
			}
		data_size += 256; // biome array
		data_size += 4; // bitmaps
		data_size += 1; // some bytes
		data_size += _chunk_layers_size (ch); // chunk layers
		
		
		/* 
		 * Create and fill the array:
		 */
		unsigned char *data = new unsigned char[data_size];
		unsigned int n = 0;
		
		data[n++] = ch->generated;
		n += _write_short (data + n, primary_bitmap);
		n += _write_short (data + n, add_bitmap);
		
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->ids, 4096); n += 4096; }
		
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->meta, 2048); n += 2048; }
		
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->blight, 2048); n += 2048; }
		
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->slight, 2048); n += 2048; }
		
		for (i = 0; i < 16; ++i)
			if (add_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->add, 2048); n += 2048; }
		
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->extra, 4096); n += 4096; }
		
		std::memcpy (data + n, ch->get_biome_array (), 256);
		n += 256;
		
		// chunk layers
		n = _make_chunk_ly_signs (ch, data, n);
		
		*out_size = n;
		return data;
	}
	
	
	
	static void
	write_to_sector (hw_chunk *hch, unsigned int index, unsigned char *data,
		unsigned int len, binary_writer writer)
	{
		unsigned int sectors_used = hch->size / 4096;
		if (hch->size % 4096 != 0)
			++ sectors_used;
		
		if (index >= sectors_used)
			{
				int sector_offset;
				
				// create the sector
				writer.seek (0, std::ios_base::end);
				sector_offset = writer.tell () / 512;
				writer.write_bytes (data, len);
				if (len < 4096)
					{
						int rem = 4096 - len;
						while (rem-- > 0)
							writer.write_byte (0);
					}
				
				hch->sector_table[index] = sector_offset;
				
				// update file
				writer.seek ((hch->offset * 512) + 4 + (index * 4));
				writer.write_int (sector_offset);
			}
		else
			{
				// overwrite existing sector
				writer.seek (hch->sector_table[index] * 512);
				writer.write_bytes (data, len);
			}
	}
	
	static void
	write_in_sectors (hw_chunk *hch, unsigned char *data, unsigned int data_size,
		binary_writer writer)
	{
		unsigned int sectors_needed = data_size / 4096;
		if (data_size % 4096 != 0)
			++ sectors_needed;
		
		size_t i, sector_size;
		
		for (i = 0; i < sectors_needed; ++i)
			{
				if (i == (sectors_needed - 1))
					{
						sector_size = data_size % 4096;
						if (sector_size == 0)
							sector_size = 4096;
					}
				else
					sector_size = 4096;
				
				write_to_sector (hch, i, data + (4096 * i), sector_size, writer);
			}
		
		if ((unsigned int)hch->size != data_size)
			{
				hch->size = data_size;
				writer.seek (hch->offset * 512);
				writer.write_int (data_size);
			}
	}
	
	static void
	save_chunk (chunk *ch, int x, int z, hw_superblock **sblocks,
		world_information& inf, binary_writer writer)
	{
		unsigned char *compressed;
		unsigned long compressed_size;
		
		unsigned int data_size = 0;
		unsigned char *data = make_chunk_data (ch, &data_size);
		
		compressed_size = compressBound (data_size);
		compressed = new unsigned char[compressed_size];
		if (compress2 (compressed, &compressed_size, data, data_size,
			Z_BEST_COMPRESSION) != Z_OK)
			{
				delete[] data;
				delete[] compressed;
				throw std::runtime_error ("failed to compress chunk");
			}
		delete[] data;
		
		bool created = false;
		hw_chunk *hch = find_or_create_chunk (x, z, sblocks, writer, true, &created);
		if (hch)
			write_in_sectors (hch, compressed, compressed_size, writer);
		
		if (created)
			{
				// update chunk count
				writer.seek (44);
				writer.write_int (++ (inf.chunk_count));
			}
		
		delete[] compressed;
	}
	
	
	
	/* 
	 * Saves only the specified chunk.
	 */
	void
	hw_provider::save (world& wr, chunk *ch, int x, int z)
	{
		bool close_when_done = false;
		if (!this->strm.is_open ())
			{
				this->open (wr);
				if (!this->strm)
					return;
				close_when_done = true;
			}
		
		binary_writer writer {this->strm};
		save_chunk (ch, x, z, this->sblocks, this->inf, writer);
		//rewrite_header (wr, strm);
		
		if (close_when_done)
			{
				this->close ();
			}
	}
	
	/* 
	 * Updates world information for a given world. 
	 */
	void
	hw_provider::save_info (world &w, const world_information &info)
	{
		binary_writer writer (strm);
		writer.seek (8);
		
		// world dimensions
		writer.write_int (info.width);
		writer.write_int (info.depth);
		
		// spawn pos
		auto spawn_pos = info.spawn_pos;
		writer.write_double (spawn_pos.x);
		writer.write_double (spawn_pos.y);
		writer.write_double (spawn_pos.z);
		writer.write_float (spawn_pos.r);
		writer.write_float (spawn_pos.l);
		
		writer.write_int (info.chunk_count);
		
		// the name of the generator used by the world.
		writer.write_string (info.generator.c_str ());
		
		writer.write_int (info.seed);
		
		writer.write_string (info.world_type.c_str ());
		
		writer.flush ();
		this->inf = info;
	}
	
	
	
//----
	
	static void
	save_empty_imp (world &wr, std::ostream& strm, hw_superblock **sblock_table)
	{
		binary_writer writer (strm);
		
		writer.write_int (0x31765748); // Magic ID ("HWv1")
		writer.write_int (HW_CURR_REV); // revision
		
		// world dimensions
		writer.write_int (wr.get_width ());
		writer.write_int (wr.get_depth ());
		
		// spawn pos
		auto spawn_pos = wr.get_spawn ();
		writer.write_double (spawn_pos.x);
		writer.write_double (spawn_pos.y);
		writer.write_double (spawn_pos.z);
		writer.write_float (spawn_pos.r);
		writer.write_float (spawn_pos.l);
		
		writer.write_int (0); // chunk count
		
		// the name of the generator used by the world.
		if (wr.get_generator ())
			{
				writer.write_string (wr.get_generator ()->name ());
				writer.write_int (wr.get_generator ()->seed ());
			}
		else
			{
				writer.write_string ("");
				writer.write_int (0);
			}
		
		writer.write_string ("NORMAL"); // world type
		
		writer.write_int (HW_LAYER_TABLE_OFFSET); // offset of layer table
		
		writer.pad_to (1024);
		
		// super-block table
		for (int i = 0; i < 4096; ++i)
			{
				writer.write_int (0); // x
				writer.write_int (0); // z
				writer.write_int (0xFFFFFFFFU); // offset
			}
		
		// layer table
		writer.write_int (0); // number of layers
		writer.write_int (0); // the offset of this table's continuation (or 0, if there isn't one)
		for (int i = 0; i < 12; ++i)
			{
				writer.write_int (0); // offset
				writer.write_int (0); // layer size
				writer.write_fixed_string ("#none#", 32);
			}
		writer.pad_to (512);
		
		writer.flush ();
		
		for (int i = 0; i < 4096; ++i)
			sblock_table[i] = nullptr;
	}
	
	/* 
	 * Saves the specified world without writing out any chunks.
	 */
	void
	hw_provider::save_empty (world &wr)
	{
		{
			// check if the file exists
			std::ifstream strm (this->out_path);
			if (strm.is_open ())
				{
					strm.close ();
					return;
				}
		}
		
		std::ofstream strm (this->out_path, std::ios_base::binary | std::ios_base::out
			| std::ios_base::trunc);
		if (!strm)
			throw std::runtime_error ("failed to open world file");
		
		save_empty_imp (wr, strm, this->sblocks);
		strm.close ();
	}
	
	
	
//----
	
	void
	hw_provider::read_layer_table (std::fstream& strm)
	{
		binary_reader reader {strm};
		reader.seek (HW_LAYER_TABLE_OFFSET);
		
		int layer_count = reader.read_int ();
		/*int cont_offset =*/ reader.read_int ();
		
		// TODO: handle pages in the table _itself_
		for (int i = 0; i < layer_count; ++i)
			{
				reader.seek (HW_LAYER_TABLE_OFFSET + 8 + (i * HW_LAYER_SIZE));
				
				int init_page_offset = reader.read_int ();
				int layer_size = reader.read_int ();
				std::string layer_name = reader.read_fixed_string (32);
				
				hw_layer ly;
				ly.name = layer_name;
				ly.size = layer_size;
				
				int page_count = ly.size / HW_LAYER_PAGE_DATA_SIZE;
				if (ly.size % HW_LAYER_PAGE_DATA_SIZE != 0)
					++ page_count;
				
				if (page_count == 0)
					ly.offsets = nullptr;
				else
					{
						ly.offsets = new unsigned int [page_count];
						int count = 0;
						
						ly.offsets[count++] = init_page_offset;
						while (count < page_count)
							{
								reader.seek (ly.offsets[count - 1] * 512);
								ly.offsets[count ++] = reader.read_int ();
							}
					}
				
				this->layers.push_back (ly);
			}
	}	
	
	static void
	read_tables (hw_superblock **sblocks, binary_reader reader)
	{
		reader.seek (HW_SUPERBLOCK_TABLE_OFFSET);
		
		int sb_x, sb_z;
		unsigned int sb_offset;
		for (int i = 0; i < 4096; ++i)
			{
				sb_x = reader.read_int ();
				sb_z = reader.read_int ();
				sb_offset = reader.read_int ();
				if (sb_offset == 0xFFFFFFFFU)
					{
						sblocks[i] = nullptr;
						continue;
					}
				
				hw_superblock *sblock = new hw_superblock (sb_x, sb_z);
				sblocks[i] = sblock;
				sblock->offset = sb_offset;
				
				int saved_pos = reader.tell ();
				int b_x, b_z;
				unsigned int b_offset;
				reader.seek (sblock->offset * 512);
				for (int i = 0; i < 64; ++i)
					{
						b_x = reader.read_int ();
						b_z = reader.read_int ();
						b_offset = reader.read_int ();
						if (b_offset == 0xFFFFFFFFU)
							{
								sblock->blocks[i] = nullptr;
								continue;
							}
						
						hw_block *block = new hw_block (b_x, b_z);
						sblock->blocks[i] = block;
						block->offset = b_offset;
						
						int saved_pos = reader.tell ();
						int r_x, r_z;
						unsigned int r_offset;
						reader.seek (block->offset * 512);
						for (int i = 0; i < 1024; ++i)
							{
								r_x = reader.read_int ();
								r_z = reader.read_int ();
								r_offset = reader.read_int ();
								if (r_offset == 0xFFFFFFFFU)
									{
										block->regions[i] = nullptr;
										continue;
									}
								
								hw_region *region = new hw_region (r_x, r_z);
								block->regions[i] = region;
								region->offset = r_offset;
								
								int saved_pos = reader.tell ();
								int c_x, c_z;
								unsigned int c_offset;
								reader.seek (region->offset * 512);
								for (int i = 0; i < 1024; ++i)
									{
										c_x = reader.read_int ();
										c_z = reader.read_int ();
										c_offset = reader.read_int ();
										if (c_offset == 0xFFFFFFFFU)
											{
												region->chunks[i] = nullptr;
												continue;
											}
										
										hw_chunk *ch = new hw_chunk (c_x, c_z);
										region->chunks[i] = ch;
										ch->offset = c_offset;
										
										int saved_pos = reader.tell ();
										reader.seek (c_offset * 512);
										
										ch->size = reader.read_int ();
										for (int i = 0; i < 256; ++i)
											ch->sector_table[i] = reader.read_int ();
										
										reader.seek (saved_pos);
									}
								reader.seek (saved_pos);
							}
						reader.seek (saved_pos);
					}
				reader.seek (saved_pos);
			}
	}
	
	static void
	read_header (world_information& inf, binary_reader reader)
	{
		if (reader.read_int () != 0x31765748)
			throw world_load_error ("File not in HWv1 format (corrupted?)");
		
		if (reader.read_int () != HW_CURR_REV)
			throw world_load_error ("HWv1: Revision mismatch (outdated?)");
		
		// dimensions
		inf.width = reader.read_int ();
		inf.depth = reader.read_int ();
		
		// spawn pos
		inf.spawn_pos.x = reader.read_double ();
		inf.spawn_pos.y = reader.read_double ();
		inf.spawn_pos.z = reader.read_double ();
		inf.spawn_pos.r = reader.read_float ();
		inf.spawn_pos.l = reader.read_float ();
		inf.spawn_pos.on_ground = true;
		
		inf.chunk_count = reader.read_int ();
		
		// generator
		int generator_len = reader.read_short ();
		inf.generator.clear ();
		inf.generator.reserve (generator_len + 1);
		for (int i = 0; i < generator_len; ++i)
			{
				inf.generator.push_back (reader.read_byte ());
			}
		
		inf.seed = reader.read_int ();
		
		inf.world_type = reader.read_string ();
	}
	
	static void
	read_file (world_information& inf, hw_superblock **sblocks, binary_reader reader)
	{
		read_header (inf, reader);
		read_tables (sblocks, reader);
	}
	
	
	
	static unsigned char*
	combine_sectors (hw_chunk *hch, binary_reader reader, unsigned int *out_size)
	{
		unsigned int compressed_size = hch->size;
		*out_size = compressed_size;
		
		unsigned char *compressed = new unsigned char[compressed_size];
		unsigned int n = 0;
		
		int sector_index = 0;
		while (compressed_size > 0)
			{
				int need = (compressed_size >= 4096) ? 4096 : compressed_size;
				reader.seek (hch->sector_table[sector_index] * 512);
				reader.read_bytes (compressed + n, need);
				
				n += need;
				compressed_size -= need;
				++ sector_index;
			}
		
		return compressed;
	}
	
	
	
	static int
	_fill_ly_signs (chunk *ch, const unsigned char *data, unsigned int n)
	{
		std::lock_guard<std::mutex> guard {ch->ly_signs.lock};
		ch->ly_signs.signs.clear ();
		
		int sign_count = _read_int (data + n, n);
		for (int i = 0; i < sign_count; ++i)
			{
				int x = _read_int (data + n, n);
				int y = data[n++];
				int z = _read_int (data + n, n);
				
				std::string l1 = _read_string (data + n, n);
				std::string l2 = _read_string (data + n, n);
				std::string l3 = _read_string (data + n, n);
				std::string l4 = _read_string (data + n, n);

				ch->ly_signs.insert_sign (x, y, z, l1.c_str (), l2.c_str (),
					l3.c_str (), l4.c_str ());
			}
		
		return n;
	}
	
	
	static void
	fill_chunk (chunk *ch, const unsigned char *data)
	{
		unsigned int n = 0, i;
		unsigned short primary_bitmap, add_bitmap;
		unsigned int d; // dummy value
		
		ch->generated = data[n++];
		primary_bitmap = _read_short (data + 1, d);
		add_bitmap = _read_short (data + 3, d);
		n += 4;
		
		// create sub-chunks
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{
					subchunk *sub = ch->create_sub (i, false);
					sub->air_count = 4096;
					sub->add_count = 0;
					sub->add = nullptr;
				}
		
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (ch->get_sub (i)->ids, data + n, 4096); n += 4096; }
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (ch->get_sub (i)->meta, data + n, 2048); n += 2048; }
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (ch->get_sub (i)->blight, data + n, 2048); n += 2048; }
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (ch->get_sub (i)->slight, data + n, 2048); n += 2048; }
		for (i = 0; i < 16; ++i)
			{
				subchunk *sub = ch->get_sub (i);
				if (add_bitmap & (1 << i))
					{
						sub->add = new unsigned char[2048];
						std::memcpy (ch->get_sub (i)->add, data + n, 2048); n += 2048;
					}
			}
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (ch->get_sub (i)->extra, data + n, 4096); n += 4096; }
		
		// custom blocks
		for (i = 0; i < 16; ++i)
			{
				subchunk *sub = ch->get_sub (i);
				if (!sub) continue;
				
				for (int i = 0; i < 128; ++i)
					sub->custom[i] = 0;
				
				for (int i = 0; i < 4096; ++i)
					if (!block_info::is_vanilla_id (sub->ids[i]))
						sub->custom[i >> 5] |=  (1 << (i & 0x1F));
			}
		
		// biomes
		std::memcpy (ch->get_biome_array (), data + n, 256);
		n += 256;
		
		// calculate add\air count
		for (i = 0; i < 16; ++i)
			{
				subchunk *sub = ch->get_sub (i);
				if (sub)
					{
						for (int j = 0; j < 4096; ++j)
							{
								if (sub->ids[j] != 0)
									-- sub->air_count;
								if (sub->add && sub->add[j >> 1] != 0)
									{
										if (sub->add[j >> 1] >> 4)
											++ sub->add_count;
										if (sub->add[j >> 1] & 0xF)
											++ sub->add_count;
									}
							}
					}
			}
		
		// and finally, layers
		n = _fill_ly_signs (ch, data, n);
	}
	
	/* 
	 * Attempts to load the chunk located at the specified coordinates into
	 * @{ch}. Returns true on success, and false if the chunk is not present
	 * within the world file.
	 */
	bool
	hw_provider::load (world &wr, chunk *ch, int x, int z)
	{
		if (!this->strm.is_open ())
			return false;
		
		binary_writer writer; // not actually used
		hw_chunk *hch = find_or_create_chunk (x, z, this->sblocks, writer, false);
		if (!hch) return false;
		
		binary_reader reader {this->strm};
		
		unsigned int compressed_size = 0;
		unsigned char *compressed = combine_sectors (hch, reader, &compressed_size);
		
		unsigned long data_size = 524288;
		unsigned char *data = new unsigned char[data_size];
		if (uncompress (data, &data_size, compressed, compressed_size) != Z_OK)
			{
				delete[] compressed;
				delete[] data;
				throw std::runtime_error ("failed to decompress chunk");
			}
		delete[] compressed;
		
		fill_chunk (ch, data);
		delete[] data;
		return true;
	}
	
	
	
	static unsigned int
	_create_layer_page (binary_writer writer)
	{
		writer.seek (0, std::ios_base::end);
		writer.pad_to (512);
		unsigned int page_offset = (unsigned int)writer.tell ();
		writer.write_int (0); // offset to next page
		
		unsigned int left = HW_LAYER_PAGE_DATA_SIZE;
		while (left > 0)
			{
				writer.write_int (0);
				left -= 4;
			}
		
		return page_offset;
	}
	
	void
	hw_provider::write_layer (const char *layer_name, const unsigned char *data,
		unsigned int layer_size)
	{
		int ly_index = -1;
		for (size_t i = 0; i < this->layers.size (); ++i)
			if (this->layers[i].name.compare (layer_name) == 0)
				{ ly_index = i;  break; }
		
		binary_writer writer {this->strm};
		unsigned int written = 0;
		
		hw_layer *ly;
		unsigned int init_page_offset;
		if (ly_index == -1)
			{
				// create the initial page
				init_page_offset = _create_layer_page (writer);
				
				// link this page to the layer table.
				ly_index = this->layers.size ();
				writer.seek (HW_LAYER_TABLE_OFFSET + 8 + (this->layers.size () * HW_LAYER_SIZE));
				writer.write_int (init_page_offset / 512);
				writer.write_int (layer_size);
				writer.write_fixed_string (layer_name, 32);
				
				// update layer count
				writer.seek (HW_LAYER_TABLE_OFFSET);
				writer.write_int (this->layers.size () + 1);
				
				int page_count = layer_size / HW_LAYER_PAGE_DATA_SIZE;
				if (layer_size % HW_LAYER_PAGE_DATA_SIZE != 0)
					++ page_count;
				
				hw_layer l;
				l.name.assign (layer_name);
				l.offsets = new unsigned int [page_count];
				l.offsets[0] = init_page_offset / 512;
				for (int i = 1; i < page_count; ++i)
					l.offsets[i] = 0;
				l.size = layer_size;
				this->layers.push_back (l);
				ly = &this->layers[this->layers.size () - 1];
			}
		else
			{
				ly = &this->layers[ly_index];
				init_page_offset = ly->offsets[0] * 512;
				
				// update layer size
				if (ly->size != layer_size)
					{
						int old_page_count = ly->size / HW_LAYER_PAGE_DATA_SIZE + !!(ly->size % HW_LAYER_PAGE_DATA_SIZE);
						int new_page_count = layer_size / HW_LAYER_PAGE_DATA_SIZE + !!(layer_size % HW_LAYER_PAGE_DATA_SIZE);
						
						int i;
						unsigned int *new_offsets = new unsigned int [new_page_count];
						for (i = 0; i < old_page_count && i < new_page_count; ++i)
							new_offsets[i] = ly->offsets[i];
						for (; i < new_page_count; ++i)
							new_offsets[i] = 0;
						
						delete[] ly->offsets;
						ly->offsets = new_offsets;
						
						ly->size = layer_size;
						writer.seek (HW_LAYER_TABLE_OFFSET + 8 + (ly_index * HW_LAYER_SIZE) + 4);
						writer.write_int (layer_size);
					}
			}
		
		int page_index = 0;
		unsigned int curr_page_offset = init_page_offset;
		unsigned int left = layer_size - written;
		writer.seek (init_page_offset + 4); // skip offset of next page
		while (left > 0)
			{
				unsigned int need = (left > HW_LAYER_PAGE_DATA_SIZE) ? HW_LAYER_PAGE_DATA_SIZE : left;
				
				writer.write_bytes (data + written, need);
				if (need < HW_LAYER_PAGE_DATA_SIZE)
					{
						// pad the rest
						unsigned int rem = HW_LAYER_PAGE_DATA_SIZE - need;
						while (rem > 4)
							{ writer.write_int (0); rem -= 4; }
						while (rem > 0)
							{ writer.write_byte (0); -- rem; }
					}
					
				left -= need;
				written += need;
				
				if (left > 0)
					{
						++ page_index;
						
						if (ly->offsets[page_index] == 0)
							{
								// allocate another page
								unsigned int next_page_offset = _create_layer_page (writer);
								writer.seek (curr_page_offset);
								writer.write_int (next_page_offset / 512);
								writer.seek (next_page_offset + 4);
								ly->offsets[page_index] = next_page_offset / 512;
							}
						else
							writer.seek (ly->offsets[page_index] * 512 + 4);
						
						curr_page_offset = ly->offsets[page_index] * 512;
					}
			}
		
		writer.flush ();
	}
	
	unsigned char*
	hw_provider::read_layer (const char *layer_name, unsigned int& data_size)
	{
		int ly_index = -1;
		for (size_t i = 0; i < this->layers.size (); ++i)
			if (this->layers[i].name.compare (layer_name) == 0)
				{ ly_index = i;  break; }
			
		if (ly_index == -1 || this->layers[ly_index].size == 0)
			{
				data_size = 0;
				return nullptr;
			}
		
		unsigned char *data = new unsigned char [this->layers[ly_index].size];
		
		hw_layer &ly = this->layers[ly_index];
		binary_reader reader {this->strm};
		
		int page_index = 0;
		unsigned int left = ly.size;
		unsigned int read = 0;
		reader.seek (ly.offsets[0] * 512 + 4);
		while (left > 0)
			{
				int need = (left > HW_LAYER_PAGE_DATA_SIZE) ? HW_LAYER_PAGE_DATA_SIZE : left;
				reader.read_bytes (data + read, need);
				
				left -= need;
				read += need;
				
				if (left > 0)
					{
						// fetch next page
						++ page_index;
						reader.seek (ly.offsets[page_index] * 512 + 4);
					}
			}
		
		data_size = read;
		return data;
	}
	
	
	
//-----
	
	/* 
	 * Saves the specified list of portals to disk.
	 */
	void
	hw_provider::save_portals (world &wr, const std::vector<portal *>& portals)
	{
		unsigned int data_size = 4; // portal count
		for (const portal *p : portals)
			{
				data_size += 32; // dest pos (double, double, double, float, float)
				data_size += 2 + p->dest_world.size ();
				data_size += 9*2; // range minimum & maximum (int, byte, int)
				
				// affected blocks
				data_size += 4;   // count
				data_size += p->affected.size () * 9;
			}
			
		unsigned char *data = new unsigned char[data_size];
		unsigned int pos = 0;
		
		pos += _write_int (data + pos, portals.size ());
		for (const portal *p : portals)
			{
				// dest pos
				pos += _write_double (data + pos, p->dest_pos.x);
				pos += _write_double (data + pos, p->dest_pos.y);
				pos += _write_double (data + pos, p->dest_pos.z);
				pos += _write_float (data + pos, p->dest_pos.r);
				pos += _write_float (data + pos, p->dest_pos.l);
				
				// dest world
				pos += _write_string (data + pos, p->dest_world);
				
				// range
				pos += _write_int (data + pos, p->r_min.x);
				data[pos++] = p->r_min.y;
				pos += _write_int (data + pos, p->r_min.z);
				pos += _write_int (data + pos, p->r_max.x);
				data[pos++] = p->r_max.y;
				pos += _write_int (data + pos, p->r_max.z);
				
				// affected blocks
				pos += _write_int (data + pos, p->affected.size ());
				for (block_pos bp : p->affected)
					{
						pos += _write_int (data + pos, bp.x);
						data[pos++] = bp.y;
						pos += _write_int (data + pos, bp.z);
					}
			}
		
		this->write_layer ("portals", data, data_size);
		delete[] data;
	}
	
	/* 
	 * Loads the portal list from disk into the given vector.
	 */
	void
	hw_provider::load_portals (world &wr, std::vector<portal *>& portals)
	{
		unsigned int data_size = 0;
		unsigned char *data = this->read_layer ("portals", data_size);
		if (!data || data_size == 0)
			return;
		
		unsigned int pos = 0;
		
		unsigned int portal_count = _read_int (data + pos, pos);
		
		for (unsigned int i = 0; i < portal_count; ++i)
			{
				portal *ptl = new portal ();
				
				// dest pos
				ptl->dest_pos.x = _read_double (data + pos, pos);
				ptl->dest_pos.y = _read_double (data + pos, pos);
				ptl->dest_pos.z = _read_double (data + pos, pos);
				ptl->dest_pos.r = _read_float (data + pos, pos);
				ptl->dest_pos.l = _read_float (data + pos, pos);
				
				// dest world
				ptl->dest_world = _read_string (data + pos, pos);
				
				// range
				ptl->r_min.x = _read_int (data + pos, pos);
				ptl->r_min.y = data[pos++];
				ptl->r_min.z = _read_int (data + pos, pos);
				ptl->r_max.x = _read_int (data + pos, pos);
				ptl->r_max.y = data[pos++];
				ptl->r_max.z = _read_int (data + pos, pos);
				
				// affected blocks
				int block_count = _read_int (data + pos, pos);
				for (int j = 0; j < block_count; ++j)
					{
						block_pos bp;
						bp.x = _read_int (data + pos, pos);
						bp.y = data[pos++];
						bp.z = _read_int (data + pos, pos);
						ptl->affected.push_back (bp);
					}
				
				portals.push_back (ptl);
			}
		
		delete[] data;
	}
	
	
	
//-----
	
	/* 
	 * Updates\saves owner\member list, build\join permissions, etc...
	 */
	void
	hw_provider::save_security (world &w, const world_security& sec)
	{
		unsigned int data_size = 0;
		data_size += 4 + 4 * sec.get_owners ().size ();    // owner list
		data_size += 4 + 4 * sec.get_members ().size ();   // member list
		data_size += 2 + sec.get_build_perms ().length (); // build perms
		data_size += 2 + sec.get_join_perms ().length ();  // join perms
		
		unsigned char *data = new unsigned char [data_size];
		unsigned int n = 0;
		
		// owners
		const auto& owners = sec.get_owners ();
		n += _write_int (data + n, owners.size ());
		for (int p : owners)
			n += _write_int (data + n, p);
		
		// members
		const auto& members = sec.get_members ();
		n += _write_int (data + n, members.size ());
		for (int p : members)
			n += _write_int (data + n, p);
		
		// build perms
		const std::string& build_perms = sec.get_build_perms ();
		n += _write_short (data + n, build_perms.length ());
		for (char c : build_perms)
			data[n++] = c;
		
		// join perms
		const std::string& join_perms = sec.get_join_perms ();
		n += _write_short (data + n, join_perms.length ());
		for (char c : join_perms)
			data[n++] = c;
		
		this->write_layer ("security", data, data_size);
		delete[] data;
	}
	
	/* 
	 * Loads world security information.
	 */
	void
	hw_provider::load_security (world &w, world_security& sec)
	{
		unsigned int data_size = 0;
		unsigned char *data = this->read_layer ("security", data_size);
		if (!data || data_size == 0)
			return;
		
		unsigned int n = 0;
		
		// owners
		int owner_count = _read_int (data + n, n);
		while (owner_count --> 0)
			sec.add_owner (_read_int (data + n, n));
		
		// members
		int member_count = _read_int (data + n, n);
		while (member_count --> 0)
			sec.add_member (_read_int (data + n, n));
		
		// build perms
		sec.set_build_perms (_read_string (data + n, n));
		
		// join perms
		sec.set_join_perms (_read_string (data + n, n));
		
		delete[] data;
	}
}

