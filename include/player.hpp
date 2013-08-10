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

#ifndef _hCraft__PLAYER_H_
#define _hCraft__PLAYER_H_

#include "entities/entity.hpp"
#include "logger.hpp"
#include "packet.hpp"
#include "world.hpp"
#include "rank.hpp"
#include "messages.hpp"
#include "window.hpp"
#include "callback.hpp"
#include "selection/world_selection.hpp"
#include "cistring.hpp"
#include "sqlops.hpp"
#include "generator.hpp"

#include <atomic>
#include <queue>
#include <deque>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <functional>

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>


namespace hCraft {
	
	class server; // forward dec
	
	
	enum gamemode_type
	{
		GT_SURVIVAL = 0,
		GT_CREATIVE = 1,
		GT_ADVENTURE = 2,
	};
	
	struct player_extra_data
	{
		void *data;
		void (*dctor)(void *);
	};
	
	
	
	struct selection_block
	{
		int x, y, z;
		mutable short counter;
		
		selection_block (int x, int y, int z, short counter = 0)
			{ this->x = x; this->y = y; this->z = z; this->counter = counter; }
		
		bool
		operator== (const selection_block& other) const
			{ return this->x == other.x && this->y == other.y && this->z == other.z; }
	};
	
	class selection_block_hash
	{
		std::hash<int> int_hash;
		
	public:
		std::size_t
		operator() (const selection_block& sb) const
		{
			return int_hash (sb.x) ^ (int_hash (sb.y) << 11) ^ (int_hash (sb.z) << 5);
		}
	};
	
	
	
//--------
	
	struct known_chunk
	{
		world *w;
		int cx, cz;
		
	//---
		bool
		operator== (const known_chunk other) const
		{
			return ((this->w == other.w) && (this->cx == other.cx) && (this->cz == other.cz));
		}
	};
	
	/*
	// TODO: remove?

	class known_chunk_hash
	{
		std::hash<int> int_hash;
		
	public:
		std::size_t
		operator() (const known_chunk& kc) const
		{
			return int_hash (((long long)kc.w & 0xFFFFFFFF) + ((int_hash (kc.cx) ^ (int_hash (kc.cz) << 5)) << 7));
		}
	};
	*/
	
//--------
	
	/* 
	 * Represents a player.
	 */
	class player: public living_entity
	{
		friend class authenticator;
		
		logger& log;
		
		struct event_base *evbase;
		struct bufferevent *bufev;
		evutil_socket_t sock;
		
		int dbid;
		bool op;
		rank rnk;
		char ip[16];
		bool logged_in;
		bool handshake;
		bool authenticated;
		bool encrypted;
		bool fail; // true if the player is no longer valid, and must be disposed of.
		std::chrono::time_point<std::chrono::system_clock> fail_time;
		
		bool eating;
		std::chrono::steady_clock::time_point eat_time;
		
		// used to handle exhaustion:
		double total_walked, total_run;
		double total_walked_old, total_run_old;
		
		double last_ground_height;
		bool fall_flag;
		
		char username[17];
		char colored_username[24];
		char nick[37]; // 36 chars max
		char colored_nick[48];
		
		gamemode_type curr_gamemode;
		short held_slot;
		slot_item cursor_slot;
		bool inv_painting; // inventory paining mode, added in 1.5
		char inv_mb; // mouse button ^
		std::vector<short> inv_paint_slots;
		
		char kick_msg[384];
		bool kicked;
		bool disconnecting;
		
		bool reading;
		unsigned char rdbuf[384];
		int total_read;
		int read_rem;
		std::atomic_int handlers_scheduled;
		CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption *decryptor;
		
		unsigned char vtoken[4];
		unsigned char ssec[16]; // shared secret
		
		// Using a thread pool to execute packet handling methods gives rise to a
		// problem: Packets that must be executed in a certain order (such as, for
		// example - 0x66 [window click] packets) are NOT guaranteed to be handled
		// in the same order they were received. To solve this, received packets
		// are first put into this queue, and only after a conclusion is made that
		// it is indeed safe to call the packet callbacks, the packets in the queue
		// are executed one after the other all at once in a pooled thread.
		std::deque<unsigned char *> exec_queue;
		
		bool writing;
		std::queue<packet *> out_queue;
		std::mutex out_lock;
		CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption *encryptor;
		
		bool ping_waiting;
		std::chrono::time_point<std::chrono::system_clock> last_ping;
		int ping_id;
		int ping_time_ms;
		int keep_alives_received;
		
		world *curr_world;
		chunk_pos chcurr;
		std::mutex world_lock;
		bool joining_world;
		bool streaming_chunks;
		
		std::unordered_set<player *> visible_players;
		std::mutex visible_player_lock;
		
		std::vector<known_chunk> pending_chunks;
		std::queue<gen_response> response_chunks;
		std::mutex response_chunks_lock;
		bool need_new_chunks;
				
		std::chrono::steady_clock::time_point last_tick;
		std::chrono::steady_clock::time_point last_heart_regen;
		std::chrono::steady_clock::time_point last_portal_use;
		std::chrono::milliseconds heal_delay;
		long long tick_counter;
		
		std::ostringstream msgbuf;
		
		std::vector<block_pos> marked_blocks;
		std::vector<callback<bool (player *, block_pos[], int)> > mark_callbacks;
		std::unordered_map<std::string, player_extra_data> extra_data;
		std::mutex data_lock;
		
		// a list of all edit stages that should re-send their contents whenever
		// the player crosses chunk boundaries.
		std::unordered_set<edit_stage *> edstages;
		sparse_edit_stage sb_updates; // selection block updates
		
	public:
		std::unordered_map<cistring, world_selection *> selections;
		world_selection *curr_sel;
		std::unordered_set<selection_block, selection_block_hash> sel_blocks;
		blocki sb_block;
		std::mutex sb_lock;
		
		std::vector<known_chunk> known_chunks;
		
		inventory inv;
		
		int bl_destroyed;
		int bl_created;
		int msgs_sent;
		
		std::time_t log_first;
		std::time_t log_last;
		int log_count;
		
		double bal;
		
		bool banned;
		
	private:
		/* 
		 * libevent callback functions:
		 */
		static void handle_read (struct bufferevent *bufev, void *ctx);
		static void handle_write (struct bufferevent *bufev, void *ctx);
		static void handle_event (struct bufferevent *bufev, short events, void *ctx);
		
		/* 
		 * Packet handlers:
		 * NOTE: These return 0 on success (any other value will disconnect the
		 *       player).
		 */
		static int handle_packet_00 (player *pl, packet_reader reader);
		static int handle_packet_02 (player *pl, packet_reader reader);
		static int handle_packet_03 (player *pl, packet_reader reader);
		static int handle_packet_07 (player *pl, packet_reader reader);
		static int handle_packet_0a (player *pl, packet_reader reader);
		static int handle_packet_0b (player *pl, packet_reader reader);
		static int handle_packet_0c (player *pl, packet_reader reader);
		static int handle_packet_0d (player *pl, packet_reader reader);
		static int handle_packet_0e (player *pl, packet_reader reader);
		static int handle_packet_0f (player *pl, packet_reader reader);
		static int handle_packet_10 (player *pl, packet_reader reader);
		static int handle_packet_12 (player *pl, packet_reader reader);
		static int handle_packet_13 (player *pl, packet_reader reader);
		static int handle_packet_cd (player *pl, packet_reader reader);
		static int handle_packet_65 (player *pl, packet_reader reader);
		static int handle_packet_66 (player *pl, packet_reader reader);
		static int handle_packet_6a (player *pl, packet_reader reader);
		static int handle_packet_6b (player *pl, packet_reader reader);
		static int handle_packet_fa (player *pl, packet_reader reader);
		static int handle_packet_fc (player *pl, packet_reader reader);
		static int handle_packet_fe (player *pl, packet_reader reader);
		static int handle_packet_ff (player *pl, packet_reader reader);
		
		/* 
		 * Examines the queue that holds packets pending to be handled by
		 * appropriate callback methods in-order to conclude whether it is safe
		 * to handle the packets. 
		 */
		bool test_packet_chain ();
		
		/* 
		 * Executes the appropriate packet handler for the given byte array.
		 */
		int handle (const unsigned char *data);
		
		/* 
		 * Executed in a thread pool task, spawned by handle ().
		 */
		friend void handle_func (void *ctx);
		
		
	//----
		
		void handle_falls_and_jumps (bool prev, bool curr, entity_pos old_pos);
		
		bool handle_crafting (unsigned char wid);
		
		void handle_death ();
		
		void handle_portals ();
		
	//----
		
		/* 
		 * Sends a ping packet to the player and waits for a response.
		 */
		void ping ();
		
		/* 
		 * Sends a ping packet to the player only if the specified amount of
		 * milliseconds have passed since the last ping packet has been sent.
		 * 
		 * If the player is still waiting for a ping response, the function
		 * will kick the player.
		 */
		void try_ping (int ms);
		
	//----
		
		/* 
		 * Moves the player to the specified position.
		 */
		void move_to (entity_pos dest);
		
		void update_home_chunk ();
		
	//----
		
		/* 
		 * Loads information about the player from the server's SQL database.
		 * Returns false on failure.
		 */
		bool load_data ();
		void save_data ();
		
		
	//----
		
		/* 
		 * Called by the authenticator to inform the player that it's ready to spawn.
		 */
		void done_authenticating ();
		
		/* 
		 * Sends the world and spawns the player.
		 */
		void login ();
		
	public:
		inline server& get_server () { return this->srv; }
		inline logger& get_logger () { return this->log; }
		
		inline const char* get_ip () { return this->ip; }
		inline const char* get_username () { return this->username; }
		inline const char* get_colored_username () { return this->colored_username; }
		inline const char* get_nickname () { return this->nick; }
		inline const char* get_colored_nickname () { return this->colored_nick; }
		
		inline const rank& get_rank () { return this->rnk; }
		inline bool is_op () { return this->op; }
		
		inline bool is_reading () { return this->reading; }
		inline bool is_writing () { return this->writing; }
		inline bool is_handling_packets () { return (this->handlers_scheduled.load () > 0); }
		inline bool is_disconnecting () { return this->disconnecting; }
		inline std::chrono::time_point<std::chrono::system_clock> disconnection_time ()
			{ return this->fail_time; }
		
		inline world* get_world () { return this->curr_world; }
		inline std::mutex& get_world_lock () { return this->world_lock; }
		static constexpr int chunk_radius () { return 5; }
		
		inline slot_item& held_item () { return this->inv.get (this->held_slot); }
		inline slot_item cursor_item () { return this->cursor_slot; }
		inline gamemode_type gamemode () { return this->curr_gamemode; }
		
		inline int get_ping () { return this->ping_time_ms; }
		
		// whether the player isn't valid anymore, and should be destroyed.
		inline bool bad () { return this->fail || this->disconnecting; }
		inline bool has_logged_in () { return this->logged_in; }
		
		virtual entity_type get_type () { return ET_PLAYER; }
		
	public:
		/* 
		 * Constructs a new player around the given socket.
		 */
		player (server &srv, struct event_base *evbase, evutil_socket_t sock,
			const char *ip);
		
		// copy constructor.
		player (const player &) = delete;
		
		/* 
		 * Class destructor.
		 */
		~player ();
		
		
		
		/* 
		 * Marks the player invalid, forcing the server that spawned the player to
		 * eventually destroy it.
		 */
		void disconnect (bool silent = false, bool wait_for_callbacks_to_finish = true);
		
		/* 
		 * Kicks the player with the given message.
		 */
		void kick (const char *msg, const char *log_msg = nullptr, bool sanitize = true);
		
		
		
		/* 
		 * Inserts the specified packet into the player's queue of outgoing packets.
		 */
		void send (packet *pack);
		
		/* 
		 * Resends the block located at the given block coordinates.
		 */
		void send_orig_block (int x, int y, int z);
		
		
		
		/* 
		 * Sends the player to the given world.
		 */
		void join_world (world* w);
		
		/* 
		 * Sends the player to the given world at the specified location.
		 */
		void join_world_at (world *w, entity_pos destpos);
		
		/* 
		 * Reloads the map for the player.
		 */
		void rejoin_world ();
		
		/* 
		 * Loads new close chunks to the player and unloads those that are too
		 * far away.
		 */
		void stream_chunks (int radius = player::chunk_radius ());
		
		/* 
		 * Checks whether the specified chunk is within the visible chunk range
		 * of the player.
		 */
		bool can_see_chunk (int x, int z);
		
		/* 
		 * Used by the chunk_generator class to inform the player that a chunk
		 * has been generated.
		 */
		void deliver_chunk (world *w, int x, int z, chunk *ch, int flags, int extra);
		
		
		
		/* 
		 * Modifies the player's gamemode.
		 */
		void change_gamemode (gamemode_type gm);
		
		
		
		/* 
		 * Teleports the player to the given position.
		 */
		void teleport_to (entity_pos dest);
		
		/* 
		 * Checks whether this player can be seen by player @{pl}.
		 */
		bool visible_to (player *pl);
		
		
		
		/* 
		 * Spawns self to the specified player.
		 */
		virtual void spawn_to (player *pl) override;
		void spawn_to_all ();
		
		/* 
		 * Despawns self from the specified player.
		 */
		virtual bool despawn_from (player *pl) override;
		void despawn_from_all ();
		
		
		void add_to_tab_list (bool self = true);
		void remove_from_tab_list (bool self = true);
		void clear_tab_list ();
		void load_tab_list ();
		
		
		
		/* 
		 * Sends the given message to the player.
		 */
		void message (const char *msg);
		void message (const std::string& msg);
		void message_wrapped (const char *msg, const char *prefix = "§7 > §f", bool first_line = false);
		void message_wrapped (const std::string& msg, const char *prefix = "§7 > §f", bool first_line = false);
		void message_spaced (const char *msg, bool remove_from_first = false);
		void message_spaced (const std::string& msg, bool remove_from_first = false);
		
	//----
		
		/* 
		 * Checks whether the player's rank has the given permission node.
		 */
		bool has (const char *perm);
		
		/* 
		 * Utility function used by commands.
		 * Returns true if the player has the given permission; otherwise, it prints
		 * an error message to the player and return false.
		 */
		bool perm (const char *perm);
		
		/* 
		 * Syntax:
		 *   term1|term2|term3|...|termN
		 * 
		 * Where term is one of:
		 *   <group name>   : e.g. moderator or builder
		 *   ^<player name> : e.g. ^BizarreCake
		 *   *<permission>  : e.g. *command.admin.say
		 */
		bool has_access (const std::string& access_str);
		
		
		
	//----
		
		/* 
		 * Modifies the player's nickname.
		 */
		void set_nickname (const char *nick, bool modify_sql = true);
		
		/* 
		 * Modifies the player's rank.
		 * NOTE: This does NOT update the database.
		 */
		void set_rank (const rank& rnk);
		
		/* 
		 * Fills the specified player_info structure with information about the
		 * player.
		 */
		void player_data (sqlops::player_info& pd);
		
	//----
		
		/* 
		 * Gets the marking callback that is executed after @{n} blocks are marked.
		 */
		callback<bool (player *, block_pos[], int)>&
		get_nth_marking_callback (int n);
		
		bool have_marking_callbacks ();
		bool mark_block (int x, int y, int z);
		void stop_marking ();
		
		
		/* 
		 * Used by world selections:
		 */
		void sb_add (int x, int y, int z);
		void sb_remove (int x, int y, int z);
		bool sb_exists (int x, int y, int z);
		void sb_commit ();
		void sb_add_nolock (int x, int y, int z);
		void sb_remove_nolock (int x, int y, int z);
		bool sb_exists_nolock (int x, int y, int z);
		void sb_commit_nolock ();
		void sb_send (int x, int y, int z);
		
		
		/* 
		 * 
		 */
		void es_add (edit_stage *es);
		void es_remove (edit_stage *es);
		
		
		/* 
		 * These three functions can be used to store additional general-purpose
		 * data for various things (such as, say, drawing operations).
		 */
		
		void create_data (const char *name, void *data,
			void (*dctor) (void *) = nullptr);
		void delete_data (const char *name, bool destruct = true);
		void* get_data (const char *name);
		
	//----
		
		/* 
		 * Called by the world that's holding the entity every tick (50ms).
		 * A return value of true will cause the world to destroy the entity.
		 */
		virtual bool tick (world &w) override;
		
		/* 
		 * Modifies the entity's health.
		 */
		virtual void set_health (int hearts, int hunger, float hunger_saturation) override;
	};
}

#endif

