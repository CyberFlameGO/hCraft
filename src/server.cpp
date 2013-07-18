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

#include "server.hpp"
#include "utils.hpp"
#include "physics/blocks/physics_block.hpp"
#include "config.hpp"
#include <memory>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <algorithm>
#include <event2/thread.h>


namespace hCraft {
	
	static const size_t sql_pool_size = 8;
	
	// constructor.
	server::initializer::initializer (std::function<void ()>&& init,
		std::function<void ()>&& destroy)
		: init (std::move (init)), destroy (std::move (destroy))
		{ this->initialized = false; }
	
	// move constructor.
	server::initializer::initializer (initializer&& other)
		: init (std::move (other.init)), destroy (std::move (other.destroy)),
			initialized (other.initialized)
		{ }
	
	server::initializer::initializer (const initializer &other)
		: init (other.init), destroy (other.destroy), initialized (other.initialized)
		{ }
	
	
	
	// constructor.
	server::worker::worker (struct event_base *base, std::thread&& th)
		: evbase (base), event_count (0), th (std::move (th))
		{ }
	
	// move constructor.
	server::worker::worker (worker&& w)
		: evbase (w.evbase), 
			event_count (w.event_count.load ()),
			th (std::move (w.th))
		{ }
	
	
	
	
	static void _noop () {}
	
	/* 
	 * Constructs a new server.
	 */
	server::server (logger &log)
		: log (log), 
			spool (sql_pool_size, "data/database.sqlite"),
			perms (),
			groups (perms)
	{
		// add <init, destory> pairs
		
		this->inits.push_back (initializer (
			_noop,
			std::bind (std::mem_fn (&hCraft::server::final_cleanup), this)));
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_config), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_config), this)));
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_sql), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_sql), this)));
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_core), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_core), this)));
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_commands), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_commands), this)));
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_ranks), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_ranks), this)));
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_worlds), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_worlds), this)));
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_workers), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_workers), this)));
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_listener), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_listener), this)));
		
		this->inits.push_back (initializer (
			_noop,
			std::bind (std::mem_fn (&hCraft::server::initial_cleanup), this)));
		
		this->running = false;
	}
	
	/* 
	 * Class destructor.
	 * 
	 * Calls `stop ()' if the server is still running.
	 */
	server::~server ()
	{
		this->stop ();
	}
	
	
	
	/* 
	 * The function executed by worker threads.
	 * Waits for incoming connections.
	 */
	void
	server::work ()
	{
		while (!this->workers_ready)
			{
				if (this->workers_stop)
					return;
				
				std::this_thread::sleep_for (std::chrono::milliseconds (10));
			}
		
		// find the worker that spawned this thread.
		std::thread::id this_id = std::this_thread::get_id ();
		worker *w = nullptr;
		for (auto itr = this->workers.begin (); itr != this->workers.end (); ++itr)
			{
				worker &t = *itr;
				if (t.th.get_id () == this_id)
					w = &t;
			}
		
		while (!event_base_got_break (w->evbase))
			{
				event_base_loop (w->evbase, EVLOOP_NONBLOCK);
				std::this_thread::sleep_for (std::chrono::milliseconds (1));
			}
	}
	
	/* 
	 * Returns the worker that has the least amount of events associated with
	 * it.
	 */
	server::worker&
	server::get_min_worker ()
	{
		auto min_itr = this->workers.begin ();
		for (auto itr = (min_itr + 1); itr != this->workers.end (); ++itr)
			{
				if (itr->event_count.load () < min_itr->event_count.load ())
					min_itr = itr;
			}
		
		return *min_itr;
	}
	
	/* 
	 * Wraps the accepted connection around a player object and associates it
	 * with a server worker.
	 */
	void
	server::handle_accept (struct evconnlistener *listener, evutil_socket_t sock,
		struct sockaddr *addr, int len, void *ptr)
	{
		server &srv = *static_cast<server *> (ptr);
		if (srv.is_shutting_down ())
			{
				evutil_closesocket (sock);
				return;
			}
		
		// get IP address
		char ip[16];
		if (!inet_ntop (AF_INET, &(((struct sockaddr_in *)addr)->sin_addr), ip,
			sizeof ip))
			{
				srv.log (LT_WARNING) << "Received a connection from an invalid IP address." << std::endl;
				evutil_closesocket (sock);
				return;
			}
		
		worker &w = srv.get_min_worker ();
		
		std::lock_guard<std::mutex> guard {srv.player_lock};
		player *pl = new player (srv, w.evbase, sock, ip);
		srv.connecting.insert (pl);
	}
	
	
	
	/* 
	 * Removes and destroys disconnected players.
	 */
	void
	server::cleanup_players (scheduler_task& task)
	{
		server &srv = *(static_cast<server *> (task.get_context ()));
		if (!srv.is_running () || srv.is_shutting_down ())
			return;
		
		// check list of logged-in players.
		srv.get_players ().remove_if (
			[] (player *pl) -> bool
				{
					return pl->bad ();
				});
		
		// destroy all players that have been idle (no I/O) for over a minute.
		std::lock_guard<std::mutex> guard {srv.player_lock};
		for (auto itr = std::begin (srv.to_destroy); itr != std::end (srv.to_destroy);)
			{
				player *pl = *itr;
				if (((std::chrono::system_clock::now () - pl->disconnection_time ()) >
					std::chrono::seconds (30)) && !pl->is_reading () && !pl->is_writing ()
					&& !pl->is_handling_packets ())
					{
						itr = srv.to_destroy.erase (itr);
						delete pl;
					}
				else
					++ itr;
			}
	}
	
	
	
	/* 
	 * Attempts to start the server up.
	 * Throws `server_error' on failure.
	 */
	void
	server::start ()
	{
		if (this->running)
			throw server_error ("server already running");
		this->shutting_down = false;
		
		try
			{
				for (auto itr = this->inits.begin (); itr != this->inits.end (); ++itr)
					{
						initializer& init = *itr;
						init.init ();
						init.initialized = true;
					}
			}
		catch (const std::exception& ex)
			{
				this->shutting_down = true;
				for (auto itr = this->inits.rbegin (); itr != this->inits.rend (); ++itr)
					{
						initializer &init = *itr;
						if (init.initialized)
							{
								init.destroy ();
								init.initialized = false;
							}
					}
				
				// wrap the error in a server_error object.
				throw server_error (ex.what ());
			}
		
		this->running = true;
	}
	
	/* 
	 * Stops the server, kicking all connected players and freeing resources
	 * previously allocated by the start () member function.
	 */
	void
	server::stop ()
	{
		if (!this->running)
			return;
		this->shutting_down = true;
		
		log (LT_SYSTEM) << "Stopping server..." << std::endl;
		for (auto itr = this->inits.rbegin (); itr != this->inits.rend (); ++itr)
			{
				initializer &init = *itr;
				init.destroy ();
				init.initialized = false;
			}
		
		this->running = false;
	}
	
	
	
	/* 
	 * Attempts to insert the specifed world into the server's world list.
	 * Returns true on success, and false on failure (due to a name collision).
	 */
	bool
	server::add_world (world *w)
	{
		std::lock_guard<std::mutex> guard {this->world_lock};
		
		cistring name {w->get_name ()};
		auto itr = this->worlds.find (name);
		if (itr != this->worlds.end ())
			return false;
		
		this->worlds[std::move (name)] = w;
		return true;
	}
	
	/* 
	 * Removes the specified world from the server's world list.
	 */
	void
	server::remove_world (world *w)
	{
		std::lock_guard<std::mutex> guard {this->world_lock};
		
		for (auto itr = this->worlds.begin (); itr != this->worlds.end (); ++itr)
			{
				world *other = itr->second;
				if (other == w)
					{
						other->stop ();
						this->worlds.erase (itr);
						delete other;
						break;
					}
			}
	}
	
	void
	server::remove_world (const char *name)
	{
		std::lock_guard<std::mutex> guard {this->world_lock};
		
		auto itr = this->worlds.find (name);
		if (itr != this->worlds.end ())
			this->worlds.erase (itr);
	}
	
	/* 
	 * Searches the server's world list for a world that has the specified name.
	 */
	world*
	server::find_world (const char *name)
	{
		std::lock_guard<std::mutex> guard {this->world_lock};
		
		auto itr = this->worlds.find (name);
		if (itr != this->worlds.end ())
			return itr->second;
		return nullptr;
	}
	
	
	
	/* 
	 * Returns a unique number that can be used for entity identification.
	 */
	int
	server::next_entity_id ()
	{
		std::lock_guard<std::mutex> guard {this->id_lock};
		int id = this->id_counter ++;
		if (this->id_counter < 0)
			this->id_counter = 0; // overflow.
		return id;
	}
	
	/* 
	 * Removes the specified player from the "connecting" list, and then inserts
	 * that player into the global player list.
	 * 
	 * If the server is full, or if the same player connected twice, the function
	 * returns false and the player is kicked with an appropriate message.
	 */
	bool
	server::done_connecting (player *pl)
	{
		bool can_stay = true;
		
		if (this->players->count () == this->get_config ().max_players)
			{ pl->kick ("§bThe server is full", "server full");
				can_stay = false; }
		else if (this->players->add (pl) == false)
			{ pl->kick ("§4You're already logged in", "already logged in");
				can_stay = false; }
		
		{
			std::lock_guard<std::mutex> guard {this->player_lock};
			this->connecting.erase (pl);
		}
		
		return can_stay;
	}
	
	/* 
	 * Informs the server that player @{pl} is no longer valid, and must be
	 * destroyed.
	 */
	void
	server::schedule_destruction (player *pl)
	{
		std::lock_guard<std::mutex> guard {this->player_lock};
		
		this->get_players ().remove (pl);
		this->connecting.erase (pl);
		
		this->to_destroy.insert (pl);
	}
	
	
	
	/* 
	 * Player muting:
	 */
	
	void
	server::mute_player (const char* username, int secs)
	{
		std::lock_guard<std::mutex> guard {this->mute_lock};
		
		for (muted_player& m : this->muted)
			if (std::strcmp (m.username, username) == 0)
				return; // already muted
		
		muted_player m;
		std::strcpy (m.username, username);
		m.seconds = secs;
		this->muted.push_back (m);
	}

	void
	server::unmute_player (const char* username)
	{
		std::lock_guard<std::mutex> guard {this->mute_lock};
		for (auto itr = this->muted.begin (); itr != this->muted.end (); ++itr)
			if (std::strcmp (itr->username, username) == 0)
				{
					this->muted.erase (itr);
					return;
				}
	}
	
	bool
	server::is_player_muted (const char* username)
	{
		std::lock_guard<std::mutex> guard {this->mute_lock};
		for (muted_player& m : this->muted)
			if (std::strcmp (m.username, username) == 0)
				return true;
		return false;
	}
	
	/* 
	 * Removes players from the muted player list when their time is up.
	 */
	void
	server::handle_muted (scheduler_task& task)
	{
		server &srv = *(static_cast<server *> (task.get_context ()));
		if (!srv.is_running () || srv.is_shutting_down ())
			return;
		
		if (srv.muted.empty ()) return;
		
		std::lock_guard<std::mutex> guard {srv.mute_lock};
		for (auto itr = srv.muted.begin (); itr != srv.muted.end (); )
			{
				muted_player& m = *itr;
				if ((-- m.seconds) <= 0)
					{
						player *pl = srv.get_players ().find (m.username);
						if (pl)
							pl->message ("§9You are no longer muted§3.");
						itr = srv.muted.erase (itr);
					}
				else
					{
						++ itr;
					}
			}
	}
	
	
	
/*******************************************************************************
		
		<init, destroy> pairs:
		
********************************************************************************/
	
//----
	// init_config (), destroy_config ():
	/* 
	 * Loads settings from the configuration file ("config.yaml",
	 * in YAML form) into the server's `cfg' structure. If "config.yaml"
	 * does not exist, it will get created with default settings.
	 */
	
	static void
	default_config (server_config& out)
	{
		std::strcpy (out.srv_name, "hCraft server");
		std::strcpy (out.srv_motd, "§6A new §ehCraft §6server is born§f!");
		out.max_players = 12;
		std::strcpy (out.main_world, "main");
		out.online_mode = true;
		
		std::strcpy (out.ip, "0.0.0.0");
		out.port = 25565;
		
		out.self_highlight_color = 'a';
		out.name_highlight_color = 'd';
	}
	
	static void
	write_config (logger& log, const server_config& in)
	{
		cfg::group root {1};
		
		{
			cfg::group *grp_general = new cfg::group ();
			
			grp_general->add_string ("server-name", in.srv_name);
			grp_general->add_string ("server-motd", in.srv_motd);
			grp_general->add_boolean ("online-mode", in.online_mode);
			grp_general->add_integer ("max-players", in.max_players);
			grp_general->add_string ("main-world", in.main_world);
			
			root.add ("general", grp_general);
		}
		
		{
			cfg::group *grp_network = new cfg::group ();
			
			grp_network->add_string ("ip-address", in.ip);
			grp_network->add_integer ("port", in.port);
			
			root.add ("network", grp_network);
		}
		
		{
			cfg::group *grp_chat = new cfg::group ();
			
			grp_chat->add_string ("self-highlight-color",
				(in.self_highlight_color == 0) ? "" : std::string (1, in.self_highlight_color));
			grp_chat->add_string ("name-highlight-color",
				(in.name_highlight_color == 0) ? "" : std::string (1, in.name_highlight_color));
			
			root.add ("chat",grp_chat);
		}
		
		try
			{
				std::ofstream fs {"data/config.cfg"};
				if (!fs) throw server_error ("failed to save configuration file");
				root.write_to (fs);
				fs.close ();
			}
		catch (const std::exception&)
			{
				log (LT_ERROR) << "Failed to save configuration file to \"data/config.cfg\"" << std::endl;
			}
	}
	
	
	static void
	_cfg_read_general_grp (logger& log, cfg::group *grp_general, server_config& out)
	{
		std::string str;
		long long num;
		bool error = false;
		bool bl;
		
		// server name
		if (grp_general->try_get_string ("server-name", str))
			{
				if (str.size () > 0 && str.size() <= 80)
					std::strcpy (out.srv_name, str.c_str ());
				else
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"server.general\":" << std::endl;
						log (LT_INFO) << " - \"server-name\" must contain at "
														 "least one character and no more than 80." << std::endl;
						error = true;
					}
			}
		
		// server motd
		if (grp_general->try_get_string ("server-motd", str))
			{
				if (str.size() <= 80)
					std::strcpy (out.srv_motd, str.c_str ());
				else
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"server.general\":" << std::endl;
						log (LT_INFO) << " - \"server-motd\" must contain no more than 80 characters." << std::endl;
						error = true;
					}
			}
		
		// online mode
		if (grp_general->try_get_boolean ("online-mode", bl))
			out.online_mode = bl;
		if (!out.online_mode)
			log (LT_INFO) << "The server is running in offline mode! (Authentication/encryption will not take place)" << std::endl;
		
		// max players
		if (grp_general->try_get_integer ("max-players", num))
			{
				if (num > 0 && num <= 1024)
					out.max_players = num;
				else
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"server.general\":" << std::endl;
						log (LT_INFO) << " - \"max-players\" must be in the range of 1-1024." << std::endl;
						error = true;
					}
			}
		
		// main world
		if (grp_general->try_get_string ("main-world", str))
			{
				if (world::is_valid_name (str.c_str ()))
					std::strcpy (out.main_world, str.c_str ());
				else
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"server.general\":" << std::endl;
						log (LT_INFO) << " - \"main-world\" is not a valid world name." << std::endl;
						error = true;
					}
			}
	}
	
	static void
	_cfg_read_network_grp (logger& log, cfg::group *grp_network, server_config& out)
	{
		std::string str;
		long long num;
		bool error = false;
		
		// ip address
		if (grp_network->try_get_string ("ip-address", str))
			{
				if (str.size () == 0)
					std::strcpy (out.ip, "0.0.0.0");
				else
					{
						struct in_addr addr;
						if (inet_pton (AF_INET, str.c_str (), &addr) == 1)
							std::strcpy (out.ip, str.c_str ());
						else
							{
								if (!error)
									log (LT_ERROR) << "Config: at group \"server.network\":" << std::endl;
								log (LT_INFO) << " - \"ip-address\" is invalid." << std::endl;
								error = true;
							}
					}
			}
		
		// port
		if (grp_network->try_get_integer ("port", num))
			{
				if (num >= 0 && num <= 65535)
					out.port = num;
				else
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"server.network\":" << std::endl;
						log (LT_INFO) << " - \"port\" must be in the range of 0-65535." << std::endl;
						error = true;
					}
			}
	}
	
	static void
	_cfg_read_chat_grp (logger& log, cfg::group *grp_chat, server_config& out)
	{
		std::string str;
		bool error = false;
		
		// self-highlight-color
		if (grp_chat->try_get_string ("self-highlight-color", str))
			{
				if (str.empty ())
					out.self_highlight_color = 0;
				else if (str.size() == 1)
					{
						int code = str[0];
						if (!std::isxdigit (code))
							throw server_error ("");
						out.self_highlight_color = code;
					}
				else
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"server.chat\":" << std::endl;
						log (LT_INFO) << " - \"self-highlight-color\" must be a hexadecimal color code." << std::endl;
						error = true;
					}
			}
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"server.chat\":" << std::endl;
				log (LT_INFO) << " - \"self-highlight-color\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		
		// name-highlight-color
		if (grp_chat->try_get_string ("name-highlight-color", str))
			{
				if (str.empty ())
					out.name_highlight_color = 0;
				else if (str.size() == 1)
					{
						int code = str[0];
						if (!std::isxdigit (code))
							throw server_error ("");
						out.name_highlight_color = code;
					}
				else
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"server.chat\":" << std::endl;
						log (LT_INFO) << " - \"name-highlight-color\" must be a hexadecimal color code." << std::endl;
						error = true;
					}
			}
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"server.chat\":" << std::endl;
				log (LT_INFO) << " - \"name-highlight-color\" is either invalid or does not exist." << std::endl;
				error = true;
			}
	}
	
	static void
	_cfg_read_root_grp (logger& log, cfg::group *root, server_config& out)
	{
		try
			{
				cfg::group *grp_general = root->find_group ("general");
				if (!grp_general) throw server_error ("not found");
				_cfg_read_general_grp (log, grp_general, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"server.general\" not found, using defaults" << std::endl;
			}
		
		try
			{
				cfg::group *grp_network = root->find_group ("network");
				if (!grp_network) throw server_error ("not found");
				_cfg_read_network_grp (log, grp_network, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"server.network\" not found, using defaults" << std::endl;
			}
		
		try
			{
				cfg::group *grp_chat = root->find_group ("chat");
				if (!grp_chat) throw server_error ("not found");
				_cfg_read_chat_grp (log, grp_chat, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"server.chat\" not found, using defaults" << std::endl;
			}
	}
	
	static void
	read_config (logger& log, std::ifstream& fs, server_config& out)
	{
		try
			{
				cfg::group *root = cfg::group::read_from (fs);
				_cfg_read_root_grp (log, root, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"server\" not found, using defaults" << std::endl;
			}
	}
	
	
	void
	server::init_config ()
	{
		default_config (this->cfg);
		
		log () << "Loading configuration from \"data/config.cfg\"" << std::endl;
		
		// check if the file exists
		{
			std::ifstream strm ("data/config.cfg");
			if (strm.is_open ())
				{
					read_config (this->log, strm, this->cfg);
					strm.close ();
					return;
				}
		}
		
		log () << "Configuration file does not exist, creating one with default settings." << std::endl;
		write_config (this->log, this->cfg);
	}
	
	void
	server::destroy_config ()
		{ }
	
	
	
//---
	// init_sql (), destroy_sql ():
	/* 
	 * Loads the server's database.
	 */
	void
	server::init_sql ()
	{
		log () << "Opening SQL database (at \"database.sqlite\")" << std::endl;
		
		sql::connection& conn = this->sql ().pop ();
		conn.execute (
			
			"CREATE TABLE IF NOT EXISTS `players` ("
				"`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
				"`name` TEXT COLLATE NOCASE, "
				"`nick` TEXT, "
				"`ip` TEXT, "
				"`op` INTEGER, "
				"`rank` TEXT, "
				"`blocks_destroyed` INTEGER, "
				"`blocks_created` INTEGER, "
				"`messages_sent` INTEGER, "
				"`first_login` UNSIGNED BIG INT, "
				"`last_login` UNSIGNED BIG INT, "
				"`login_count` INTEGER, "
				"`balance` DOUBLE, "
				"`banned` INTEGER); "
			
			"CREATE TABLE IF NOT EXISTS `kicks` ("
				"`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
				"`target` TEXT COLLATE NOCASE, "
				"`kicker` TEXT COLLATE NOCASE, "
				"`reason` TEXT, "
				"`kick_time` INTEGER); "
			
			"CREATE TABLE IF NOT EXISTS `bans` ("
				"`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
				"`target` TEXT COLLATE NOCASE, "
				"`banner` TEXT COLLATE NOCASE, "
				"`reason` TEXT, "
				"`ban_time` INTEGER); "
			
			"CREATE TABLE IF NOT EXISTS `unbans` ("
				"`id` INTEGER PRIMARY KEY AUTOINCREMENT, "
				"`target` TEXT COLLATE NOCASE, "
				"`unbanner` TEXT COLLATE NOCASE, "
				"`reason` TEXT, "
				"`unban_time` INTEGER); "
			
			"CREATE TABLE IF NOT EXISTS `player-logout-data` ("
				"`name` TEXT COLLATE NOCASE, "
				"`world` TEXT, "
				"`pos_x` DOUBLE, "
				"`pos_y` DOUBLE, "
				"`pos_z` DOUBLE, "
				"`pos_r` DOUBLE, "
				"`pos_l` DOUBLE, "
				"`gm` INTEGER); "
				
			
			"CREATE TABLE IF NOT EXISTS `autoload-worlds` (`name` TEXT);");
		
		
		this->sql ().push (conn); 
	}
	
	void
	server::destroy_sql ()
	{
		this->sql ().clear ();
	}
	
	
	
//---
	// init_core (), destroy_core ():
	/* 
	 * Initializes various data structures and variables needed by the server.
	 */
	
	void
	server::init_core ()
	{
		if (evthread_use_pthreads ())
			throw server_error ("libevent cannot be set up for use in a multithreaded environment");
		
		// create directories
		mkdir ("data", 0744);
		mkdir ("data/worlds", 0744);
		mkdir ("data/perms", 0744);
		
		// authentication/encryption
		{
			this->auth.start ();
			
			CryptoPP::AutoSeededRandomPool rnd;
			this->rsa_private.GenerateRandomWithKeySize (rnd, 1024);
			
			// server id
			this->server_id.clear ();
			std::minstd_rand srnd (utils::ns_since_epoch ());
			std::uniform_int_distribution<> dis (0, 15);
			for (int i = 0; i < 16; ++i)
				{
					int j = dis (srnd);
					this->server_id.push_back ((j >= 10) ? ('a' + (j - 10)) : ('0' + j));
				}
			log (LT_DEBUG) << "Server ID: " << this->server_id << std::endl;
		}
		
		this->sched.start ();
		
		this->players = new playerlist ();
		this->id_counter = 0;
		
		physics_block::init_blocks ();
		
		this->get_scheduler ().new_task (hCraft::server::cleanup_players, this)
			.run_forever (10000);
		
		this->get_scheduler ().new_task (hCraft::server::handle_muted, this)
			.run_forever (1000);
		
		// create pooled threads
		this->tpool.start (6);
	}
	
	void
	server::destroy_core ()
	{
		log (LT_SYSTEM) << "Stopping threading pools and schedulers" << std::endl;
		this->tpool.stop ();
		physics_block::destroy_blocks ();
		this->sched.stop ();
		this->auth.stop ();
		
		delete this->players;
	}
	
	
	
//---
	// init_commands (), destroy_commands ():
	/* 
	 * Loads up commands.
	 */
	
	static void _add_command (permission_manager& perm_man, command_list *dest,
		const char *name)
	{
		command *cmd = command::create (name);
		if (cmd)
			{
				dest->add (cmd);
			}
	}
	
	void
	server::init_commands ()
	{
		this->commands = new command_list ();
		
		_add_command (this->perms, this->commands, "help");
		_add_command (this->perms, this->commands, "me");
		_add_command (this->perms, this->commands, "ping");
		_add_command (this->perms, this->commands, "wcreate");
		_add_command (this->perms, this->commands, "wload");
		_add_command (this->perms, this->commands, "world");
		_add_command (this->perms, this->commands, "tp");
		_add_command (this->perms, this->commands, "nick");
		_add_command (this->perms, this->commands, "wunload");
		_add_command (this->perms, this->commands, "physics");
		_add_command (this->perms, this->commands, "select");
		_add_command (this->perms, this->commands, "fill");
		_add_command (this->perms, this->commands, "gm");
		_add_command (this->perms, this->commands, "cuboid");
		_add_command (this->perms, this->commands, "line");
		_add_command (this->perms, this->commands, "bezier");
		_add_command (this->perms, this->commands, "aid");
		_add_command (this->perms, this->commands, "circle");
		_add_command (this->perms, this->commands, "ellipse");
		_add_command (this->perms, this->commands, "sphere");
		_add_command (this->perms, this->commands, "polygon");
		_add_command (this->perms, this->commands, "curve");
		_add_command (this->perms, this->commands, "rank");
		_add_command (this->perms, this->commands, "status");
		_add_command (this->perms, this->commands, "money");
		_add_command (this->perms, this->commands, "kick");
		_add_command (this->perms, this->commands, "ban");
		_add_command (this->perms, this->commands, "unban");
		_add_command (this->perms, this->commands, "mute");
		_add_command (this->perms, this->commands, "wsetspawn");
		_add_command (this->perms, this->commands, "unmute");
		_add_command (this->perms, this->commands, "say");
	}
	
	void
	server::destroy_commands ()
	{
		delete this->commands;
	}
	
	
	
//---
	// init_ranks (), destroy_ranks ():
	/* 
	 * Reads reads and their associated permissions from "ranks.yaml".
	 * If the file does not exist, it will get created with default settings.
	 */
	
	static void
	create_default_ranks (group_manager& groups)
	{
		group* grp_spectator = groups.add (0, "spectator");
		grp_spectator->color = '8';
		grp_spectator->can_build = false;
		grp_spectator->add ("command.info.help");
		grp_spectator->msuffix = "§f:";
		
		group* grp_guest = groups.add (1, "guest");
		grp_guest->color = '7';
		grp_guest->add ("command.info.help");
		grp_guest->msuffix = "§f:";
		
		group* grp_member = groups.add (2, "member");
		grp_member->color = 'a';
		grp_member->inherit (grp_guest);
		grp_member->add ("command.chat.me");
		grp_member->add ("command.info.status");
		grp_member->add ("command.info.status.nick");
		grp_member->add ("command.info.status.balance");
		grp_member->add ("command.info.money");
		grp_member->add ("command.info.money.pay");
		grp_member->msuffix = "§f:";
		
		group* grp_builder = groups.add (3, "builder");
		grp_builder->color = '2';
		grp_builder->inherit (grp_member);
		grp_builder->add ("command.world.world");
		grp_builder->add ("command.world.tp");
		grp_builder->add ("command.draw.cuboid");
		grp_builder->add ("command.draw.aid");
		grp_builder->msuffix = "§f:";
		
		group* grp_designer = groups.add (4, "designer");
		grp_designer->color = 'b';
		grp_designer->inherit (grp_builder);
		grp_designer->add ("command.draw.select");
		grp_designer->add ("command.draw.fill");
		grp_designer->add ("command.draw.sphere");
		grp_designer->msuffix = "§f:";
		
		group* grp_architect = groups.add (5, "architect");
		grp_architect->color = '3';
		grp_architect->inherit (grp_designer);
		grp_architect->add ("command.draw.circle");
		grp_architect->add ("command.draw.line");
		grp_architect->add ("command.draw.bezier");
		grp_architect->add ("command.draw.ellipse");
		grp_architect->add ("command.draw.polygon");
		grp_architect->add ("command.draw.curve");
		grp_architect->msuffix = "§f:";
		
		group* grp_moderator = groups.add (6, "moderator");
		grp_moderator->color = 'c';
		grp_moderator->inherit (grp_designer);
		grp_moderator->add ("command.misc.ping");
		grp_moderator->add ("command.info.status.rank");
		grp_moderator->add ("command.info.status.logins");
		grp_moderator->add ("command.admin.mute");
		grp_moderator->add ("command.admin.unmute");
		grp_moderator->msuffix = "§f:";
		
		group* grp_admin = groups.add (7, "admin");
		grp_admin->color = '4';
		grp_admin->inherit (grp_architect);
		grp_admin->inherit (grp_moderator);
		grp_admin->add ("command.world.tp.others");
		grp_admin->add ("command.admin.gm");
		grp_admin->add ("command.admin.rank");
		grp_admin->add ("command.info.status.*");
		grp_admin->add ("command.info.money.*");
		grp_admin->add ("command.admin.kick");
		grp_admin->add ("command.admin.ban");
		grp_admin->add ("command.chat.say.*");
		grp_admin->text_color = 'c';
		grp_admin->msuffix = "§f:";
		grp_admin->color_codes = true;
		
		group* grp_executive = groups.add (8, "executive");
		grp_executive->color = 'e';
		grp_executive->inherit (grp_admin);
		grp_executive->add ("command.world.wcreate");
		grp_executive->add ("command.world.wload");
		grp_executive->add ("command.world.wunload");
		grp_executive->add ("command.world.physics");
		grp_executive->add ("command.world.wsetspawn");
		grp_executive->add ("command.chat.nick");
		grp_executive->add ("command.admin.unban");
		grp_executive->text_color = 'c';
		grp_executive->msuffix = "§f:";
		grp_executive->color_codes = true;
		
		group* grp_owner = groups.add (9, "owner");
		grp_owner->color = '6';
		grp_owner->text_color = 'b';
		grp_owner->add ("*");
		grp_owner->msuffix = "§f:";
		grp_owner->color_codes = true;
		
		// ladders
		group_ladder *ld_normal = groups.add_ladder ("normal");
		group_ladder *ld_staff = groups.add_ladder ("staff");
		ld_normal->insert (grp_spectator);
		ld_normal->insert (grp_guest);
		ld_normal->insert (grp_member);
		ld_normal->insert (grp_builder);
		ld_normal->insert (grp_designer);
		ld_normal->insert (grp_architect);
		ld_staff->insert (grp_moderator);
		ld_staff->insert (grp_admin);
		ld_staff->insert (grp_executive);
		ld_staff->insert (grp_owner);
		
		
		groups.default_rank.set ("@guest[normal]", groups);
	}
	
	
	static void
	write_ranks (logger& log, group_manager& groups)
	{
		groups.save ("data/ranks.cfg");
	}
	
	static void
	read_ranks (logger& log, group_manager& groups)
	{
		groups.load ("data/ranks.cfg");
	}
	
	
	void
	server::init_ranks ()
	{
		std::ifstream istrm ("data/ranks.cfg");
		if (istrm)
			{
				istrm.close ();
				
				log () << "Loading ranks from \"data/ranks.cfg\"" << std::endl;
				read_ranks (this->log, this->groups);
				log (LT_INFO) << " - Loaded " << this->groups.size () << " groups." << std::endl;
				return;
			}
		
		create_default_ranks (this->groups);
		log () << "\"data/ranks.cfg\" does not exist, creating..." << std::endl;
		write_ranks (this->log, this->groups);
	}
	
	void
	server::destroy_ranks ()
	{
		this->groups.clear ();
	}
	
	
	
//---
	// init_worlds (), destroy_worlds ():
	/* 
	 * Loads up and initializes worlds.
	 */
	
	void
	server::init_worlds ()
	{
		std::vector<std::string> to_load;
		
		world *main_world;
		std::string prov_name;
		entity_pos spos;
		
		log () << "Loading worlds:" << std::endl;
		
		// load main world
		prov_name = world_provider::determine ("data/worlds", this->get_config ().main_world);
		if (prov_name.empty ())
			{
				// main world does not exist
				log () << " - Main world does not exist, creating..." << std::endl;
				main_world = new world (*this, this->get_config ().main_world, this->log, 
					world_generator::create ("overhang"),
					world_provider::create ("hw", "data/worlds", this->get_config ().main_world));
				main_world->set_size (192, 192);
				main_world->prepare_spawn (15, true);
				main_world->save_all ();
			}
		else
			{
				log () << " - Loading \"" << this->get_config ().main_world << "\"" << std::endl;
				world_provider *prov = world_provider::create (prov_name.c_str (),
					"data/worlds", this->get_config ().main_world);
				if (!prov)
					throw server_error ("failed to load main world (invalid provider)");
				
				const world_information& winf = prov->info ();
				world_generator *gen = world_generator::create (winf.generator.c_str (), winf.seed);
				if (!gen)
					{
						delete prov;
						throw server_error ("failed to load main world (invalid generator)");
					}
				
				main_world = new world (*this, this->get_config ().main_world, this->log, gen, prov);
				main_world->set_size (winf.width, winf.depth);
				main_world->set_spawn (winf.spawn_pos);
				main_world->prepare_spawn (10, false);
			}
		main_world->start ();
		this->add_world (main_world);
		this->main_world = main_world;
		
		// load worlds from the autoload list.
		{
			auto& conn = this->sql ().pop ();
			auto stmt = conn.query ("SELECT * FROM `autoload-worlds`");
			
			sql::row row;
			while (stmt.step (row))
				to_load.push_back (row.at (0).as_cstr ());
			
			this->sql ().push (conn);
		}
		for (std::string& wname : to_load)
			{
				std::string prov_name = world_provider::determine ("data/worlds", wname.c_str ());
				if (prov_name.empty ())
					{
						log (LT_WARNING) << " - World \"" << wname << "\" does not exist." << std::endl;
						continue;
					}
				
				world_provider *prov = world_provider::create (prov_name.c_str (),
					"data/worlds", wname.c_str ());
				if (!prov)
					{
						log (LT_ERROR) << " - Failed to load world \"" << wname
							<< "\": Invalid provider." << std::endl;
						continue;
					}
				
				const world_information& winf = prov->info ();
				world_generator *gen = world_generator::create (winf.generator.c_str (), winf.seed);
				if (!gen)
					{
						delete prov;
						log (LT_ERROR) << " - Failed to load world \"" << wname
							<< "\": Invalid generator." << std::endl;
						continue;
					}
				
				log () << " - Loading \"" << wname << "\"" << std::endl;
				world *wr = new world (*this, wname.c_str (), this->log, gen, prov);
				wr->set_size (winf.width, winf.depth);
				
				wr->set_spawn (winf.spawn_pos);
				wr->prepare_spawn (10, false);
				wr->start ();
				if (!this->add_world (wr))
					{
						log (LT_ERROR) << "   - Failed to load world \"" << wname << "\": Already loaded." << std::endl;
						delete wr;
						continue;
					}
			}
		
		// start physics
		this->global_physics.set_thread_count (1);
		
		// start the generator
		this->cgen.start ();
	}
	
	void
	server::destroy_worlds ()
	{
		log (LT_SYSTEM) << "Saving worlds..." << std::endl;
		
		// generator
		this->cgen.stop ();
		
		// clear worlds
		{
			std::lock_guard<std::mutex> guard {this->world_lock};
			for (auto itr = this->worlds.begin (); itr != this->worlds.end (); ++itr)
				{
					world *w = itr->second;
					log (LT_SYSTEM) << "  - Saving world \"" << w->get_name () << "\"" << std::endl;
					w->stop ();
					w->save_all ();
					delete w;
				}
			this->worlds.clear ();
		}
	}
	
	
	
//----
	// init_workers (), destroy_workers ():
	/* 
	 * Creates and starts server workers.
	 * The total number of workers created depends on how many cores the user
	 * has installed on their system, which means that on a multi-core system,
	 * the work will be parallelized between all cores.
	 */
	
	void
	server::init_workers ()
	{
		this->worker_count = std::thread::hardware_concurrency ();
		if (this->worker_count <= 1)
			this->worker_count = 2;
		this->workers.reserve (this->worker_count);
		log () << "Creating " << this->worker_count << " server worker(s)." << std::endl;
		
		this->workers_stop = false;
		this->workers_ready = false;
		for (int i = 0; i < this->worker_count; ++i)
			{
				struct event_base *base = event_base_new ();
				if (!base)
					{
						this->workers_stop = true;
						for (auto itr = this->workers.begin (); itr != this->workers.end (); ++itr)
							{
								worker &w = *itr;
								if (w.th.joinable ())
									w.th.join ();
								
								event_base_free (w.evbase);
							}
						
						throw server_error ("failed to create workers");
					}
				
				std::thread th (std::bind (std::mem_fn (&hCraft::server::work), this));
				this->workers.push_back (worker (base, std::move (th)));
			}
		
		this->workers_ready = true;
	}
	
	void
	server::destroy_workers ()
	{
		log (LT_SYSTEM) << "Destroying server workers.." << std::endl;
		
		this->workers_stop = true;
		for (auto itr = this->workers.begin (); itr != this->workers.end (); ++itr)
			{
				worker &w = *itr;
				
				event_base_loopbreak (w.evbase);
				if (w.th.joinable ())
					w.th.join ();
			}
	}
	
	
	
//----
	// init_listener (), destroy_listener ():
	/* 
	 * Creates the listening socket and starts listening on the IP address and
	 * port number specified by the user in the configuration file for incoming
	 * connections.
	 */
	void
	server::init_listener ()
	{
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port   = htons (this->cfg.port);
		inet_pton (AF_INET, this->cfg.ip, &addr.sin_addr);
		
		worker &w = this->get_min_worker ();
		this->listener = evconnlistener_new_bind (w.evbase,
			&hCraft::server::handle_accept, this, LEV_OPT_CLOSE_ON_FREE
			| LEV_OPT_REUSEABLE, -1, (struct sockaddr *)&addr, sizeof addr);
		if (!this->listener)
			throw server_error ("failed to create listening socket (port taken?)");
		
		++ w.event_count;
		log () << "Started listening on port " << this->cfg.port << "." << std::endl;
	}
	
	void
	server::destroy_listener ()
	{
		evconnlistener_free (this->listener);
	}
	
	
	
//----
	// final_cleanup (), initial_cleanup ():
	
	/* 
	 * Performs cleanup on resources that can be only done before all other
	 * <init, destory> pairs have been executed.
	 */
	void
	server::initial_cleanup ()
	{
		/* 
		 * Kick all connected players.
		 */
		{
			std::unordered_set<player *> remove_vec;
			
			log (LT_SYSTEM) << "Counting disconnected players..." << std::endl;
			{
				std::lock_guard<std::mutex> guard {this->player_lock};
		
				this->get_players ().remove_if (
					[&remove_vec] (player *pl)
						{
							remove_vec.insert (pl);
							return true;
						});
			
				for (auto itr = this->connecting.begin (); itr != this->connecting.end (); ++itr)
					remove_vec.insert (*itr);
				this->connecting.clear ();
			
				for (auto itr = std::begin (this->to_destroy);
						 itr != std::end (this->to_destroy);
						 ++itr)
					remove_vec.insert (*itr);
				this->to_destroy.clear ();
			}
			
			log (LT_INFO) << "  - " << remove_vec.size () << " instance(s)." << std::endl;
			for (player *pl : remove_vec)
				delete pl;
			log (LT_INFO) << "  - All player instances have been destroyed." << std::endl;
		}
	}
	
	/* 
	 * Performs cleanup on resources that can only be done after all other
	 * <init, destory> pairs have been executed.
	 */
	void
	server::final_cleanup ()
	{
		for (auto itr = this->workers.begin (); itr != this->workers.end (); ++itr)
			{
				worker &w = *itr;
				event_base_free (w.evbase);
			}
		this->workers.clear ();
	}
}


