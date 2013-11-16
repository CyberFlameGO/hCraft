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

#ifndef _hCraft__PACKET_H_
#define _hCraft__PACKET_H_

#include <cstdint>
#include <vector>
#include "slot/slot.hpp"

#include <cryptopp/rsa.h>


namespace hCraft {
	
	class chunk;
	class player;
	class entity_metadata;
	class edit_stage;
	
	
	struct block_change_record
	{
		unsigned char x, y, z;
		unsigned short id;
		unsigned char meta;
	};
	
	
	struct entity_property
	{
		const char *key;
		double value;
	};
	
	
	/* 
	 * The three protocol states that exist in 1.7.
	 */
	enum protocol_state
	{
		PS_HANDSHAKE	= 0,
		PS_STATUS			= 1,
		PS_LOGIN			= 2,
		PS_PLAY				= 3,
	};
	
	
	/* 
	 * A byte array wrapper that provides methods to encode binary data into it.
	 */
	struct packet
	{
		static const int protocol_version = 4;
		static constexpr const char* game_version = "1.7.2";
	//----
		
		unsigned char *data;
		unsigned int size;
		unsigned int pos;
		unsigned int cap;
		
		/* 
		 * Constructs a new packet that can hold up to the specified amount of bytes.
		 */
		packet (unsigned int size);
		
		/* 
		 * Class copy constructor.
		 */
		packet (const packet &other);
		
		/* 
		 * Class destructor.
		 */
		~packet ();
		
		
		
		/* 
		 * put methods:
		 */
		
		void put_byte (uint8_t val);
		void put_short (uint16_t val);
		void put_int (uint32_t val);
		void put_long (uint64_t val);
		void put_varint (uint32_t val);
		void put_float (float val);
		void put_double (double val);
		int put_string (const char *str, bool sanitize = true,
			bool encode_length = true);
		void put_bytes (const unsigned char *val, unsigned int len);
		void put_bool (bool val)
			{ put_byte (val ? 1 : 0); }
		void put_slot (const slot_item& item);
		
		
		/* 
		 * Resizes the packet.
		 */
		void resize (unsigned int new_size);
		
		void clear ();
		
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
		static int remaining (const unsigned char *data, unsigned int have);
	};
	
	
	/* 
	 * Data decoder for packets.
	 */
	class packet_reader
	{
		const unsigned char *data;
		unsigned int pos;
		
	public:
		/* 
		 * Constructs a new packet reader around the given byte array.
		 */
		packet_reader (const unsigned char *data);
		
		
		inline unsigned int
		seek (unsigned int new_pos)
		{
			unsigned int prev_pos = this->pos;
			this->pos = new_pos;
			return prev_pos;
		}
		
		
		/* 
		 * read methods:
		 */
		
		uint8_t read_byte ();
		uint16_t read_short ();
		uint32_t read_int ();
		uint64_t read_long ();
		uint32_t read_varint ();
		float read_float ();
		double read_double ();
		int read_string (char *out, int max_chars = 65535);
		slot_item read_slot ();
		void read_bytes (unsigned char *out, int len);
	};
	
	
	
	namespace packets {
		/* 
		 * Packet creation.
		 */
		
		namespace play {
			
			packet* make_keep_alive (int id);
			packet* make_join_game (int eid, int gm, int dim, int diff,
				int max_players, const char *level_type);
			packet* make_chat_message (const char *js);
			packet* make_time_update (long long world_age, long long time);
			packet* make_entity_equipment (int eid, short slot, const slot_item& item);
			packet* make_spawn_position (int x, int y, int z);
			packet* make_update_health (float health, short food, float sat);
			packet* make_respawn (int dim, int diff, int gm, const char *level_type);
			packet* make_player_pos_and_look (double x, double y, double z, float r,
				float l, bool on_ground);
			packet* make_held_item_change (int slot);
			packet* make_animation (int eid, int anim);
			packet* make_spawn_player (int eid, const char *uuid, const char *username,
				double x, double y, double z, float r, float l, short item,
				entity_metadata& meta);
			packet* make_collect_item (int collected_eid, int collector_eid);
			packet* make_spawn_object (int eid, int type, double x, double y, double z,
				float r, float l, int data, short speed_x, short speed_y, short speed_z);
			packet* make_entity_velocity (int eid, short vx, short vy, short vz);
			packet* make_destroy_entities (const std::vector<int>& eids);
			packet* make_destroy_entity (int eid);
			packet* make_entity (int eid);
			packet* make_entity_rel_move (int eid, double dx, double dy, double dz);
			packet* make_entity_look (int eid, float r, float l);
			packet* make_entity_look_and_rel_move (int eid, double dx, double dy, char dz,
				float r, float l);
			packet* make_entity_move (int eid, double x, double y, double z, float r, float l);
			packet* make_entity_head_look (int eid, float r);
			packet* make_entity_status (int eid, int status);
			packet* make_entity_metadata (int eid, entity_metadata& meta);
			packet* make_entity_properties (int eid,
				const std::vector<entity_property>& props);
			packet* make_chunk (int x, int z, chunk *ch, const std::vector<edit_stage *> es_vec);
			packet* make_chunk (int x, int z, chunk *ch);
			packet* make_empty_chunk (int x, int z);
			packet* make_multi_block_change (int cx, int cz,
				const std::vector<block_change_record>& records, player *sb = nullptr);
			packet* make_block_change (int x, int y, int z, unsigned short id,
				unsigned char meta);
			packet* make_sound_effect (const char *sound, double x, double y, double z,
				float vol, char pitch);
			packet* make_change_game_state (int reason, float value);
			packet* make_open_window (unsigned char wid, unsigned char wtype,
				const char *title, unsigned char slot_count, bool use_title);
			packet* make_close_window (unsigned char wid);
			packet* make_set_slot (int wid, int slot, const slot_item& item);
			packet* make_window_items (int wid, const std::vector<slot_item>& items);
			packet* make_update_sign (int x, int y, int z, const char *first,
				const char *second, const char *third, const char *fourth);
			packet* make_open_sign_editor (int x, int y, int z);
			packet* make_player_list_item (const char *name, bool online, short ping);
			packet* make_player_abilities (int flags, float flying_speed,
				float walking_speed);
			packet* make_disconnect (const char *msg);
		}
		
		namespace status {
			
			packet* make_response (const char *response);
			packet* make_ping (long long time);
		}
		
		namespace login {
			
			packet* make_disconnect (const char *js);
			packet* make_encryption_request (const std::string& sid,
				CryptoPP::RSA::PublicKey& pkey, unsigned char vtoken[4]);
			packet* make_login_success (const char *uuid, const char *username);
		}
	}
}

#endif

