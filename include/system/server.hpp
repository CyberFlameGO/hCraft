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

#ifndef _hCraft__SERVER_H_
#define _hCraft__SERVER_H_

#include "util/cistring.hpp"
#include "system/logger.hpp"
#include "player/player.hpp"
#include "player/player_list.hpp"
#include "system/scheduler.hpp"
#include "world/world.hpp"
#include "system/threadpool.hpp"
#include "commands/command.hpp"
#include "player/permissions.hpp"
#include "player/rank.hpp"
#include "system/authentication.hpp"
#include "world/generation/generator.hpp"
#include "world/world_list.hpp"
#include "system/messages.hpp"
#include "irc/irc.hpp"
#include "slot/crafting.hpp"

#include <soci/soci.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <event2/event.h>
#include <event2/listener.h>
#include <unordered_set>
#include <map>
#include <set>

#include <cryptopp/rsa.h>
#include <cryptopp/osrng.h>


namespace hCraft {
	
	/* 
	 * Base class for all exceptions thrown by the server.
	 */
	class server_error: public std::runtime_error
	{
	public:
		server_error (const char *what)
		: std::runtime_error (what)
			{ }
	};
	
	
	/* 
	 * A simple POD structure for settings needed by the server to run.
	 */
	struct server_config
	{
		char srv_name[81];
		char srv_motd[81];
		int  max_players;
		char main_world[33];
		bool online_mode;
		bool load_prev_pos;
		
		char ip[16];
		int  port;
		
		char self_highlight_color;
		char name_highlight_color;
		
		// sql:
		std::string db_name;
		std::string db_user;
		std::string db_pass;
		std::string db_host;
		int db_port;
		
		// irc:
		bool irc_enabled;
		std::string irc_net;
		int irc_port;
		std::string irc_chan;
		std::string irc_nick;
		
		std::set<std::string> dcmds; // disabled commands
	};
	
	
	struct muted_player {
		char username[17];
		int seconds;
	};
	
	/* 
	 * 
	 */
	class server
	{
		friend struct worker;
		
		/* 
		 * To simplify the process of starting up a server, the actual logic of the
		 * start () member function is divided and moved into smaller functions
		 * (the initializer and destroyer functions). When an initializer function
		 * fails (i.e: an exception is thrown), the destroyer functions for all
		 * initializers that have successfully completed up until that point are
		 * invoked to reclaim the resources that had been allocated.
		 */
		struct initializer
		{
			std::function<void ()> init;
			std::function<void ()> destroy;
			bool initialized;
			
			// constructor.
			initializer (std::function<void ()>&& init,
				std::function<void ()>&& destroy);
			
			// move constructor.
			initializer (initializer&& other);
			
			initializer (const initializer &);
		};
		
		
		/* 
		 * Represents a server worker, an entity to which a thread and an event base
		 * are attached. Every server worker handles I/O for all events that are
		 * registered with it in a separate thread of execution, equally dividing
		 * the server's load.
		 */
		struct worker
		{
			struct event_base *evbase;
			std::atomic_int event_count;
			std::thread th;
			
			// constructor.
			worker (struct event_base *base, std::thread&& th);
			
			// move constructor.
			worker (worker&& w);
			
			worker (const worker&) = delete;
		};
		
	private:
		logger& log;
		server_config cfg;
		
		//sql::connection_pool spool;
		soci::connection_pool spool;
		
		permission_manager perms;
		group_manager groups;
		
		std::vector<initializer> inits; // <init, destroy> pairs
		bool running;
		bool shutting_down;
		
		std::vector<worker> workers;
		int worker_count;
		bool workers_ready;
		bool workers_stop;
		
		struct evconnlistener *listener;
		
		player_list *players;
		std::unordered_set<player *> connecting;
		std::unordered_set<player *> to_destroy;
		std::mutex player_lock;
		crafting_manager craftman;
		
		std::mutex id_lock;
		int entity_id_counter;
		std::unordered_map<int, entity *> entity_map;
		int world_id_counter;
		std::map<int, world *> world_map;
		
		scheduler sched;
		thread_pool tpool;
		
		world_list worlds;
		world *main_world;
		
		command_list *commands;
		
		// encryption
		CryptoPP::RSA::PrivateKey rsa_private;
		std::string server_id;
		
		// muted players
		std::vector<muted_player> muted;
		std::mutex mute_lock;
		
		// IRC client
		irc_client *ircc;
		
	public:
		physics_manager global_physics; // initially shared between all worlds
		authenticator auth;
		chunk_generator cgen;
		
		server_messages msgs;
		
	private:
		// <init, destroy> functions:
		
		/* 
		 * Performs cleanup on resources that can be only done before all other
		 * <init, destory> pairs have been executed.
		 */
		void initial_cleanup ();
		
		/* 
		 * Performs cleanup on resources that can only be done after all other
		 * <init, destory> pairs have been executed.
		 */
		void final_cleanup ();
		
		/* 
		 * Loads settings from the configuration file ("config.yaml",
		 * in YAML form) into the server's `cfg' structure. If "config.yaml"
		 * does not exist, it will get created with default settings.
		 */
		void init_config ();
		void destroy_config ();
		
		/* 
		 * Loads the server's database.
		 */
		void init_sql ();
		void destroy_sql ();
		
		/* 
		 * Initializes various data structures and variables needed by the server.
		 */
		void init_core ();
		void destroy_core ();
		
		/* 
		 * Loads up commands.
		 */
		void init_commands ();
		void destroy_commands ();
		
		/* 
		 * Reads reads and their associated permissions from "ranks.yaml".
		 * If the file does not exist, it will get created with default settings.
		 */
		void init_ranks ();
		void destroy_ranks ();
		
		/* 
		 * Loads up and initializes worlds.
		 */
		void init_worlds ();
		void destroy_worlds ();
		
		/* 
		 * Creates and starts server workers.
		 * The total number of workers created depends on how many cores the user
		 * has installed on their system, which means that on a multi-core system,
		 * the work will be parallelized between all cores.
		 */
		void init_workers ();
		void destroy_workers ();
		
		/* 
		 * Creates the listening socket and starts listening on the IP address and
		 * port number specified by the user in the configuration file for incoming
		 * connections.
		 */
		void init_listener ();
		void destroy_listener ();

		/* 
		 * Initializes the IRC client, and connects it to the IRC network/channel
		 * specified in the configuration file.
		 */
		void init_irc ();
		void destroy_irc ();
		
	private:
		/* 
		 * The function executed by worker threads.
		 * Waits for incoming connections.
		 */
		void work ();
		
		/* 
		 * Returns the worker that has the least amount of events associated with
		 * it.
		 */
		worker& get_min_worker ();
		
		/* 
		 * Wraps the accepted connection around a player object and associates it
		 * with a server worker.
		 */
		static void handle_accept (struct evconnlistener *listener,
			evutil_socket_t sock, struct sockaddr *addr, int len, void *ptr);
			
	private:
		// scheduler callbacks:
		
		/* 
		 * Removes and destroys disconnected players.
		 */
		static void cleanup_players (scheduler_task& task);
		
		/* 
		 * Removes players from the muted player list when their time is up.
		 */
		static void handle_muted (scheduler_task& task);
		
		/* 
		 * Saves all currently loaded worlds.
		 */
		static void save_worlds (scheduler_task& task);
		
		/* 
		 * Pings all online players (every eight seconds).
		 */
		static void ping_players (scheduler_task& task);
		
	public:
		inline bool is_running () { return this->running; }
		inline bool is_shutting_down () { return this->shutting_down; }
		
		inline const server_config& get_config () { return this->cfg; }
		inline logger& get_logger () { return this->log; }
		inline player_list& get_players () { return *this->players; }
		inline irc_client* get_irc () { return this->ircc; }
		inline scheduler& get_scheduler () { return this->sched; }
		inline thread_pool& get_thread_pool () { return this->tpool; }
		inline world* get_main_world () { return this->main_world; }
		inline command_list& get_commands () { return *this->commands; }
		inline permission_manager& get_perms () { return this->perms; }
		inline group_manager& get_groups () { return this->groups; }
		inline world_list& get_worlds () { return this->worlds; }
		inline crafting_manager& get_crafting_manager () { return this->craftman; }
		
		inline std::mutex& get_player_lock () { return this->player_lock; }
		
		inline soci::connection_pool& sql_pool () { return this->spool; }
		
		inline const std::string& auth_id () { return this->server_id; }
		inline CryptoPP::RSA::PublicKey public_key ()
			{ return CryptoPP::RSA::PublicKey (this->rsa_private); }
		inline CryptoPP::RSA::PrivateKey& private_key ()
			{ return this->rsa_private; }
		
	public:
		/* 
		 * Constructs a new server.
		 */
		server (logger &log);
		
		// copy constructor.
		server (const server &) = delete;
		
		/* 
		 * Class destructor.
		 * 
		 * Calls `stop ()' if the server is still running.
		 */
		~server ();
		
		
		
		/* 
		 * Attempts to start the server up.
		 * Throws `server_error' on failure.
		 */
		void start ();
		
		/* 
		 * Stops the server, kicking all connected players and freeing resources
		 * previously allocated by the start () member function.
		 */
		void stop ();
		
		
		
//-----
		/* 
		 * Returns a unique number that can be used for entity identification.
		 */
		int register_entity (entity *e);
		
		/* 
		 * Removes an entity from the entity\id map.
		 */
		void deregister_entity (entity *e);
		void deregister_entity (int eid);
		
		/* 
		 * Returns the player\entity associated with the given ID.
		 */
		entity* entity_by_id (int id);
		player* player_by_id (int id);
//-----

		
//-----
		/* 
		 * Same thing for worlds.
		 */
		 
		bool register_world (world *w);
		void deregister_world (world *w);
		world* world_by_id (int id);

//-----
		
		
		
		/* 
		 * Removes the specified player from the "connecting" list, and then inserts
		 * that player into the global player list.
		 * 
		 * If the server is full, or if the same player connected twice, the function
		 * returns false and the player is kicked with an appropriate message.
		 */
		bool done_connecting (player *pl);
		
		/* 
		 * Informs the server that player @{pl} is no longer valid, and must be
		 * destroyed.
		 */
		void schedule_destruction (player *pl);
		
		
		
		/* 
		 * Player muting:
		 */
		void mute_player (const char* username, int secs);
		void unmute_player (const char* username);
		bool is_player_muted (const char* username);
	};
}

#endif

