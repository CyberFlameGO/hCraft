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

#include "packet.hpp"
#include "chunk.hpp"
#include "entities/entity.hpp"
#include "utils.hpp"
#include "nbt.hpp"
#include "player.hpp"
#include <cstring>
#include <zlib.h>
#include <cmath>
#include <sstream>
#include <string>

#include <cryptopp/queue.h>
#include <iostream> // DEBUG


namespace hCraft {
	
	/* 
	 * Constructs a new packet that can hold up to the specified amount of bytes.
	 */
	packet::packet (unsigned int size)
	{
		this->size = 0;
		this->pos  = 0;
		this->cap  = size;
		this->data = new unsigned char[size];
	}
	
	/* 
	 * Class copy constructor.
	 */
	packet::packet (const packet &other)
	{
		this->size = other.size;
		this->pos  = other.pos;
		this->cap  = other.cap;
		
		this->data = new unsigned char [other.cap];
		std::memcpy (this->data, other.data, other.size);
	}
	
	/* 
	 * Class destructor.
	 */
	packet::~packet ()
	{
		delete[] this->data;
	}
	
	
	
	/* 
	 * put methods:
	 */
	
	void
	packet::put_byte (uint8_t val)
	{
		this->data[(this->pos) ++] = val;
		++ this->size;
	}
	
	void
	packet::put_short (uint16_t val)
	{
		this->data[this->pos + 0] = (val >>  8) & 0xFF;
		this->data[this->pos + 1] = (val      ) & 0xFF;
		
		this->pos += 2; this->size += 2;
	}
	
	void
	packet::put_int (uint32_t val)
	{
		this->data[this->pos + 0] = (val >> 24) & 0xFF;
		this->data[this->pos + 1] = (val >> 16) & 0xFF;
		this->data[this->pos + 2] = (val >>  8) & 0xFF;
		this->data[this->pos + 3] = (val      ) & 0xFF;
		
		this->pos += 4; this->size += 4;
	}
	
	void
	packet::put_long (uint64_t val)
	{
		this->data[this->pos + 0] = (val >> 56) & 0xFF;
		this->data[this->pos + 1] = (val >> 48) & 0xFF;
		this->data[this->pos + 2] = (val >> 40) & 0xFF;
		this->data[this->pos + 3] = (val >> 32) & 0xFF;
		this->data[this->pos + 4] = (val >> 24) & 0xFF;
		this->data[this->pos + 5] = (val >> 16) & 0xFF;
		this->data[this->pos + 6] = (val >>  8) & 0xFF;
		this->data[this->pos + 7] = (val      ) & 0xFF;
		
		this->pos += 8; this->size += 8;
	}
	
	void
	packet::put_float (float val)
	{
		this->put_int (*((uint32_t *)&val));
	}
	
	void
	packet::put_double (double val)
	{
		this->put_long (*((uint64_t *)&val));
	}
	
	
	
	static bool
	is_chat_code (char c)
	{
		if (std::isxdigit (c))
			return true;
		if (((c >= 'k') && (c <= 'o')) || (c == 'r'))
			return true;
		return false;
	}
	
	static void
	sanitize_string (const char *str, std::string& out)
	{
		out.clear ();
		
		int in_len = std::strlen (str);
		out.reserve (in_len + 1);
		
		int col_start = -1;
		char col_code = 'f';
		int space_count = 0;
		int i;
		for (i = 0; i < in_len; ++i)
			{
				if ((i + 1) < in_len && (str[i] & 0xFF) == 0xC2 &&
					(str[i + 1] & 0xFF) == 0xA7)
					{
						if ((i + 2) < in_len)
							{
								if (!is_chat_code (str[i + 2]))
									//goto not_color_code;
									continue;
								
								col_start = i;
								col_code = str[i + 2];
								i += 2;
							}
						else
							{
								break;
							}
					}
				else
					{
			//not_color_code:
						if (str[i] == ' ')
							{
								++ space_count;
							}
						else
							{
								if (col_start != -1)
									{
										out.append ("§");
										out.push_back (col_code);
										col_start = -1;
									}
								
								while (space_count-- > 0)
										out.push_back (' ');
								space_count = 0;
								out.push_back (str[i]);
							}
					}
			}
	}
	
	
	
	// returns the amount of space the specified string will take once it is put
	// in a packet.
	static int
	mc_str_len (const char *str)
	{
		return 2 + (std::strlen (str) * 2);
	}
	
	static int
	mc_str_len (const std::string& str)
	{
		return 2 + (str.size () * 2);
	}
	
	
	int
	packet::put_string (const char *in, bool sanitize, bool encode_length)
	{
		std::string str;
		
		if (sanitize)
			sanitize_string (in, str);
		else
			str.assign (in);
		
		// don't emit the length of the string, we will do that after we convert
		// the string from UTF-8 to the format used by Minecraft.
		int len_pos = this->pos;
		if (encode_length)
			{
				this->pos += 2;
			}
		
		/* 
		 * Convert UTF-8 to UCS-2:
		 */
		int str_len = str.size ();
		int real_len = 0;
		int i, c, o;
		for (i = 0; i < str_len; ++i)
			{
				c = str[i] & 0xFF;
				if ((c >> 7) == 0)
					{
						this->put_short (c);
						++ real_len;
					}
				else if ((c >> 5) == 0x6)
					{
						o = (c & 0x1F);            // first byte
						c = str[++i] & 0xFF;
						o = (o << 6) | (c & 0x3F); // second byte
						
						this->put_short (o);
						++ real_len;
					}
				else if ((c >> 4) == 0xE)
					{
						o = c & 0xF;               // first byte
						c = str[++i] & 0xFF;
						o = (o << 6) | (c & 0x3F); // second byte
						c = str[++i] & 0xFF;
						o = (o << 6) | (c & 0x3F); // third byte
						
						this->put_short (o);
						++ real_len;
					}
				
				// characters with more than 16 bits of data are not encoded.
			}
		
		// now that we went over the string, we can safely emit its length.
		if (encode_length)
			{
				int cur_pos = this->pos;
				this->pos = len_pos;
				this->put_short (real_len);
				this->pos = cur_pos;
			}
		
		return real_len;
	}
	
	void
	packet::put_bytes (const unsigned char *val, unsigned int len)
	{
		std::memcpy (this->data + this->pos, val, len);
		this->pos += len;
		this->size += len;
	}
	
	
	
	/* 
	 * Resizes the packet.
	 */
	void
	packet::resize (unsigned int new_size)
	{
		unsigned char *d = new unsigned char[new_size];
		int m = utils::max (new_size, this->size);
		std::memcpy (d, this->data, m);
		delete[] this->data;
		this->data = d;
		if (new_size < this->size)
			this->size = new_size;
		if (new_size < this->pos)
			this->pos = new_size;
		this->cap = new_size;
	}
	
	void 
	packet::clear ()
	{
		this->pos = this->size = 0;
	}
	
	
	
	static nbt_tag_compound*
	build_slot_metadata (const slot_item& item)
	{
		nbt_tag_compound *root = new nbt_tag_compound ("");
		
		// display
		if (!item.display_name.empty () || !item.lore.empty ())
			{
				nbt_tag_compound *display = new nbt_tag_compound ("display");
				
				if (!item.display_name.empty ())
					{
						nbt_tag_string *name = new nbt_tag_string ("Name");
						name->set (item.display_name);
						display->add (name);
					}
				
				if (!item.lore.empty ())
					{
						nbt_tag_list *lore = new nbt_tag_list ("Lore", TAG_STRING);
						for (const std::string& str : item.lore)
							{
								nbt_tag_string *t = new nbt_tag_string ("");
								t->set (str);
								lore->add (t);
							}
						display->add (lore);
					}
				
				root->add (display);
			}
		
		// enchantments
		nbt_tag_list *ench_list = new nbt_tag_list ("ench", TAG_COMPOUND);
		for (enchantment ench : item.enchants)
			{
				nbt_tag_compound *t = new nbt_tag_compound ("");
				
				nbt_tag_short *t_id = new nbt_tag_short ("id");
				t_id->value () = ench.eid;
				t->add (t_id);
				
				nbt_tag_short *t_lvl = new nbt_tag_short ("lvl");
				t_lvl->value () = ench.lvl;
				t->add (t_lvl);
				
				ench_list->add (t);
			}
		root->add (ench_list);
		
		return root;
	}
	
	
	
	static bool
	slot_has_metadata (const slot_item& item)
	{
		return (!item.enchants.empty () || !item.display_name.empty ()
			|| !item.lore.empty ());
	}
	
	void
	packet::put_slot (const slot_item& item)
	{
		if (!item.is_valid () || item.amount () == 0)
			this->put_short (-1);
		else
			{
				this->put_short (item.id ());
				this->put_byte ((item.amount () > 64) ? 64 : item.amount ());
				this->put_short (item.damage ());
				
				// enchantments
				if (!slot_has_metadata (item))
					this->put_short (-1);
				else
					{
						nbt_tag_compound *t = build_slot_metadata (item);
						unsigned char *data = new unsigned char [t->size ()];
						int s = t->encode (data);
						delete t;
						
						long comp_size = 0;
						unsigned char *comp = utils::gz_compress (data, s, comp_size);
						delete[] data;
						if (!comp) // shouldn't happen
							this->put_short (-1);
						else
							{
								this->put_short (comp_size);
								this->put_bytes (comp, comp_size);
								delete[] comp;
							}
					}
			}
	}
	
	
	
//----
	
	static uint16_t
	_read_short (const unsigned char *ptr)
	{
		return ((uint16_t)ptr[0] << 8)
				 | ((uint16_t)ptr[1]);
	}
	
	/* 
	 * Checks the specified byte array and determines how many more bytes should
	 * be read in-order to complete reading the packet. Note that this function
	 * might be called numerous times, since packets often contain variable-
	 * length data (such as strings, slot data, etc...). Aside from checking the
	 * remaining amount of bytes left, the function can also be used to check
	 * whether the first byte of the array (the packet's opcode) is associated
	 * with any valid packet (it returns -1 to indicate that it is not).
	 */
	int
	packet::remaining (const unsigned char *data, unsigned int have)
	{
		static const char* rem_table[] =
			{
			/* 
			 * o = opcode
			 * ? = boolean
			 * b = byte
			 * s = short
			 * i = int
			 * l = long
			 * f = float
			 * d = double
			 * z = string
			 * q = slot
			 * a = array
			 */
			
				"oi"         , ""           , "obzzi"      , "oz"         , // 0x03
				""           , ""           , ""           , "oii?"       , // 0x07
				""           , ""           , "o?"         , "odddd?"     , // 0x0B
				"off?"       , "oddddff?"   , "obibib"     , "oibibqbbb"  , // 0x0F
				
				"os"         , ""           , "oib"        , "oibi"       , // 0x13
				""           , ""           , ""           , ""           , // 0x17
				""           , ""           , ""           , ""           , // 0x1B
				""           , ""           , ""           , ""           , // 0x1F
				
				""           , ""           , ""           , ""           , // 0x23
				""           , ""           , ""           , ""           , // 0x27
				""           , ""           , ""           , ""           , // 0x2B
				""           , ""           , ""           , ""           , // 0x2F
				
				""           , ""           , ""           , ""           , // 0x33
				""           , ""           , ""           , ""           , // 0x37
				""           , ""           , ""           , ""           , // 0x3B
				""           , ""           , ""           , ""           , // 0x3F
				
				""           , ""           , ""           , ""           , // 0x43
				""           , ""           , ""           , ""           , // 0x47
				""           , ""           , ""           , ""           , // 0x4B
				""           , ""           , ""           , ""           , // 0x4F
				
				""           , ""           , ""           , ""           , // 0x53
				""           , ""           , ""           , ""           , // 0x57
				""           , ""           , ""           , ""           , // 0x5B
				""           , ""           , ""           , ""           , // 0x5F
				
				""           , ""           , ""           , ""           , // 0x63
				""           , "ob"         , "obsbsbq"    , ""           , // 0x67
				""           , ""           , "obs?"       , "osq"        , // 0x6B
				"obb"        , ""           , ""           , ""           , // 0x6F
				
				""           , ""           , ""           , ""           , // 0x73
				""           , ""           , ""           , ""           , // 0x77
				""           , ""           , ""           , ""           , // 0x7B
				""           , ""           , ""           , ""           , // 0x7F
				
				""           , ""           , "oisizzzz"   , ""           , // 0x83
				""           , ""           , ""           , ""           , // 0x87
				""           , ""           , ""           , ""           , // 0x8B
				""           , ""           , ""           , ""           , // 0x8F
				
				""           , ""           , ""           , ""           , // 0x93
				""           , ""           , ""           , ""           , // 0x97
				""           , ""           , ""           , ""           , // 0x9B
				""           , ""           , ""           , ""           , // 0x9F
				
				""           , ""           , ""           , ""           , // 0xA3
				""           , ""           , ""           , ""           , // 0xA7
				""           , ""           , ""           , ""           , // 0xAB
				""           , ""           , ""           , ""           , // 0xAF
				
				""           , ""           , ""           , ""           , // 0xB3
				""           , ""           , ""           , ""           , // 0xB7
				""           , ""           , ""           , ""           , // 0xBB
				""           , ""           , ""           , ""           , // 0xBF
				
				""           , ""           , ""           , ""           , // 0xC3
				""           , ""           , ""           , ""           , // 0xC7
				""           , ""           , "obff"       , "oz"         , // 0xCB
				"ozbbb?"     , "ob"         , ""           , ""           , // 0xCF
				
				""           , ""           , ""           , ""           , // 0xD3
				""           , ""           , ""           , ""           , // 0xD7
				""           , ""           , ""           , ""           , // 0xDB
				""           , ""           , ""           , ""           , // 0xDF
				
				""           , ""           , ""           , ""           , // 0xE3
				""           , ""           , ""           , ""           , // 0xE7
				""           , ""           , ""           , ""           , // 0xEB
				""           , ""           , ""           , ""           , // 0xEF
				
				""           , ""           , ""           , ""           , // 0xF3
				""           , ""           , ""           , ""           , // 0xF7
				""           , ""           , "oza"        , ""           , // 0xFB
				"oaa"        , ""           , "ob"         , "oz"         , // 0xFF
			};
		
		const char *str = rem_table[(*data) & 0xFF];
		if (!str[0] || str[0] == ' ')
			return -1;
		
		short tmp;
		
		int c;
		unsigned int need = 0;
		while ((c = (int)(*str++)))
			{
				switch (c)
					{
						case 'o': case 'b': case '?': ++ need; break;
						case 's': need += 2; break;
						case 'f': case 'i': need += 4; break;
						case 'd': case 'l': need += 8; break;
						
						// string
						case 'z':
							need += 2;
							if (have < need)
								goto done;
							need += _read_short (data + need - 2) * 2;
							break;
						
						// array (prefixed with 16-bit integer describing length)
						case 'a':
							need += 2;
							if (have < need)
								goto done;
							need += _read_short (data + need - 2);
							break;
						
						// slot data
						case 'q':
							need += 2;
							if (have < need)
								goto done;
							tmp = _read_short (data + need - 2); // id
							
							if (tmp == -1)
								break; // done
							else if (tmp == 0)
								return -1; // shouldn't happen
							
							need += 5;
							if (have < need)
								goto done;
							tmp = _read_short (data + need - 2); // metadata length
							
							if (tmp == -1)
								break; // done
							else if (tmp == 0)
								return -1; // shouldn't happen
							
							need += tmp;
							break;
					}
			}
		
	done:
		return need - have;
	}
	
	
	
//----
	
	/* 
	 * Constructs a new packet reader around the given byte array.
	 */
	packet_reader::packet_reader (const unsigned char *data)
	{
		this->data = data;
		this->pos = 0;
	}
	
	
	
	/* 
	 * read methods:
	 */
	
	uint8_t
	packet_reader::read_byte ()
		{ return this->data[(this->pos) ++]; }
	
	uint16_t
	packet_reader::read_short ()
	{
		return (((uint16_t)read_byte ()) <<  8)
				 | (((uint16_t)read_byte ()));
	}
	
	uint32_t
	packet_reader::read_int ()
	{
		return (((uint32_t)read_byte ()) << 24)
				 | (((uint32_t)read_byte ()) << 16)
				 | (((uint32_t)read_byte ()) <<  8)
				 | (((uint32_t)read_byte ()));
	}
	
	uint64_t
	packet_reader::read_long ()
	{
		return (((uint64_t)read_byte ()) << 56)
				 | (((uint64_t)read_byte ()) << 48)
				 | (((uint64_t)read_byte ()) << 40)
				 | (((uint64_t)read_byte ()) << 32)
				 | (((uint64_t)read_byte ()) << 24)
				 | (((uint64_t)read_byte ()) << 16)
				 | (((uint64_t)read_byte ()) <<  8)
				 | (((uint64_t)read_byte ()));
	}
	
	float
	packet_reader::read_float ()
	{
		uint32_t num = this->read_int ();
		return *((float *)&num);
	}
	
	double 
	packet_reader::read_double ()
	{
		uint64_t num = this->read_long ();
		return *((double *)&num);
	}
	
	int
	packet_reader::read_string (char *out, int max_chars)
	{
		int len, i, c;
		int out_pos = 0;
		
		/* 
		 * Convert from UCS-2 back to UTF-8.
		 */
		len = this->read_short ();
		for (i = 0; i < len; ++i)
			{
				if (i == max_chars)
					{
						out[out_pos] = '\0';
						return -1;
					}
					
				c = this->read_short ();
				if (c < 0x80)
					{
						out[out_pos ++] = c & 0x7F;
					}
				else if (c < 0x800)
					{
						out[out_pos ++] = 0xC0 | (c >> 6);
						out[out_pos ++] = 0x80 | (c & 0x3F);
					}
				else
					{
						out[out_pos ++] = 0xE0 | (c >> 12);
						out[out_pos ++] = 0x80 | ((c >> 6) & 0x3F);
						out[out_pos ++] = 0x80 | (c & 0x3F);
					}
			}
		
		out[out_pos ++] = '\0';
		return len;
	}
	
	slot_item
	packet_reader::read_slot ()
	{
		short id = this->read_short ();
		if (id == -1)
			return slot_item (-1, 0, 0);
		
		char  amount = this->read_byte ();
		short damage = this->read_short ();
		short meta   = this->read_short ();
		if (meta != -1)
			{
				/* TODO */
				// skip the array for now
				this->pos += meta;
			}
		
		return slot_item (id, damage, amount);
	}
	
	void
	packet_reader::read_bytes (unsigned char *out, int len)
	{
		std::memcpy (out, this->data + this->pos, len);
		this->pos += len;
	}
	
	
	
//---
	/* 
	 * Packet creation:
	 */
	
	// calculates the total size of an entity metadata dictionary in bytes.
	static int
	entity_metadata_size (entity_metadata &dict)
	{
		int size = 0;
		
		for (auto& rec : dict)
			{
				++ size; // type + index
				switch (rec.type)
					{
						case EMT_BYTE: ++ size; break;
						case EMT_SHORT: size += 2; break;
						case EMT_FLOAT: case EMT_INT: size += 4; break;
						case EMT_STRING:
							size += mc_str_len (rec.data.str);
							break;
						case EMT_SLOT: size += 7; break;
						case EMT_BLOCK: size += 12; break;
					}
				
				++ size; // for the trailing `127' byte.
			}
		
		return size;
	}
	
	static void
	encode_entity_metadata (packet *pack, entity_metadata &dict)
	{
		for (auto& rec : dict)
			{
				pack->put_byte ((rec.type << 5) | rec.index);
				switch (rec.type)
					{
						case EMT_BYTE: pack->put_byte (rec.data.i8); break;
						case EMT_SHORT: pack->put_short (rec.data.i16); break;
						case EMT_INT: pack->put_int (rec.data.i32); break;
						case EMT_FLOAT: pack->put_float (rec.data.f32); break;
						case EMT_STRING: pack->put_string (rec.data.str); break;
						case EMT_SLOT:
							pack->put_short (rec.data.slot.id);
							if (rec.data.slot.id != -1)
								{
									pack->put_byte (rec.data.slot.count);
									pack->put_short (rec.data.slot.damage);
									pack->put_short (-1); // no extra data yet
								}
							break;
						case EMT_BLOCK:
							pack->put_int (rec.data.block.x);
							pack->put_int (rec.data.block.y);
							pack->put_int (rec.data.block.z);
							break;
					}
			}
		pack->put_byte (0x7F);
	}
	 
	
	static int
	slot_size (const slot_item& item)
	{
		int size = 2;
		if (item.id () != -1)
			{
				size += 7;
			}
		
		// very rough estimation
		bool has_nbt = slot_has_metadata (item);
		if (has_nbt)
			{
				size += 192;
				if (!item.enchants.empty ())
					size += (item.enchants.size () * 8);
				if (!item.display_name.empty ())
					size += item.display_name.size () * 2 + 3;
				if (!item.lore.empty ())
					{
						for (const std::string& str : item.lore)
							size += mc_str_len (str) + 1;
					}
			}
		
		return size;
	}
	
	
	
	packet*
	packet::make_ping (int id)
	{
		packet* pack = new packet (5);
		
		pack->put_byte (0x00);
		pack->put_int (id);
		
		return pack;
	}
	
	packet*
	packet::make_login (int eid, const char *level_type, char game_mode,
		char dimension, char difficulty, unsigned char max_players)
	{
		packet *pack = new packet (12 + mc_str_len (level_type));
		
		pack->put_byte (0x01);
		pack->put_int (eid);
		pack->put_string (level_type);
		pack->put_byte (game_mode);
		pack->put_byte (dimension);
		pack->put_byte (difficulty);
		pack->put_byte (0); // unused
		pack->put_byte (max_players);
		
		return pack;
	}
	
	
	
	static std::string
	escape_string (const char *str)
	{
		std::string res;
		res.reserve (std::strlen (str));
		
		const char *ptr = str;
		while (*ptr)
			{
				char c = *ptr++;
				if (c == '"')
					{
						res.push_back ('\\');
						res.push_back ('"');
					}
				else
					res.push_back (c);
			}
		
		return res;
	}
	
	packet*
	packet::make_message (const char *msg)
	{
		// 1.6 messages...
		std::string s = "{\"text\":\"";
		s.append (escape_string (msg));
		s.append ("\"}");
		
		packet *pack = new packet (3 + mc_str_len (s));
		
		pack->put_byte (0x03);
		pack->put_string (s.c_str ());
		
		return pack;
	}
	
	
	
	packet*
	packet::make_time_update (long long world_age, long long day_time)
	{
		packet *pack = new packet (17);
		
		pack->put_byte (0x04);
		pack->put_long (world_age);
		pack->put_long (day_time);
		
		return pack;
	}
	
	packet*
	packet::make_entity_equipment (int eid, short slot, slot_item item)
	{
		packet *pack = new packet (7 + slot_size (item));
		
		pack->put_byte (0x05);
		pack->put_int (eid);
		pack->put_short (slot);
		pack->put_slot (item);
		
		return pack;
	}
	
	packet*
	packet::make_spawn_pos (int x, int y, int z)
	{
		packet *pack = new packet (13);
		
		pack->put_byte (0x06);
		pack->put_int (x);
		pack->put_int (y);
		pack->put_int (z);
		
		return pack;
	}
	
	packet*
	packet::make_update_health (float hearts, short hunger,
		float hunger_saturation)
	{
		packet *pack = new packet (9);
		
		pack->put_byte (0x08);
		pack->put_float (hearts);
		pack->put_short (hunger);
		pack->put_float (hunger_saturation);
		
		return pack;
	}
	
	packet*
	packet::make_respawn (int dimension, char difficulty, char game_mode,
		const char *level_type)
	{
		packet *pack = new packet (11 + mc_str_len (level_type));
		
		pack->put_byte (0x09);
		pack->put_int (dimension);
		pack->put_byte (difficulty);
		pack->put_byte (game_mode);
		pack->put_short (256);
		pack->put_string (level_type);
		
		return pack;
	}
	
	packet*
	packet::make_player_pos_and_look (double x, double y, double z,
		double stance, float r, float l, bool on_ground)
	{
		packet *pack = new packet (42);
		
		pack->put_byte (0x0D);
		pack->put_double (x);
		pack->put_double (stance);
		pack->put_double (y);
		pack->put_double (z);
		pack->put_float (r);
		pack->put_float (l);
		pack->put_bool (on_ground);
		
		return pack;
	}
	
	packet*
	packet::make_animation (int eid, char animation)
	{
		packet *pack = new packet (6);
		
		pack->put_byte (0x12);
		pack->put_int (eid);
		pack->put_byte (animation);
		
		return pack;
	}
	
	packet*
	packet::make_spawn_named_entity (int eid, const char *name, double x,
		double y, double z, float r, float l, short current_item,
		entity_metadata& meta)
	{
		packet* pack = new packet (22 + mc_str_len (name)
			+ entity_metadata_size (meta));
		
		pack->put_byte (0x14);
		pack->put_int (eid);
		pack->put_string (name);
		pack->put_int ((int)(x * 32.0));
		pack->put_int ((int)(y * 32.0));
		pack->put_int ((int)(z * 32.0));
		pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
		pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
		pack->put_short (current_item);
		encode_entity_metadata (pack, meta);
		
		return pack;
	}
	
	packet*
	packet::make_collect_item (int collected_eid, int collector_eid)
	{
		packet* pack = new packet (9);
		
		pack->put_byte (0x16);
		pack->put_int (collected_eid);
		pack->put_int (collector_eid);
		
		return pack;
	}
	
	packet*
	packet::make_spawn_object (int eid, char type, double x, double y, double z,
		float r, float l, int data, short speed_x, short speed_y,
		short speed_z)
	{
		packet* pack = new packet ((data == 0) ? 24 : 30);
		
		pack->put_byte (0x17);
		pack->put_int (eid);
		pack->put_byte (type);
		pack->put_int ((int)(x * 32.0));
		pack->put_int ((int)(y * 32.0));
		pack->put_int ((int)(z * 32.0));
		pack->put_byte (utils::int_rot (r));
		pack->put_byte (utils::int_rot (l));
		pack->put_int (data);
		if (data != 0)
			{
				pack->put_short (speed_x);
				pack->put_short (speed_y);
				pack->put_short (speed_z);
			}
		
		return pack;
	}
	
	packet*
	packet::make_spawn_mob (int eid, char type, double x, double y,
		double z, float r, float l, float hl, short vx, short vy,
		short vz, entity_metadata& meta)
	{
		packet *pack = new packet (27 + entity_metadata_size (meta));
		
		pack->put_byte (0x18);
		pack->put_int (eid);
		pack->put_byte (type);
		pack->put_int ((int)(x * 32.0));
		pack->put_int ((int)(y * 32.0));
		pack->put_int ((int)(z * 32.0));
		pack->put_byte (utils::int_rot (l));
		pack->put_byte (utils::int_rot (hl));
		pack->put_byte (utils::int_rot (r));
		pack->put_short (vx);
		pack->put_short (vy);
		pack->put_short (vz);
		encode_entity_metadata (pack, meta);
		
		return pack;
	}
	
	packet*
	packet::make_entity_velocity (int eid, short vx, short vy, short vz)
	{
		packet *pack = new packet (11);
		
		pack->put_byte (0x1C);
		pack->put_int (eid);
		pack->put_short (vx);
		pack->put_short (vy);
		pack->put_short (vz);
		
		return pack;
	}
	
	packet*
	packet::make_destroy_entity (int eid)
	{
		packet* pack = new packet (6);
		
		pack->put_byte (0x1D);
		pack->put_byte (1); // entity count
		pack->put_int (eid);
		
		return pack;
	}
	
	packet*
	packet::make_entity_relative_move (int eid, char dx, char dy, char dz)
	{
		packet *pack = new packet (8);
		
		pack->put_byte (0x1F);
		pack->put_int (eid);
		pack->put_byte (dx);
		pack->put_byte (dy);
		pack->put_byte (dz);
		
		return pack;
	}
	
	packet*
	packet::make_entity_look (int eid, float r, float l)
	{
		packet *pack = new packet (7);
		
		pack->put_byte (0x20);
		pack->put_int (eid);
		pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
		pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
		
		return pack;
	}
	
	packet*
	packet::make_entity_look_and_move (int eid, char dx, char dy, char dz,
		float r, float l)
	{
		packet* pack = new packet (10);
		
		pack->put_byte (0x21);
		pack->put_int (eid);
		pack->put_byte (dx);
		pack->put_byte (dy);
		pack->put_byte (dz);
		pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
		pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
		
		return pack;
	}
	
	packet*
	packet::make_entity_head_look (int eid, float yaw)
	{
		packet* pack = new packet (6);
		
		pack->put_byte (0x23);
		pack->put_int (eid);
		pack->put_byte ((unsigned char)(std::fmod (std::floor (yaw), 360.0f) / 360.0 * 256.0));
		
		return pack;
	}
	
	packet*
	packet::make_entity_status (int eid, char status)
	{
		packet* pack = new packet (6);
		
		pack->put_byte (0x26);
		pack->put_int (eid);
		pack->put_byte (status);
		
		return pack;
	}
	
	packet*
	packet::make_entity_teleport (int eid, int x, int y, int z, float r, float l)
	{
		packet* pack = new packet (19);
		
		pack->put_byte (0x22);
		pack->put_int (eid);
		pack->put_int (x);
		pack->put_int (y);
		pack->put_int (z);
		pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
		pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
		
		return pack;
	}
	
	packet*
	packet::make_entity_metadata (int eid, entity_metadata& meta)
	{
		packet *pack = new packet (5 + entity_metadata_size (meta));
		
		pack->put_byte (0x28);
		pack->put_int (eid);
		encode_entity_metadata (pack, meta);
		
		return pack;
	}
	
	packet*
	packet::make_entity_properties (int eid,
		const std::vector<entity_property>& props)
	{
		int pack_size = 9;
		for (entity_property prop : props)
			pack_size += 12 + mc_str_len (prop.key);
		
		packet *pack = new packet (pack_size);
		
		pack->put_byte (0x2C);
		pack->put_int (eid);
		pack->put_int (props.size ());
		for (entity_property prop : props)
			{
				pack->put_string (prop.key);
				pack->put_double (prop.value);
				
				// list of weird things
				pack->put_short (0);
			}
		
		return pack;
	}
	
	packet*
	packet::make_chunk (int x, int z, chunk *ch)
	{
		int data_size = 0, n = 0, i;
		unsigned short primary_bitmap = 0, add_bitmap = 0;
		int primary_count = 0;
		
		// create bitmaps and calculate the size of the uncompressed data array.
		data_size += 256; // biome array
		for (i = 0; i < 16; ++i)
			{
				subchunk *sub = ch->get_sub (i);
				if (sub && !sub->all_air ())
					{
						primary_bitmap |= (1 << i);
						++ primary_count;
						data_size += 10240;
						
						//// NOTE: Do not send add values for now... or else it cause custom
						//// blocks to screw up.
						//if (sub->has_add ())
						//	{ add_bitmap |= (1 << i); data_size += 2048; }
					}
			}
		
		unsigned char *data = new unsigned char[data_size];
		
		// fill the array.
		
		/* 
		 * We do IDs and metadata values at the same time.
		 */
		int ind, hlf;
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{
					// we take into account that the ID array might contain custom IDs -
					// ID values that the vanilla client does NOT recognize. So we replace
					// them with the their suitable equivalents.
					
					subchunk *sub = ch->get_sub (i);
					unsigned char *ids = sub->ids;
					unsigned char *metas = sub->meta;
					unsigned int *customs = sub->custom;
					for (int i = 0; i < 128; ++i)
						{
							if (customs[i] == 0)
								{
									std::memcpy (data + n, ids + (i << 5), 32);
									std::memcpy (data + (primary_count << 12) + (n >> 1), metas + (i << 4), 16);
									n += 32;
								}
							else
								{
									int e = (i << 5) + 32; int id;
									for (int j = (i << 5); j < e; ++j)
										{
											// extract full id
											id = sub->ids[j];
											if (sub->add)
												{
													if (j & 1)
														id |= (sub->add[j >> 1] >> 4) << 8;
													else
														id |= (sub->add[j >> 1] & 0xF) << 8;
												}
											
											ind = n;
											hlf = ind >> 1;
											
											if (block_info::is_vanilla_id (id))
												{
													data[n] = id & 0xFF;
													if (ind & 1)
														{ data[(primary_count << 12) + hlf] &= 0x0F; data[(primary_count << 12) + hlf] |= (metas[hlf] & 0xF0); }
													else
														{ data[(primary_count << 12) + hlf] &= 0xF0; data[(primary_count << 12) + hlf] |= (metas[hlf] & 0x0F); }
													++ n;
												}
											else
												{
													physics_block *ph = physics_block::from_id (id);
													if (ph)
														{
															blocki vn = ph->vanilla_block ();
															data[n] = vn.id & 0xFF;
															if (ind & 1)
																{ data[(primary_count << 12) + hlf] &= 0x0F; data[(primary_count << 12) + hlf] |= (vn.meta << 4); }
															else
																{ data[(primary_count << 12) + hlf] &= 0xF0; data[(primary_count << 12) + hlf] |= vn.meta; }
															++ n;
														}
													else
														{
															data[n] = 0;
															if (ind & 1)
																data[(primary_count << 12) + hlf] &= 0x0F;
															else
																data[(primary_count << 12) + hlf] &= 0xF0;
															++ n;
														}
												}
										}
								}
						}
				}
		n += primary_count * 2048; // account for metadata
		
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->blight, 2048);
					n += 2048; }
		
		for (i = 0; i < 16; ++i)
			if (primary_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->slight, 2048);
					n += 2048; }
		
		for (i = 0; i < 16; ++i)
			if (add_bitmap & (1 << i))
				{ std::memcpy (data + n, ch->get_sub (i)->add, 2048);
					n += 2048; }
		
		std::memcpy (data + n, ch->get_biome_array (), 256);
		n += 256;
		
		// compress.
		unsigned long compressed_size = compressBound (data_size);
		unsigned char *compressed = new unsigned char[compressed_size];
		if (compress2 (compressed, &compressed_size, data, data_size,
			Z_BEST_COMPRESSION) != Z_OK)
			{
				delete[] compressed;
				delete[] data;
				return nullptr;
			}
		
		delete[] data;
		
		// and finally, create the packet.
		packet* pack = new packet (18 + compressed_size);
		
		pack->put_byte (0x33);
		pack->put_int (x);
		pack->put_int (z);
		pack->put_bool (true); // ground-up continuous
		pack->put_short (primary_bitmap);
		pack->put_short (add_bitmap);
		pack->put_int (compressed_size);
		pack->put_bytes (compressed, compressed_size);
		delete[] compressed;
		
		return pack;
	}
	
	packet*
	packet::make_empty_chunk (int x, int z)
	{
		static const unsigned char unload_sequence[] =
			{ 0x78, 0x9C, 0x63, 0x64, 0x1C, 0xD9, 0x00, 0x00, 0x81, 0x80, 0x01, 0x01 };
		
		packet *pack = new packet (18 + (sizeof unload_sequence));
		
		pack->put_byte (0x33);
		pack->put_int (x);
		pack->put_int (z);
		pack->put_bool (true); // ground-up continuous
		pack->put_short (0); // primary bitmap
		pack->put_short (0); // add bitmap
		pack->put_int (sizeof unload_sequence);
		pack->put_bytes (unload_sequence, sizeof unload_sequence);
		
		return pack;
	}
	
	packet*
	packet::make_multi_block_change (int cx, int cz,
		const std::vector<block_change_record>& records, player *sb)
	{
		packet* pack = new packet (15 + (records.size () * 4));
		int srec = utils::min (records.size (), 65535);
		
		pack->put_byte (0x34);
		pack->put_int (cx);
		pack->put_int (cz);
		pack->put_short (srec);
		
		int spos = pack->pos;
		pack->pos += 4;
		
		if (sb)
			sb->sb_lock.lock ();
		
		int elems = srec;
		for (int i = 0; i < srec; ++i)
			{
				block_change_record rec = records[i];
				if (sb && (sb->sb_exists_nolock ((cx << 4) | rec.x, rec.y, (cz << 4) | rec.z)))
					{ -- elems; continue; }
				
				int id = rec.id;
				int meta = rec.meta;
				if (!block_info::is_vanilla_id (id))
					{
						physics_block *ph = physics_block::from_id (id);
						if (ph)
							{
								id = ph->vanilla_block ().id;
								meta = ph->vanilla_block ().meta;
							}
						else
							id = meta = 0;
					}
				
				pack->put_byte ((rec.x << 4) | rec.z);
				pack->put_byte (rec.y);
				pack->put_byte (id >> 4);
				pack->put_byte (((id & 0xF) << 4) | meta);
			}
		
		if (sb)
			sb->sb_lock.unlock ();
		
		int npos = pack->pos;
		pack->pos = spos;
		pack->put_int (elems * 4);
		pack->pos = npos;
		
		return pack;
	}
	
	packet*
	packet::make_block_change (int x, unsigned char y, int z,
		unsigned short id, unsigned char meta)
	{
		packet* pack = new packet (13);
		
		int new_id = id;
		int new_meta = meta;
		if (!block_info::is_vanilla_id (new_id))
			{
				physics_block *ph = physics_block::from_id (new_id);
				if (ph)
					{
						new_id = ph->vanilla_block ().id;
						new_meta = ph->vanilla_block ().meta;
					}
				else
					new_id = new_meta = 0;
			}
		
		pack->put_byte (0x35);
		pack->put_int (x);
		pack->put_byte (y);
		pack->put_int (z);
		pack->put_short (new_id);
		pack->put_byte (meta);
		
		return pack;
	}
	
	packet*
	packet::make_named_sound_effect (const char *sound, double x, double y, double z,
		float volume, unsigned char pitch)
	{
		packet* pack = new packet (20 + mc_str_len (sound));
		
		pack->put_byte (0x3E);
		pack->put_string (sound);
		pack->put_int (x * 8.0);
		pack->put_int (y * 8.0);
		pack->put_int (z * 8.0);
		pack->put_float (volume);
		pack->put_byte (pitch);
		
		return pack;
	}
	
	packet*
	packet::make_change_game_state (char reason, char gm)
	{
		packet *pack = new packet (3);
		
		pack->put_byte (0x46);
		pack->put_byte (reason);
		pack->put_byte (gm);
		
		return pack;
	}
	
	packet*
	packet::make_set_slot (char wid, short slot, const slot_item& item)
	{
		packet *pack = new packet (4 + slot_size (item));
		
		/* 
		 * TODO: Item metadata.
		 */
		
		pack->put_byte (0x67);
		pack->put_byte (wid);
		pack->put_short (slot);
		pack->put_slot (item);
		
		return pack;
	}
	
	packet*
	packet::make_set_window_items (char wid, const std::vector<slot_item>& slots)
	{
		/* 
		 * TODO: Item metadata.
		 */
		
		packet *pack = new packet (4 + (slots.size () * 7));
		
		pack->put_byte (0x68);
		pack->put_byte (wid);
		pack->put_short (slots.size ());
		for (const slot_item& item : slots)
			pack->put_slot (item);
		
		return pack;
	}
	
	packet*
	packet::make_player_list_item (const char *name, bool online,
		short ping_ms)
	{
		packet *pack = new packet (6 + mc_str_len (name));
		
		pack->put_byte (0xC9);
		pack->put_string (name);
		pack->put_bool (online);
		pack->put_short (ping_ms);
		
		return pack;
	}
	
	packet*
	packet::make_empty_encryption_key_response ()
	{
		packet *pack = new packet (5);
		
		pack->put_byte (0xFC);
		pack->put_short (0);
		pack->put_short (0);
		
		return pack;
	}
	
	packet*
	packet::make_encryption_key_request (const std::string& sid,
		CryptoPP::RSA::PublicKey& pkey, unsigned char vtoken[4])
	{
		CryptoPP::ByteQueue q;
		pkey.Save (q);
		
		unsigned char buf[384];
		size_t keylen = q.Get (buf, sizeof buf);
		
		packet *pack = new packet (7 + mc_str_len (sid) + keylen + 4);
		
		pack->put_byte (0xFD);
		pack->put_string (sid.c_str ());
		pack->put_short (keylen);
		pack->put_bytes (buf, keylen);
		pack->put_short (4);
		pack->put_bytes (vtoken, 4);
		
		return pack;
	}
	
	packet*
	packet::make_kick (const char *str)
	{
		packet *pack = new packet (3 + mc_str_len (str));
		
		pack->put_byte (0xFF);
		pack->put_string (str);
		
		return pack;
	}
	
	packet*
	packet::make_ping_kick (const char *motd, int player_count, int max_players)
	{
		packet *pack = new packet (mc_str_len (motd)
			+ ((std::log10 (player_count + 1) + 1) * 2)
			+ ((std::log10 (max_players + 1) + 1) * 2)
			+ 64);
		
		std::ostringstream ss;
		
		ss << player_count;
		std::string player_count_str {ss.str ()};
		
		ss.clear (); ss.str (std::string ());
		ss << max_players;
		std::string max_players_str {ss.str ()};
		
		pack->put_byte (0xFF);
		
		// we encode length later
		int str_len_pos = pack->pos;
		pack->pos += 2;
		
		pack->put_string ("§1", false, false);
		
		pack->put_short (0); // delimiter
		
		ss.clear (); ss.str (std::string ());
		ss << packet::protocol_version;
		pack->put_string (ss.str ().c_str (), false, false);
		
		pack->put_short (0); // delimiter
		pack->put_string (packet::game_version, false, false);
		
		pack->put_short (0); // delimiter
		pack->put_string (motd, true, false);
		
		pack->put_short (0); // delimiter
		pack->put_string (player_count_str.c_str (), false, false);
		
		pack->put_short (0); // delimiter
		pack->put_string (max_players_str.c_str (), false, false);
		
		// encode length
		int curr_pos = pack->pos;
		int str_len  = (curr_pos - (str_len_pos + 2)) / 2;
		pack->pos = str_len_pos;
		pack->put_short (str_len);
		pack->pos = curr_pos;
		
		return pack;
	}
}

