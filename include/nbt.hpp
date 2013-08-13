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

#ifndef _hCraft__NBT_H_
#define _hCraft__NBT_H_

#include <string>
#include <vector>
#include <unordered_map>


namespace hCraft {
	
	/* 
	 * Values for a tag's initial byte.
	 */
	enum nbt_tag_type
	{
		TAG_END,
		TAG_BYTE,
		TAG_SHORT,
		TAG_INT,
		TAG_LONG,
		TAG_FLOAT,
		TAG_DOUBLE,
		TAG_BYTE_ARRAY,
		TAG_STRING,
		TAG_LIST,
		TAG_COMPOUND,
		TAG_INT_ARRAY,
	};
	
	
	/* 
	 * Base class for all tag types.
	 */
	class nbt_tag
	{
		std::string tag_name;
	public:
		nbt_tag (const char *name);
		nbt_tag (const std::string& name);
		
		virtual ~nbt_tag () {};
		virtual nbt_tag_type type () = 0;
		virtual int size (bool with_id_and_tag = true)
			{ return with_id_and_tag ? (3 + tag_name.size ()) : 0; }
		virtual const std::string& name () { return this->tag_name; }
		
		/* 
		 * Serializes the tag onto the given byte array.
		 * Returns the number of bytes written.
		 */
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		
		/* 
		 * Deserializes an NBT tag from the given byte array.
		 */
		static nbt_tag* decode (const unsigned char *data, int *read = nullptr);
		
		/* 
		 * Deserializes an NBT tag when its type is already known.
		 * NOTE: This function does not attempt to handle the initial id and name
		 *       fields.
		 */
		static nbt_tag* decode_as (const unsigned char *data, nbt_tag_type type,
			int *read = nullptr);
	};
	
	
	class nbt_tag_byte: public nbt_tag
	{
		char val;
	public:
		nbt_tag_byte (const char *name)
			: nbt_tag (name) {}
		nbt_tag_byte (const std::string& name)
			: nbt_tag (name) {}
		
		virtual nbt_tag_type type () { return TAG_BYTE; }
		virtual int size (bool with_id_and_tag = true)
			{ return 1 + (with_id_and_tag ? nbt_tag::size () : 0); }
		char& value () { return this->val; }
		nbt_tag_byte* set (char val)
			{ this->val = val; return this; }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_byte* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_short: public nbt_tag
	{
		short val;
	public:
		nbt_tag_short (const char *name)
			: nbt_tag (name) {}
		nbt_tag_short (const std::string& name)
			: nbt_tag (name) {}
		
		virtual nbt_tag_type type () { return TAG_SHORT; }
		virtual int size (bool with_id_and_tag = true)
			{ return 2 + (with_id_and_tag ? nbt_tag::size () : 0); }
		short& value () { return this->val; }
		nbt_tag_short* set (short val)
			{ this->val = val; return this; }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_short* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_int: public nbt_tag
	{
		int val;
	public:
		nbt_tag_int (const char *name)
			: nbt_tag (name) {}
		nbt_tag_int (const std::string& name)
			: nbt_tag (name) {}
		
		virtual nbt_tag_type type () { return TAG_INT; }
		virtual int size (bool with_id_and_tag = true)
			{ return 4 + (with_id_and_tag ? nbt_tag::size () : 0); }
		int& value () { return this->val; }
		nbt_tag_int* set (int val)
			{ this->val = val; return this; }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_int* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_long: public nbt_tag
	{
		long long val;
	public:
		nbt_tag_long (const char *name)
			: nbt_tag (name) {}
		nbt_tag_long (const std::string& name)
			: nbt_tag (name) {}
			
		virtual nbt_tag_type type () { return TAG_LONG; }
		virtual int size (bool with_id_and_tag = true)
			{ return 8 + (with_id_and_tag ? nbt_tag::size () : 0); }
		long long& value () { return this->val; }
		nbt_tag_long* set (long long val)
			{ this->val = val; return this; }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_long* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_float: public nbt_tag
	{
		float val;
	public:
		nbt_tag_float (const char *name)
			: nbt_tag (name) {}
		nbt_tag_float (const std::string& name)
			: nbt_tag (name) {}
		
		virtual nbt_tag_type type () { return TAG_FLOAT; }
		virtual int size (bool with_id_and_tag = true)
			{ return 4 + (with_id_and_tag ? nbt_tag::size () : 0); }
		float& value () { return this->val; }
		nbt_tag_float* set (float val)
			{ this->val = val; return this; }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_float* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_double: public nbt_tag
	{
		double val;
	public:
		nbt_tag_double (const char *name)
			: nbt_tag (name) {}
		nbt_tag_double (const std::string& name)
			: nbt_tag (name) {}
		
		virtual nbt_tag_type type () { return TAG_DOUBLE; }
		virtual int size (bool with_id_and_tag = true)
			{ return 8 + (with_id_and_tag ? nbt_tag::size () : 0); }
		double& value () { return this->val; }
		nbt_tag_double* set (double val)
			{ this->val = val; return this; }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_double* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_byte_array: public nbt_tag
	{
		char *arr;
		int  arr_size;
	public:
		nbt_tag_byte_array (const char *name, int reserve);
		nbt_tag_byte_array (const std::string& name, int reserve);
		
		virtual ~nbt_tag_byte_array ()
			{ delete[] arr; }
		
		virtual nbt_tag_type type () { return TAG_BYTE_ARRAY; }
		virtual int size (bool with_id_and_tag = true)
			{ return (4 + arr_size) + (with_id_and_tag ? nbt_tag::size () : 0); }
		char* value () { return this->arr; }
		int   length () { return this->arr_size; }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_byte_array* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_string: public nbt_tag
	{
		std::string val;
	public:
		nbt_tag_string (const char *name)
			: nbt_tag (name) {}
		nbt_tag_string (const std::string& name)
			: nbt_tag (name) {}
		
		virtual nbt_tag_type type () { return TAG_STRING; }
		virtual int size (bool with_id_and_tag = true)
			{ return (2 + this->val.size ()) + (with_id_and_tag ? nbt_tag::size () : 0); }
		std::string& value () { return this->val; }
		int length () { return this->val.size (); }
		nbt_tag_string* set (const char *str)
			{ this->val.assign (str); return this; }
		nbt_tag_string* set (const std::string& str)
			{ this->val.assign (str); return this; }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_string* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_list: public nbt_tag
	{
		nbt_tag_type tags_type;
		std::vector<nbt_tag *> tags;
	public:
		nbt_tag_list (const char *name, nbt_tag_type t)
			: nbt_tag (name) { this->tags_type = t; }
		nbt_tag_list (const std::string& name, nbt_tag_type t)
			: nbt_tag (name) { this->tags_type = t; }
		virtual ~nbt_tag_list ()
			{ for (nbt_tag *tag : this->tags) delete tag; }
		
		virtual nbt_tag_type type () { return TAG_LIST; }
		virtual int size (bool with_id_and_tag = true);
		nbt_tag_type contained_type () { return this->tags_type; }
		std::vector<nbt_tag *>& value () { return this->tags; }
		int length () { return this->tags.size (); }
		
		inline std::vector<nbt_tag *>::iterator
		begin ()
			{ return this->tags.begin (); }
		inline std::vector<nbt_tag *>::iterator
		end ()
			{ return this->tags.end (); }
		
		void
		add (nbt_tag *tag)
			{ this->tags.push_back (tag); }
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_list* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
	
	class nbt_tag_compound: public nbt_tag
	{
		std::unordered_map<std::string, nbt_tag *> tag_map;
		std::vector<nbt_tag *> tags;
	public:
		nbt_tag_compound (const char *name)
			: nbt_tag (name) {}
		nbt_tag_compound (const std::string& name)
			: nbt_tag (name) {}
		
		virtual ~nbt_tag_compound ()
		{
			for (nbt_tag *tag : tags)
				delete tag;
			tags.clear ();
			tag_map.clear ();
		}
		
		virtual nbt_tag_type type () { return TAG_COMPOUND; }
		virtual int size (bool with_id_and_tag = true);
		std::vector<nbt_tag *>& value () { return this->tags; }
		int length () { return this->tags.size (); }
		
		inline std::vector<nbt_tag *>::iterator
		begin ()
			{ return this->tags.begin (); }
		inline std::vector<nbt_tag *>::iterator
		end ()
			{ return this->tags.end (); }
		
		void
		add (nbt_tag *tag)
			{
				this->tags.push_back (tag);
				this->tag_map[tag->name ()] = tag;
			}
		
		nbt_tag*
		operator[] (const char *name) const
		{
			auto itr = this->tag_map.find (name);
			if (itr != this->tag_map.end ())
				return itr->second;
			return nullptr;
		}
		
		virtual int encode (unsigned char *out, bool encode_id_and_name = true);
		static nbt_tag_compound* decode (const unsigned char *data, bool id_and_name = true,
			int *read = nullptr);
	};
}

#endif

