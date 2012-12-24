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

#include "nbt.hpp"
#include <cstring>


namespace hCraft {
	
	nbt_tag::nbt_tag (const char *name)
		: tag_name (name)
		{ }
	
	nbt_tag::nbt_tag (const std::string& name)
		: tag_name (name)
		{ }
	
	
	
	int
	nbt_tag_list::size (bool with_id_and_tag)
	{
		int res = 5;
		if (with_id_and_tag)
			res += nbt_tag::size ();
		for (nbt_tag *tag : this->tags)
			res += tag->size (false);
		return res;
	}
	
	int
	nbt_tag_compound::size (bool with_id_and_tag)
	{
		int res = 1; // TAG_END
		if (with_id_and_tag)
			res += nbt_tag::size ();
		for (nbt_tag *tag : this->tags)
			res += tag->size (true);
		return res;
	}
	
	
	
	nbt_tag_byte_array::nbt_tag_byte_array (const char *name, int reserve)
		: nbt_tag (name)
	{
		this->arr = new char[reserve];
		this->arr_size = reserve;
	}
	
	nbt_tag_byte_array::nbt_tag_byte_array (const std::string& name, int reserve)
		: nbt_tag (name)
	{
		this->arr = new char[reserve];
		this->arr_size = reserve;
	}
	
	
	
//----
	
	static void
	_write_short (unsigned char *out, unsigned short val)
	{
		out[0] = (val >> 8) & 0xFF;
		out[1] = val & 0xFF;
	}
	
	/* 
	 * Serializes the tag onto the given byte array.
	 * Returns the number of bytes written.
	 */
	int
	nbt_tag::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		
		if (encode_id_and_name)
			{
				*ptr++ = (unsigned char)this->type ();
		
				// name
				const std::string& n = this->name ();
				_write_short (ptr, n.size ());
				ptr += 2;
				std::memcpy (ptr, n.c_str (), n.size ());
				ptr += n.size ();
			}
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_byte::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		*ptr++ = this->val;
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_short::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		*ptr++ = (this->val >> 8) & 0xFF;
		*ptr++ = this->val & 0xFF;
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_int::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		*ptr++ = (this->val >> 24) & 0xFF;
		*ptr++ = (this->val >> 16) & 0xFF;
		*ptr++ = (this->val >> 8) & 0xFF;
		*ptr++ = this->val & 0xFF;
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_long::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		*ptr++ = (this->val >> 56) & 0xFF;
		*ptr++ = (this->val >> 48) & 0xFF;
		*ptr++ = (this->val >> 40) & 0xFF;
		*ptr++ = (this->val >> 32) & 0xFF;
		*ptr++ = (this->val >> 24) & 0xFF;
		*ptr++ = (this->val >> 16) & 0xFF;
		*ptr++ = (this->val >> 8) & 0xFF;
		*ptr++ = this->val & 0xFF;
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_float::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		unsigned int num = *((unsigned int *)&this->val);
		*ptr++ = (num >> 24) & 0xFF;
		*ptr++ = (num >> 16) & 0xFF;
		*ptr++ = (num >> 8) & 0xFF;
		*ptr++ = num & 0xFF;
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_double::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		unsigned long long num = *((unsigned long long *)&this->val);
		*ptr++ = (num >> 56) & 0xFF;
		*ptr++ = (num >> 48) & 0xFF;
		*ptr++ = (num >> 40) & 0xFF;
		*ptr++ = (num >> 32) & 0xFF;
		*ptr++ = (num >> 24) & 0xFF;
		*ptr++ = (num >> 16) & 0xFF;
		*ptr++ = (num >> 8) & 0xFF;
		*ptr++ = num & 0xFF;
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_byte_array::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload	
		*ptr++ = (this->arr_size >> 24) & 0xFF;
		*ptr++ = (this->arr_size >> 16) & 0xFF;
		*ptr++ = (this->arr_size >> 8) & 0xFF;
		*ptr++ = this->arr_size & 0xFF;
		std::memcpy (ptr, this->arr, this->arr_size);
		ptr += this->arr_size;
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_string::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		const std::string& str = this->value ();
		unsigned short str_len = str.size ();
		*ptr++ = (str_len >> 8) & 0xFF;
		*ptr++ = str_len & 0xFF;
		std::memcpy (ptr, str.c_str (), str_len);
		ptr += str_len;
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_list::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		
		*ptr++ = (unsigned char)this->tags_type;
		
		unsigned int tag_count = this->tags.size ();
		*ptr++ = (tag_count >> 24) & 0xFF;
		*ptr++ = (tag_count >> 16) & 0xFF;
		*ptr++ = (tag_count >> 8) & 0xFF;
		*ptr++ = tag_count & 0xFF;
		
		for (nbt_tag *tag : this->tags)
			ptr += tag->encode (ptr, false);
		
		return (int)(ptr - out);
	}
	
	int
	nbt_tag_compound::encode (unsigned char *out, bool encode_id_and_name)
	{
		unsigned char *ptr = out;
		if (encode_id_and_name)
			ptr += nbt_tag::encode (out, true); // encode id and name
		
		// payload
		for (nbt_tag *tag : this->tags)
			ptr += tag->encode (ptr, true);
		*ptr++ = 0; // TAG_END
		
		return (int)(ptr - out);
	}
	
	
	
//----
	
	static int
	read_tag_name (const unsigned char *data, std::string& name)
	{
		const unsigned char *ptr = data;
		
		++ ptr; // skip id
			
		unsigned short name_len =
			(((unsigned short)ptr[0]) << 8) |
			(((unsigned short)ptr[1])     );
		ptr += 2;
		name.clear ();
		name.reserve (name_len + 1);
		while (name_len-- > 0)
			name.push_back (*ptr++);
		
		return (int)(ptr - data);
	}
	
	
	
	/* 
	 * Deserializes an NBT tag from the given byte array.
	 */
	nbt_tag*
	nbt_tag::decode (const unsigned char *data, int *read)
	{
		const unsigned char *ptr = data;
		int type = *ptr++;
		if (type > TAG_INT_ARRAY)
			return nullptr;
		
		switch (type)
			{
				case TAG_END: *read = 1; return nullptr;
				case TAG_BYTE: return nbt_tag_byte::decode (data, true, read);
				case TAG_SHORT: return nbt_tag_short::decode (data, true, read);
				case TAG_INT: return nbt_tag_int::decode (data, true, read);
				case TAG_LONG: return nbt_tag_long::decode (data, true, read);
				case TAG_FLOAT: return nbt_tag_float::decode (data, true, read);
				case TAG_DOUBLE: return nbt_tag_double::decode (data, true, read);
				case TAG_BYTE_ARRAY: return nbt_tag_byte_array::decode (data, true, read);
				case TAG_STRING: return nbt_tag_string::decode (data, true, read);
				case TAG_LIST: return nbt_tag_list::decode (data, true, read);
				case TAG_COMPOUND: return nbt_tag_compound::decode (data, true, read);
			}
		
		return nullptr; // never reached
	}
	
	/* 
	 * Deserializes an NBT tag when its type is already known.
	 * NOTE: This function does not attempt to handle the initial id and name
	 *       fields.
	 */
	nbt_tag*
	nbt_tag::decode_as (const unsigned char *data, nbt_tag_type type, int *read)
	{
		switch (type)
			{
				case TAG_END: return nullptr;
				case TAG_BYTE: return nbt_tag_byte::decode (data, false, read);
				case TAG_SHORT: return nbt_tag_short::decode (data, false, read);
				case TAG_INT: return nbt_tag_int::decode (data, false, read);
				case TAG_LONG: return nbt_tag_long::decode (data, false, read);
				case TAG_FLOAT: return nbt_tag_float::decode (data, false, read);
				case TAG_DOUBLE: return nbt_tag_double::decode (data, false, read);
				case TAG_BYTE_ARRAY: return nbt_tag_byte_array::decode (data, false, read);
				case TAG_STRING: return nbt_tag_string::decode (data, false, read);
				case TAG_LIST: return nbt_tag_list::decode (data, false, read);
				case TAG_COMPOUND: return nbt_tag_compound::decode (data, false, read);
				case TAG_INT_ARRAY: /* TODO */ return nullptr;
			}
		
		return nullptr;
	}
	
	
	
	nbt_tag_byte*
	nbt_tag_byte::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_byte *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		tag = new nbt_tag_byte (name);
		tag->set ((char)*ptr++);
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_short*
	nbt_tag_short::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_short *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		tag = new nbt_tag_short (name);
		tag->set (
			(((unsigned short)ptr[0]) << 8) |
			(((unsigned short)ptr[1])     ));
		ptr += 2;
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_int*
	nbt_tag_int::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_int *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		tag = new nbt_tag_int (name);
		tag->set (
			(((unsigned int)ptr[0]) << 24) |
			(((unsigned int)ptr[1]) << 16) |
			(((unsigned int)ptr[2]) <<  8) |
			(((unsigned int)ptr[3])      ));
		ptr += 4;
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_long*
	nbt_tag_long::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_long *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		tag = new nbt_tag_long (name);
		tag->set (
			(((unsigned long long)ptr[0]) << 56) |
			(((unsigned long long)ptr[1]) << 48) |
			(((unsigned long long)ptr[2]) << 40) |
			(((unsigned long long)ptr[3]) << 32) |
			(((unsigned long long)ptr[4]) << 24) |
			(((unsigned long long)ptr[5]) << 16) |
			(((unsigned long long)ptr[6]) <<  8) |
			(((unsigned long long)ptr[7])      ));
		ptr += 8;
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_float*
	nbt_tag_float::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_float *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		tag = new nbt_tag_float (name);
		
		unsigned int num = 
			(((unsigned int)ptr[0]) << 24) |
			(((unsigned int)ptr[1]) << 16) |
			(((unsigned int)ptr[2]) <<  8) |
			(((unsigned int)ptr[3])      );
		ptr += 4;
		tag->set (*((float *)&num));
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_double*
	nbt_tag_double::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_double *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		tag = new nbt_tag_double (name);
		
		unsigned long long num = 
			(((unsigned long long)ptr[0]) << 56) |
			(((unsigned long long)ptr[1]) << 48) |
			(((unsigned long long)ptr[2]) << 40) |
			(((unsigned long long)ptr[3]) << 32) |
			(((unsigned long long)ptr[4]) << 24) |
			(((unsigned long long)ptr[5]) << 16) |
			(((unsigned long long)ptr[6]) <<  8) |
			(((unsigned long long)ptr[7])      );
		ptr += 8;
		tag->set (*((double *)&num));
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_byte_array*
	nbt_tag_byte_array::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_byte_array *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		unsigned int byte_count = 
			(((unsigned int)ptr[0]) << 24) |
			(((unsigned int)ptr[1]) << 16) |
			(((unsigned int)ptr[2]) <<  8) |
			(((unsigned int)ptr[3])      );
		ptr += 4;
		
		tag = new nbt_tag_byte_array (name, byte_count);
		char *bytes = tag->value ();
		std::memcpy (bytes, ptr, byte_count);
		ptr += byte_count;
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_string*
	nbt_tag_string::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_string *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		unsigned int str_len = 
			(((unsigned short)ptr[0]) <<  8) |
			(((unsigned short)ptr[1])      );
		ptr += 2;
		
		tag = new nbt_tag_string (name);
		std::string& str = tag->value ();
		
		str.reserve (str_len + 1);
		for (unsigned int i = 0; i < str_len; ++i)
			str.push_back (*ptr++);
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_list*
	nbt_tag_list::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_list *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		int type = *ptr++;
		if (type > TAG_INT_ARRAY)
			return nullptr;
		tag = new nbt_tag_list (name, (nbt_tag_type)type);
		
		unsigned int tag_count = 
			(((unsigned int)ptr[0]) << 24) |
			(((unsigned int)ptr[1]) << 16) |
			(((unsigned int)ptr[2]) <<  8) |
			(((unsigned int)ptr[3])      );
		ptr += 4;
		while (tag_count-- > 0)
			{
				int read = 0;
				nbt_tag *child = nbt_tag::decode_as (ptr, (nbt_tag_type)type, &read);
				ptr += read;
				if (!child)
					break;
				tag->add (child);
			}
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
	
	nbt_tag_compound*
	nbt_tag_compound::decode (const unsigned char *data, bool id_and_name, int *read)
	{
		nbt_tag_compound *tag;
		std::string name;
		
		const unsigned char *ptr = data;
		if (id_and_name)
			ptr += read_tag_name (data, name);
		
		tag = new nbt_tag_compound (name);
		for (;;)
			{
				int read = 0;
				nbt_tag *child = nbt_tag::decode (ptr, &read);
				ptr += read;
				if (!child)
					break;
				tag->add (child);
			}
		
		if (read)
			*read = (int)(ptr - data);
		return tag;
	}
}

