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

#include "system/packet.hpp"
#include "world/chunk.hpp"
#include "entities/entity.hpp"
#include "util/utils.hpp"
#include "util/nbt.hpp"
#include "player/player.hpp"
#include "drawing/editstage.hpp"
#include "util/wordwrap.hpp"
#include <cstring>
#include <zlib.h>
#include <cmath>
#include <sstream>
#include <string>

#include <cryptopp/queue.h>

#include <iostream> // DEBUG
#include <iomanip> // DEBUG


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
	packet::put_varint (uint32_t val)
	{
		if (val == 0)
			{
				this->put_byte (0);
				return;
			}
			
		int i;
		for (i = 0; val; ++i)
			{
				unsigned char b = val & 0x7F;
				val >>= 7;
				if (val)
					b |= 0x80;
				
				this->data[this->pos + i] = b;
			}
		
		this->pos += i;
		this->size += i;
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
	
	
	
	static void
	sanitize_string (const char *str, std::string& out)
	{
		out.clear ();
		
		int in_len = std::strlen (str);
		out.reserve (in_len + 1);
		
		bool got_col = false;
		char col_code = 'f';
		bool got_stl = false;
		char stl_code = 'r';
		
		int space_count = 0;
		int i;
		for (i = 0; i < in_len; ++i)
			{
				if ((i + 1) < in_len && (str[i] & 0xFF) == 0xC2 &&
					(str[i + 1] & 0xFF) == 0xA7)
					{
						if ((i + 2) < in_len)
							{
								if (is_color_code (str[i + 2]))
									{
										got_col = true;
										col_code = str[i + 2];
										i += 2;
									}
								else if (is_style_code (str[i + 2]))
									{
										got_stl = true;
										stl_code = str[i + 2];
										i += 2;
									}
								else
									continue;
							}
						else
							{
								break;
							}
					}
				else
					{
						if (str[i] == ' ')
							{
								++ space_count;
							}
						else
							{
								if (got_stl && stl_code == 'r')
									{
										out.append ("§");
										out.push_back (stl_code);
										got_stl = false;
									}
								
								if (got_col)
									{
										out.append ("§");
										out.push_back (col_code);
										got_col = false;
									}
								
								if (got_stl)
									{
										out.append ("§");
										out.push_back (stl_code);
										got_stl = false;
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
	mc_str_len (const char *str, bool sanitized = true)
	{
		int len = 0;
		if (sanitized)
			{
				std::string out;
				sanitize_string (str, out);
				len = out.length ();
			}
		else
			len = std::strlen (str);
		
		if (len > 0x7F)
			return 2 + len;
		return 1 + len;
	}
	
	static int
	mc_str_len (const std::string& str, bool sanitized = true)
	{
		return mc_str_len (str.c_str (), sanitized);
	}
	
	
	int
	packet::put_string (const char *in, bool sanitize, bool encode_length)
	{
		std::string str;
		
		if (sanitize)
			sanitize_string (in, str);
		else
			str.assign (in);
		
		if (encode_length)
			this->put_varint (str.size ());
		for (char c : str)
			this->put_byte (c);
		
		return str.size ();
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
		unsigned int need = 0, varint_size = 0;
		for (;;)
			{
				++ need;
				if (have < need)
					return need - have;
				
				++ varint_size;
				if (!(data[need - 1] & 0x80))
					break; // end of varint
				else if (varint_size > 4)
					return -1;
			}
		
		unsigned int packet_size = 0;
		for (int i = varint_size - 1; i >= 0; --i)
			{
				packet_size <<= 7;
				packet_size |= (data[i] & 0x7F);
			}
		
		packet_size += varint_size;
		return packet_size - have;
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
	
	uint32_t
	packet_reader::read_varint ()
	{
		unsigned char data[4];
		
		unsigned int size = 0;
		do
			{
				if (size == 4)
					return -1;
				data[size++] = this->read_byte ();
			}
		while (data[size - 1] & 0x80);
		
		unsigned int num = 0;
		for (int i = size - 1; i >= 0; --i)
			{
				num <<= 7;
				num |= (data[i] & 0x7F);
			}
		
		return num;
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
		int len = this->read_varint ();
		if (len >= max_chars)
			return -1;
		
		for (int i = 0; i < len; ++i)
			out[i] = this->read_byte ();
		out[len] = '\0';
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
						case EMT_SLOT:
							size += 2;
							if (rec.data.slot.id != -1)
								size += 5;
							break;
						
						case EMT_BLOCK: size += 12; break;
					}
				
			}
		
		++ size; // for the trailing `127' byte.
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
		if (item.is_valid () && item.amount () > 0)
			{
				size += 5;
				
				if (slot_has_metadata (item))
					{
						// somewhat inefficient.. :X
						nbt_tag_compound *t = build_slot_metadata (item);
						unsigned char *data = new unsigned char [t->size ()];
						int s = t->encode (data);
						delete t;
		
						long comp_size = 0;
						unsigned char *comp = utils::gz_compress (data, s, comp_size);
						delete[] data;
						if (comp)
							size += comp_size;
					}
			}
		
		return size;
	}
	
	
	static inline int
	varint_size (int num)
	{
		if (num > 0x7F7F7F)
			return 4;
		else if (num > 0x7F7F)
			return 3;
		else if (num> 0x7F)
			return 2;
		return 1;
	}
	
	namespace packets {
		
		namespace play {
		
			packet*
			make_keep_alive (int id)
			{
				packet *pack = new packet (6);
				
				pack->put_varint (5);
				pack->put_byte (0x00);
				pack->put_int (id);
				
				return pack;
			}
			
			packet*
			make_join_game (int eid, int gm, int dim, int diff, int max_players,
				const char *level_type)
			{
				int level_type_len = mc_str_len (level_type);
				packet *pack = new packet (11 + level_type_len);
				
				pack->put_varint (9 + level_type_len);
				pack->put_varint (0x01);
				pack->put_int (eid);
				pack->put_byte (gm);
				pack->put_byte (dim);
				pack->put_byte (diff);
				pack->put_byte (max_players);
				pack->put_string (level_type);
				
				return pack;
			}
			
			packet*
			make_chat_message (const char *js)
			{
				int js_len = mc_str_len (js);
				packet *pack = new packet (3 + js_len);
				
				pack->put_varint (1 + js_len);
				pack->put_varint (0x02);
				pack->put_string (js);
				
				return pack;
			}
			
			packet*
			make_time_update (long long world_age, long long time)
			{
				packet *pack = new packet (18);
				
				pack->put_varint (17);
				pack->put_varint (0x03);
				pack->put_long (world_age);
				pack->put_long (time);
				
				return pack;
			}
			
			packet*
			make_entity_equipment (int eid, short slot, const slot_item& item)
			{
				int item_size = slot_size (item);
				packet *pack = new packet (9 + item_size);
				
				pack->put_varint (7 + item_size);
				pack->put_varint (0x04);
				pack->put_int (eid);
				pack->put_short (slot);
				pack->put_slot (item);
				
				return pack;
			}
			
			packet*
			make_spawn_position (int x, int y, int z)
			{
				packet *pack = new packet (14);
				
				pack->put_varint (13);
				pack->put_varint (0x05);
				pack->put_int (x);
				pack->put_int (y);
				pack->put_int (z);
				
				return pack;
			}
			
			packet*
			make_update_health (float health, short food, float sat)
			{
				packet *pack = new packet (12);
				
				pack->put_varint (11);
				pack->put_varint (0x06);
				pack->put_float (health);
				pack->put_short (food);
				pack->put_float (sat);
				
				return pack;
			}
			
			packet*
			make_respawn (int dim, int diff, int gm, const char *level_type)
			{
				int level_type_len = mc_str_len (level_type);
				packet *pack = new packet (9 + level_type_len);
				
				pack->put_varint (7 + level_type_len);
				pack->put_varint (0x07);
				pack->put_int (dim);
				pack->put_byte (diff);
				pack->put_byte (gm);
				pack->put_string (level_type);
				
				return pack;
			}
			
			packet*
			make_player_pos_and_look (double x, double y, double z, float r, float l,
				bool on_ground)
			{
				packet *pack = new packet (35);
				
				pack->put_varint (34);
				pack->put_varint (0x08);
				pack->put_double (x);
				pack->put_double (y + 2.0);
				pack->put_double (z);
				pack->put_float (r);
				pack->put_float (l);
				pack->put_bool (on_ground);
				
				return pack;
			}
			
			packet*
			make_held_item_change (int slot)
			{
				packet *pack = new packet (4);
				
				pack->put_varint (3);
				pack->put_varint (0x09);
				pack->put_short (slot);
				
				return pack;
			}
			
			packet*
			make_animation (int eid, int anim)
			{
				int eid_size = varint_size (eid);
				packet *pack = new packet (3 + eid_size);
				
				pack->put_varint (2 + eid_size);
				pack->put_varint (0x0B);
				pack->put_varint (eid);
				pack->put_byte (anim);
				
				return pack;
			}
			
			packet*
			make_spawn_player (int eid, const char *uuid, const char *username,
				double x, double y, double z, float r, float l, short item,
				entity_metadata& meta)
			{
				int name_len = mc_str_len (username);
				int uuid_len = mc_str_len (uuid);
				int meta_size = entity_metadata_size (meta);
				int eid_size = varint_size (eid);
				int datacount_size = varint_size (0);
				packet* pack = new packet (19 + datacount_size + name_len + meta_size + eid_size + uuid_len);
				
				pack->put_varint (17 + datacount_size + eid_size + name_len + uuid_len + meta_size);
				pack->put_varint (0x0C);
				pack->put_varint (eid);
				pack->put_string (uuid);
				pack->put_string (username);
				
				// TODO: handle data
				pack->put_varint (0); // data count
				
				pack->put_int ((int)(x * 32.0));
				pack->put_int ((int)(y * 32.0));
				pack->put_int ((int)(z * 32.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
				pack->put_short (item);
				encode_entity_metadata (pack, meta);
				
				return pack;
			}
			
			packet*
			make_collect_item (int collected_eid, int collector_eid)
			{
				packet *pack = new packet (10);
				
				pack->put_varint (9);
				pack->put_varint (0x0D);
				pack->put_int (collected_eid);
				pack->put_int (collector_eid);
				
				return pack;
			}
			
			packet*
			make_spawn_object (int eid, int type, double x, double y, double z,
				float r, float l, int data, short speed_x, short speed_y, short speed_z)
			{
				int eid_size = varint_size (eid);
				int data_size = (data == 0) ? 4 : 10;
				packet *pack = new packet (18 + eid_size + data_size);
				
				pack->put_varint (16 + eid_size + data_size);
				pack->put_varint (0x0E);
				pack->put_varint (eid);
				pack->put_byte (type);
				pack->put_int ((int)(x * 32.0));
				pack->put_int ((int)(y * 32.0));
				pack->put_int ((int)(z * 32.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
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
			make_entity_velocity (int eid, short vx, short vy, short vz)
			{
				packet *pack = new packet (12);
				
				pack->put_varint (11);
				pack->put_varint (0x12);
				pack->put_int (eid);
				pack->put_short (vx);
				pack->put_short (vy);
				pack->put_short (vz);
				
				return pack;
			}
			
			packet*
			make_destroy_entities (const std::vector<int>& eids)
			{
				packet *pack = new packet (4 + 4 * eids.size ());
				
				pack->put_varint (2 + eids.size () * 4);
				pack->put_varint (0x13);
				pack->put_byte (eids.size ());
				for (int eid : eids)
					pack->put_int (eid);
				
				return pack;
			}
			
			packet*
			make_destroy_entity (int eid)
			{
				std::vector<int> eids (1, eid);
				return packets::play::make_destroy_entities (eids);
			}
			
			packet*
			make_entity (int eid)
			{
				packet *pack = new packet (6);
				
				pack->put_varint (1);
				pack->put_varint (0x14);
				pack->put_int (eid);
				
				return pack;
			}
			
			packet*
			make_entity_rel_move (int eid, double dx, double dy, double dz)
			{
				packet *pack = new packet (9);
				
				pack->put_varint (8);
				pack->put_varint (0x15);
				pack->put_int (eid);
				pack->put_byte ((int)(dx * 32.0));
				pack->put_byte ((int)(dy * 32.0));
				pack->put_byte ((int)(dz * 32.0));
				
				return pack;
			}
			
			packet*
			make_entity_look (int eid, float r, float l)
			{
				packet *pack = new packet (8);
				
				pack->put_varint (7);
				pack->put_varint (0x16);
				pack->put_int (eid);
				pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
				
				return pack;
			}
			
			packet*
			make_entity_look_and_rel_move (int eid, double dx, double dy, double dz,
				float r, float l)
			{
				packet *pack = new packet (11);
				
				pack->put_varint (10);
				pack->put_varint (0x17);
				pack->put_int (eid);
				pack->put_byte ((int)(dx * 32.0));
				pack->put_byte ((int)(dy * 32.0));
				pack->put_byte ((int)(dz * 32.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
				
				return pack;
			}
			
			packet*
			make_entity_move (int eid, double x, double y, double z, float r, float l)
			{
				packet *pack = new packet (20);
				
				pack->put_varint (19);
				pack->put_varint (0x18);
				pack->put_int (eid);
				pack->put_int ((int)(x * 32.0));
				pack->put_int ((int)(y * 32.0));
				pack->put_int ((int)(z * 32.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
				pack->put_byte ((unsigned char)(std::fmod (std::floor (l), 360.0f) / 360.0 * 256.0));
				
				return pack;
			}
			
			packet*
			make_entity_head_look (int eid, float r)
			{
				packet *pack = new packet (7);
				
				pack->put_varint (6);
				pack->put_varint (0x19);
				pack->put_int (eid);
				pack->put_byte ((unsigned char)(std::fmod (std::floor (r), 360.0f) / 360.0 * 256.0));
				
				return pack;
			}
			
			packet*
			make_entity_status (int eid, int status)
			{
				packet *pack = new packet (7);
				
				pack->put_varint (6);
				pack->put_varint (0x1A);
				pack->put_int (eid);
				pack->put_byte (status);
				
				return pack;
			}
			
			packet*
			make_entity_metadata (int eid, entity_metadata& meta)
			{
				int meta_size = entity_metadata_size (meta);
				packet *pack = new packet (7 + meta_size);
				
				pack->put_varint (5 + meta_size);
				pack->put_varint (0x1C);
				pack->put_int (eid);
				encode_entity_metadata (pack, meta);
				
				return pack;
			}
			
			packet*
			make_entity_properties (int eid,
				const std::vector<entity_property>& props)
			{
				int pack_size = 9;
				for (entity_property prop : props)
					pack_size += 10 + mc_str_len (prop.key);
		
				packet *pack = new packet (2 + pack_size);
				
				pack->put_varint (pack_size);
				pack->put_varint (0x20);
				pack->put_int (eid);
				pack->put_int (props.size ());
				for (entity_property prop : props)
					{
						pack->put_string (prop.key);
						pack->put_double (prop.value);
				
						// list of modifier data structures
						pack->put_short (0);
					}
		
				return pack;
			}
			
			
			
			packet*
			make_chunk (int x, int z, chunk *och, const std::vector<edit_stage *> es_vec)
			{
				chunk *ch = och;
				for (edit_stage *es : es_vec)
					if (es->mod_count_at (x, z) > 0)
						{
							ch = och->duplicate ();
							for (edit_stage *es : es_vec)
								es->commit_chunk (ch, x, z);
							break;
						}
				
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
						
								//// NOTE: Do not send add values for now... or else it will cause custom
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
																{ data[(primary_count << 12) + hlf] &= 0x0F; data[(primary_count << 12) + hlf] |= (metas[j >> 1] & 0xF0); }
															else
																{ data[(primary_count << 12) + hlf] &= 0xF0; data[(primary_count << 12) + hlf] |= (metas[j >> 1] & 0x0F); }
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
				if (ch != och)
					delete ch;
				
				// and finally, create the packet.
				packet* pack = new packet (20 + compressed_size);
				
				pack->put_varint (18 + compressed_size);
				pack->put_varint (0x21);
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
			make_chunk (int x, int z, chunk *ch)
			{
				std::vector<edit_stage *> vec;
				return packets::play::make_chunk (x, z, ch, vec);
			}
			
			packet*
			make_empty_chunk (int x, int z)
			{
				static const unsigned char unload_sequence[] =
					{ 0x78, 0x9C, 0x63, 0x64, 0x1C, 0xD9, 0x00, 0x00, 0x81, 0x80, 0x01, 0x01 };
		
				packet *pack = new packet (20 + (sizeof unload_sequence));
				
				pack->put_varint (18 + (sizeof unload_sequence));
				pack->put_byte (0x21);
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
			make_multi_block_change (int cx, int cz,
				const std::vector<block_change_record>& records, player *sb)
			{
				packet* pack = new packet (17 + (records.size () * 4));
				
				if (sb)
					sb->sb_lock.lock ();
				
				int srec = utils::min (records.size (), 65535);
				std::vector<block_change_record> good_records;
				for (int i = 0; i < srec; ++i)
					{
						block_change_record rec = records[i];
						if (!(sb && (sb->sb_exists_nolock ((cx << 4) | rec.x, rec.y, (cz << 4) | rec.z))))
							good_records.push_back (rec);
					}
				
				pack->put_varint (15 + (good_records.size () * 4));
				pack->put_varint (0x22);
				pack->put_int (cx);
				pack->put_int (cz);
				pack->put_short (good_records.size ());
				pack->put_int (4 * good_records.size ());
				
				for (block_change_record rec : good_records)
					{
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
		
				return pack;
			}
			
			packet*
			make_block_change (int x, int y, int z, unsigned short id,
				unsigned char meta)
			{
				packet* pack = new packet (15);
				
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
				
				int id_size = varint_size (new_id);
				pack->put_varint (11 + id_size);
				pack->put_varint (0x23);
				pack->put_int (x);
				pack->put_byte (y);
				pack->put_int (z);
				pack->put_varint (new_id);
				pack->put_byte (meta);
				
				return pack;
			}
			
			packet*
			make_sound_effect (const char *sound, double x, double y, double z,
				float vol, char pitch)
			{
				int sound_len = mc_str_len (sound);
				packet *pack = new packet (20 + sound_len);
				
				pack->put_varint (18 + sound_len);
				pack->put_varint (0x29);
				pack->put_string (sound);
				pack->put_int (x * 8.0);
				pack->put_int (y * 8.0);
				pack->put_int (z * 8.0);
				pack->put_float (vol);
				pack->put_byte (pitch);
				
				return pack;
			}
			
			packet*
			make_change_game_state (int reason, float value)
			{
				packet *pack = new packet (7);
				
				pack->put_varint (6);
				pack->put_varint (0x2B);
				pack->put_byte (reason);
				pack->put_float (value);
				
				return pack;
			}
			
			packet*
			make_open_window (unsigned char wid, unsigned char wtype,
				const char *title, unsigned char slot_count, bool use_title)
			{
				int title_len = mc_str_len (title);
				packet *pack = new packet (7 + title_len);
				
				pack->put_varint (5 + title_len);
				pack->put_varint (0x2D);
				pack->put_byte (wid);
				pack->put_byte (wtype);
				pack->put_string (title);
				pack->put_byte (slot_count);
				pack->put_bool (use_title);
				
				return pack;
			}
			
			packet*
			make_close_window (unsigned char wid)
			{
				packet *pack = new packet (3);
				
				pack->put_varint (2);
				pack->put_varint (0x2E);
				pack->put_byte (wid);
				
				return pack;
			}
			
			packet*
			make_set_slot (int wid, int slot, const slot_item& item)
			{
				int item_size = slot_size (item);
				packet *pack = new packet (6 + item_size);
				
				/* 
				 * TODO: Item metadata.
				 */
				
				pack->put_varint (4 + item_size);
				pack->put_varint (0x2F);
				pack->put_byte (wid);
				pack->put_short (slot);
				pack->put_slot (item);
				
				return pack;
			}
			
			packet*
			make_window_items (int wid, const std::vector<slot_item>& items)
			{
				/* 
				 * TODO: Item metadata.
				 */
				
				int items_size = 0;
				for (const slot_item s : items)
					items_size += slot_size (s);
				
				packet *pack = new packet (6 + items_size);
				
				pack->put_varint (4 + items_size);
				pack->put_varint (0x30);
				pack->put_byte (wid);
				pack->put_short (items.size ());
				for (const slot_item item : items)
					pack->put_slot (item);
				
				return pack;
			}
			
			packet*
			make_update_sign (int x, int y, int z, const char *first,
				const char *second, const char *third, const char *fourth)
			{
				int strs_len = mc_str_len (first) + mc_str_len (second)
					+ mc_str_len (third) + mc_str_len (fourth);
				packet *pack = new packet (13 + strs_len);
				
				pack->put_varint (11 + strs_len);
				pack->put_varint (0x33);
				pack->put_int (x);
				pack->put_short (y);
				pack->put_int (z);
				pack->put_string (first);
				pack->put_string (second);
				pack->put_string (third);
				pack->put_string (fourth);
				
				return pack;
			}
			
			packet*
			make_open_sign_editor (int x, int y, int z)
			{
				packet *pack = new packet (14);
				
				pack->put_varint (13);
				pack->put_varint (0x36);
				pack->put_int (x);
				pack->put_int (y);
				pack->put_int (z);
				
				return pack;
			}
			
			packet*
			make_player_list_item (const char *name, bool online, short ping)
			{
				int name_len = mc_str_len (name);
				packet *pack = new packet (6 + name_len);
				
				pack->put_varint (4 + name_len);
				pack->put_varint (0x38);
				pack->put_string (name);
				pack->put_bool (online);
				pack->put_short (ping);
				
				return pack;
			}
			
			packet*
			make_disconnect (const char *msg)
			{
				int msg_len = mc_str_len (msg);
				packet *pack = new packet (3 + msg_len);
				
				pack->put_varint (1 + msg_len);
				pack->put_varint (0x40);
				pack->put_string (msg);
				
				return pack;
			}
		}
		
		
		namespace status {
			
			packet*
			make_response (const char *response)
			{
				int response_len = mc_str_len (response);
				packet *pack = new packet (5 + response_len);
				
				int len = 1 + response_len;
				
				pack->put_varint (len);
				pack->put_varint (0x00); // opcode
				pack->put_string (response);
				
				return pack;
			}
			
			packet*
			make_ping (long long time)
			{
				packet *pack = new packet (10);
				
				pack->put_varint (9);	// length
				pack->put_varint (0x01); // opcode
				pack->put_long (time);
				
				return pack;
			}
		}
		
		
		namespace login {
			
			packet*
			make_disconnect (const char *js)
			{
				return nullptr;
			}
			
			packet*
			make_encryption_request (const std::string& sid,
				CryptoPP::RSA::PublicKey& pkey, unsigned char vtoken[4])
			{
				CryptoPP::ByteQueue q;
				pkey.Save (q);
		
				unsigned char buf[384];
				size_t keylen = q.Get (buf, sizeof buf);
		
				int sid_len = mc_str_len (sid);
				packet *pack = new packet (7 + sid_len + keylen + 4);
				
				pack->put_varint (9 + sid_len + keylen);
				pack->put_varint (0x01);
				pack->put_string (sid.c_str ());
				pack->put_short (keylen);
				pack->put_bytes (buf, keylen);
				pack->put_short (4);
				pack->put_bytes (vtoken, 4);
		
				return pack;
			}
			
			packet*
			make_login_success (const char *uuid, const char *username)
			{
				int uuid_len = mc_str_len (uuid);
				int username_len = mc_str_len (username);
				packet *pack = new packet (3 + uuid_len + username_len);
				
				int len = 1 + uuid_len + username_len;
				pack->put_varint (len);
				pack->put_varint (0x02);
				pack->put_string (uuid);
				pack->put_string (username);
				
				return pack;
			}
		}
	}
}

