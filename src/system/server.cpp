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

#include "system/server.hpp"
#include "util/utils.hpp"
#include "physics/blocks/physics_block.hpp"
#include "util/config.hpp"
#include <memory>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <algorithm>
#include <event2/thread.h>
#include <soci/mysql/soci-mysql.h>

#include <iostream> // DEBUG


namespace hCraft {
	
#define SQL_POOL_SIZE 8
	
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
			spool (SQL_POOL_SIZE),
			perms (),
			groups (perms),
			global_physics (*this)
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
		
		this->inits.push_back (initializer (
			std::bind (std::mem_fn (&hCraft::server::init_irc), this),
			std::bind (std::mem_fn (&hCraft::server::destroy_irc), this)));
		
		this->running = false;
		this->ircc = nullptr;
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
	 * Saves all currently loaded worlds.
	 */
	void
	server::save_worlds (scheduler_task& task)
	{
		server &srv = *(static_cast<server *> (task.get_context ()));
		if (!srv.is_running () || srv.is_shutting_down ())
			return;
		
		srv.log (LT_SYSTEM) << "Saving all loaded worlds [Autosave]" << std::endl;
		srv.get_players ().message (
			"§5Autosave: §dSaving all loaded worlds");
		srv.get_worlds ().all (
			[] (world *w) {
				w->save_all ();
			});
		srv.get_players ().message (
			"§5Autosave: §dAll worlds have been saved");
	}
	
	/* 
	 * Pings all online players (every eight seconds).
	 */
	void
	server::ping_players (scheduler_task& task)
	{
		server &srv = *(static_cast<server *> (task.get_context ()));
		if (!srv.is_running () || srv.is_shutting_down ())
			return;
		
		srv.get_players ().all (
			[] (player *pl)
				{
					pl->try_ping (8000);
				});
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
	 * Returns a unique number that can be used for entity identification.
	 */
	int
	server::register_entity (entity *e)
	{
		std::lock_guard<std::mutex> guard {this->id_lock};
		int id = this->entity_id_counter ++;
		if (this->entity_id_counter < 0)
			this->entity_id_counter = 0; // overflow.
		this->entity_map[id] = e;
		return id;
	}
	
	/* 
	 * Removes an entity from the entity\id map.
	 */
	void
	server::deregister_entity (entity *e)
	{
		this->deregister_entity (e->get_eid ());
	}
	
	void
	server::deregister_entity (int eid)
	{
		std::lock_guard<std::mutex> guard {this->id_lock};
		auto itr = this->entity_map.find (eid);
		if (itr != this->entity_map.end ())
			this->entity_map.erase (itr);
	}
	
	
	/* 
	 * Returns the player\entity associated with the given ID.
	 */
	entity*
	server::entity_by_id (int id)
	{
		auto itr = this->entity_map.find (id);
		if (itr == this->entity_map.end ())
			return nullptr;
		return itr->second;
	}
	
	player*
	server::player_by_id (int id)
	{
		entity *e = this->entity_by_id (id);
		if (!e || e->get_type () != ET_PLAYER)
			return nullptr;
		return (player *)e;
	}
	
	
	
	bool
	server::register_world (world *w)
	{
		std::lock_guard<std::mutex> guard {this->id_lock};
		int id = this->world_id_counter ++;
		if (this->world_id_counter < 0)
			this->world_id_counter = 0; // overflow.
		this->world_map[id] = w;
		
		if (!this->get_worlds ().add (w))
			{
				this->world_map.erase (id);
				-- this->world_id_counter;
				return false;
			}
		
		w->id = id;
		return true;
	}
	
	void
	server::deregister_world (world *w)
	{
		std::lock_guard<std::mutex> guard {this->id_lock};
		auto itr = this->world_map.find (w->id);
		if (itr != this->world_map.end ())
			this->world_map.erase (itr);
	}
	
	world*
	server::world_by_id (int id)
	{
		auto itr = this->world_map.find (id);
		if (itr == this->world_map.end ())
			return nullptr;
		return itr->second;
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
		std::strcpy (out.main_world, "Main");
		out.online_mode = true;
		out.load_prev_pos = false;
		
		std::strcpy (out.ip, "0.0.0.0");
		out.port = 25565;
		
		out.self_highlight_color = 'a';
		out.name_highlight_color = 'd';
		
		out.db_name = "hCraftDB1";
		out.db_user = "root";
		out.db_pass = "";
		out.db_host = "localhost";
		out.db_port = 3306;
		
		out.irc_enabled = false;
		out.irc_net = "irc.panicirc.net";
		out.irc_port = 6667;
		out.irc_chan = "#channel";
		out.irc_nick = "hCraftBot";
		
		out.dcmds.clear ();
		out.dcmds.insert ("realm");
		out.dcmds.insert ("money");
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
			grp_general->add_boolean ("load-prev-pos", in.load_prev_pos);
			
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
			
			root.add ("chat", grp_chat);
		}
		
		{
			cfg::group *grp_sql = new cfg::group ();
		
			grp_sql->add_string ("database", in.db_name);
			grp_sql->add_string ("user", in.db_user);
			grp_sql->add_string ("pass", in.db_pass);
			grp_sql->add_string ("host", in.db_host);
			grp_sql->add_integer ("port", in.db_port);
		
			root.add ("sql", grp_sql);
		}
		
		{
			cfg::group *grp_irc = new cfg::group ();
			
			grp_irc->add_boolean ("enabled", in.irc_enabled);
			grp_irc->add_string ("nick", in.irc_nick);
			grp_irc->add_string ("network", in.irc_net);
			grp_irc->add_integer ("port", in.irc_port);
			grp_irc->add_string ("channel", in.irc_chan);
		
			root.add ("irc", grp_irc);
		}
		
		{
			cfg::array *arr_dcmds = new cfg::array ();
			
			for (const std::string& str : in.dcmds)
				arr_dcmds->add_string (str);
			
			root.add ("disabled-commands", arr_dcmds);
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
			
		// load prev pos
		if (grp_general->try_get_boolean ("load-prev-pos", bl))
			out.load_prev_pos = bl;
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
	_cfg_read_sql_grp (logger& log, cfg::group *grp_sql, server_config& out)
	{
		std::string str;
		long long int num;
		bool error = false;
		
		// database
		if (grp_sql->try_get_string ("database", str))
			{
				if (str.empty ())
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"sql\":" << std::endl;
						log (LT_INFO) << " - \"database\" cannot be empty." << std::endl;
						error = true;
					}
				out.db_name = str;
			}
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"sql\":" << std::endl;
				log (LT_INFO) << " - \"database\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		// user
		if (grp_sql->try_get_string ("user", str))
			out.db_user = str;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"sql\":" << std::endl;
				log (LT_INFO) << " - \"user\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		// pass
		if (grp_sql->try_get_string ("pass", str))
			out.db_pass = str;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"sql\":" << std::endl;
				log (LT_INFO) << " - \"pass\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		// host
		if (grp_sql->try_get_string ("host", str))
			out.db_host = str;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"sql\":" << std::endl;
				log (LT_INFO) << " - \"host\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		// port
		if (grp_sql->try_get_integer ("port", num))
			out.db_port = num;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"sql\":" << std::endl;
				log (LT_INFO) << " - \"port\" is either invalid or does not exist." << std::endl;
				error = true;
			}
	}
	
	static void
	_cfg_read_irc_grp (logger& log, cfg::group *grp_irc, server_config& out)
	{
		std::string str;
		long long int num;
		bool error = false;
		bool b;
		
		// enabled
		if (grp_irc->try_get_boolean ("enabled", b))
			out.irc_enabled = b;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"irc\":" << std::endl;
				log (LT_INFO) << " - \"enabled\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		// nick
		if (grp_irc->try_get_string ("nick", str))
			out.irc_nick = str;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"irc\":" << std::endl;
				log (LT_INFO) << " - \"nick\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		// network
		if (grp_irc->try_get_string ("network", str))
			out.irc_net = str;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"irc\":" << std::endl;
				log (LT_INFO) << " - \"network\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		// port
		if (grp_irc->try_get_integer ("port", num))
			out.irc_port = num;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"irc\":" << std::endl;
				log (LT_INFO) << " - \"port\" is either invalid or does not exist." << std::endl;
				error = true;
			}
		
		// channel
		if (grp_irc->try_get_string ("channel", str))
			out.irc_chan = str;
		else
			{
				if (!error)
					log (LT_ERROR) << "Config: at group \"irc\":" << std::endl;
				log (LT_INFO) << " - \"channel\" is either invalid or does not exist." << std::endl;
				error = true;
			}
	}
	
	static void
	_cfg_read_dcmds_arr (logger& log, cfg::array *arr_dcmds, server_config& out)
	{
		out.dcmds.clear ();
		for (cfg::value *elem : *arr_dcmds)
			{
				if (elem->type () != cfg::CFG_STRING)
					throw server_error ("expected string");
				
				const std::string& cmd = (dynamic_cast<cfg::string *> (elem))->val ();
				out.dcmds.insert (cmd);
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
				log (LT_WARNING) << "Config: Group \"general\" not found or invalid, using defaults" << std::endl;
			}
		
		try
			{
				cfg::group *grp_network = root->find_group ("network");
				if (!grp_network) throw server_error ("not found");
				_cfg_read_network_grp (log, grp_network, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"network\" not found or invalid, using defaults" << std::endl;
			}
		
		try
			{
				cfg::group *grp_chat = root->find_group ("chat");
				if (!grp_chat) throw server_error ("not found");
				_cfg_read_chat_grp (log, grp_chat, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"chat\" not found or invalid, using defaults" << std::endl;
			}
		
		try
			{
				cfg::group *grp_sql = root->find_group ("sql");
				if (!grp_sql) throw server_error ("not found");
				_cfg_read_sql_grp (log, grp_sql, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"sql\" not found or invalid, using defaults" << std::endl;
			}
		
		try
			{
				cfg::group *grp_irc = root->find_group ("irc");
				if (!grp_irc) throw server_error ("not found");
				_cfg_read_irc_grp (log, grp_irc, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"irc\" not found or invalid, using defaults" << std::endl;
			}
		
		try
			{
				cfg::array *arr_dcmds = root->find_array ("disabled-commands");
				if (!arr_dcmds) throw server_error ("not found");
				_cfg_read_dcmds_arr (log, arr_dcmds, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Array \"disabled-commands\" not found or invalid, using defaults" << std::endl;
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
	
	
	
	static void
	default_messages (server_messages& msgs)
	{
		msgs.server_join  = "§e[§a+§e] %target-col-nick §ehas joined the server";
		msgs.server_leave = "§e[§c-§e] %target-col-nick §ehas left the server";
		
		msgs.world_join_self = "§9 -- %target-col-nick §7went to %curr-world-col";
		msgs.world_join = "§9 -- %target-col-nick §7went to %curr-world-col";
		msgs.world_enter = "§9 -- %target-col-nick §7went to %curr-world-col";
		msgs.world_depart = "§9 -- %target-col-nick §7went to %curr-world-col";
		
		msgs.join_msg.clear ();
		msgs.join_msg.emplace_back ("§aHey %target-col§f, §ayou have logged in §b%target-login-count §atime§7(§as§7)§f!");
		msgs.join_msg.emplace_back ("§7Type §3/help §7for general help, tips and tricks§f.");
		
		msgs.help.clear ();
		msgs.help.emplace_back ("§6Displaying help§e:");
		msgs.help.emplace_back ("§f  Type §c*§daction §fto perform §daction §7(§fIRC-style§7)");
		msgs.help.emplace_back ("§6  Type §b@§cplayer message §6to send a private message");
		msgs.help.emplace_back ("§3  Type §c#§9message §3to send a global message");
		msgs.help.emplace_back ("§b  Use §7/help §2command §bto get help about §2command");
		msgs.help.emplace_back ("§e  Type §4/rules §efor a list of important rules§f!");
		msgs.help.emplace_back ("§d  Type §a/players §dto get a list of all online players");
		msgs.help.emplace_back ("§8  If you have any questions, feel free to ask a staff member§7!");
		
		msgs.rules.clear ();
		msgs.rules.emplace_back ("§cNothing in here yet§4!");
	}
	
	static void
	write_messages (server &srv)
	{
		logger &log = srv.get_logger ();
		
		cfg::group root {1};
		
		{
			cfg::group *grp_messages = new cfg::group ();
			
			grp_messages->add_string ("server-join", srv.msgs.server_join);
			grp_messages->add_string ("server-leave", srv.msgs.server_leave);
			
			grp_messages->add_separator ();
			grp_messages->add_string ("world-join", srv.msgs.world_join);
			grp_messages->add_string ("world-join-self", srv.msgs.world_join_self);
			grp_messages->add_string ("world-enter", srv.msgs.world_enter);
			grp_messages->add_string ("world-depart", srv.msgs.world_depart);
			
			grp_messages->add_separator ();
			{
				cfg::array *arr = new cfg::array ();
				for (const std::string& str : srv.msgs.join_msg)
					arr->add_string (str);
				grp_messages->add ("join-msg", arr);
			}
			
			grp_messages->add_separator ();
			{
				cfg::array *arr = new cfg::array ();
				for (const std::string& str : srv.msgs.help)
					arr->add_string (str);
				grp_messages->add ("help", arr);
			}
			
			grp_messages->add_separator ();
			{
				cfg::array *arr = new cfg::array ();
				for (const std::string& str : srv.msgs.rules)
					arr->add_string (str);
				grp_messages->add ("rules", arr);
			}
			
			root.add ("messages", grp_messages);
		}
		
		try
			{
				std::ofstream fs {"data/messages.cfg"};
				if (!fs) throw server_error ("failed to save messages file");
				root.write_to (fs);
				fs.close ();
			}
		catch (const std::exception&)
			{
				log (LT_ERROR) << "Failed to save messages file to \"data/messages.cfg\"" << std::endl;
			}
	}
	
	
	
	static void
	_cfg_msgs_read_messages_grp (logger& log, cfg::group *grp_messages, server_messages& out)
	{
		out.server_join = grp_messages->exists ("server-join", cfg::CFG_STRING)
			? grp_messages->get_string ("server-join") : "";
		out.server_leave = grp_messages->exists ("server-leave", cfg::CFG_STRING)
			? grp_messages->get_string ("server-leave") : "";
		
		out.world_join = grp_messages->exists ("world-join", cfg::CFG_STRING)
			? grp_messages->get_string ("world-join") : "";
		out.world_join_self = grp_messages->exists ("world-join-self", cfg::CFG_STRING)
			? grp_messages->get_string ("world-join-self") : "";
		out.world_enter = grp_messages->exists ("world-enter", cfg::CFG_STRING)
			? grp_messages->get_string ("world-enter") : "";
		out.world_depart = grp_messages->exists ("world-depart", cfg::CFG_STRING)
			? grp_messages->get_string ("world-depart") : "";
		
		cfg::array *arr;
		
		out.join_msg.clear ();
		arr = grp_messages->find_array ("join-msg");
		if (arr)
			{
				for (cfg::value *v : *arr)
					{
						if (v->type () == cfg::CFG_STRING)
							out.join_msg.push_back ((dynamic_cast<cfg::string *> (v))->val ());
					}
			}
		
		out.help.clear ();
		arr = grp_messages->find_array ("help");
		if (arr)
			{
				for (cfg::value *v : *arr)
					{
						if (v->type () == cfg::CFG_STRING)
							out.help.push_back ((dynamic_cast<cfg::string *> (v))->val ());
					}
			}
		
		out.rules.clear ();
		arr = grp_messages->find_array ("rules");
		if (arr)
			{
				for (cfg::value *v : *arr)
					{
						if (v->type () == cfg::CFG_STRING)
							out.rules.push_back ((dynamic_cast<cfg::string *> (v))->val ());
					}
			}
	}
	
	static void
	_cfg_msgs_read_root_grp (logger& log, cfg::group *root, server_messages& out)
	{
		try
			{
				cfg::group *grp_messages = root->find_group ("messages");
				if (!grp_messages) throw server_error ("not found");
				_cfg_msgs_read_messages_grp (log, grp_messages, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"messages\" not found or invalid, using defaults" << std::endl;
			}
	}
	
	static void
	read_messages (logger& log, std::ifstream& fs, server_messages& out)
	{
		try
			{
				cfg::group *root = cfg::group::read_from (fs);
				_cfg_msgs_read_root_grp (log, root, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"server\" not found, using defaults" << std::endl;
			}
	}
	
	
	void
	server::init_config ()
	{
		// data/server.cfg
		{
			default_config (this->cfg);
			log () << "Loading server configuration from \"data/config.cfg\"" << std::endl;
			
			std::ifstream strm ("data/config.cfg");
			if (strm.is_open ())
				{
					read_config (this->log, strm, this->cfg);
					strm.close ();
				}
			else
				{
					log () << "Configuration file does not exist, saving default." << std::endl;
					write_config (this->log, this->cfg);
				}
		}
		
		// data/messages.cfg
		{
			default_messages (this->msgs);
			log () << "Loading messages from \"data/messages.cfg\"" << std::endl;
			
			std::ifstream strm ("data/messages.cfg");
			if (strm.is_open ())
				{
					read_messages (this->log, strm, this->msgs);
					strm.close ();
				}
			else
				{
					log () << "Messages file does not exist, saving default." << std::endl;
					write_messages (*this);
				}
		}
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
		log () << "Opening SQL database" << std::endl;
		
		std::string conn_str;
		{
			std::ostringstream ss;
			ss << "dbname=" << this->cfg.db_name << " user=" << this->cfg.db_user
			   << " pass='" << this->cfg.db_pass << "' host=" << this->cfg.db_host
			   << " port=" << this->cfg.db_port;
			conn_str.assign (ss.str ());
		}
		
		// initialize pool sessions
		for (size_t i = 0; i < SQL_POOL_SIZE; ++i)
			{
				soci::session& sql = this->spool.at (i);
				sql.open (soci::mysql, conn_str);
			}
		
		{
			soci::session sql (this->spool);
			
			sql.once << "CREATE TABLE IF NOT EXISTS `players` ("
				"`id` INT UNSIGNED UNIQUE AUTO_INCREMENT PRIMARY KEY, "
				"`name` TEXT, "
				"`nick` TEXT, "
				"`ip` TEXT, "
				"`op` TINYINT, "
				"`rank` TEXT, "
				"`blocks_destroyed` INTEGER, "
				"`blocks_created` INTEGER, "
				"`messages_sent` INTEGER, "
				"`first_login` BIGINT UNSIGNED, "
				"`last_login` BIGINT UNSIGNED, "
				"`login_count` INTEGER, "
				"`balance` DOUBLE, "
				"`banned` TINYINT)";
			
			sql.once << "CREATE TABLE IF NOT EXISTS `kicks` ("
				"`id` INTEGER PRIMARY KEY NOT NULL AUTO_INCREMENT, "
				"`target` INT UNSIGNED NOT NULL, "
				"`kicker` INT UNSIGNED NOT NULL, "
				"`reason` TEXT, "
				"`kick_time` BIGINT UNSIGNED, "
				"FOREIGN KEY (`target`) REFERENCES `players`(`id`), "
				"FOREIGN KEY (`kicker`) REFERENCES `players`(`id`))";
			
			sql.once << "CREATE TABLE IF NOT EXISTS `bans` ("
				"`id` INTEGER PRIMARY KEY NOT NULL AUTO_INCREMENT, "
				"`target` INT UNSIGNED NOT NULL, "
				"`banner` INT UNSIGNED NOT NULL, "
				"`reason` TEXT, "
				"`ban_time` BIGINT UNSIGNED, "
				"FOREIGN KEY (`target`) REFERENCES `players`(`id`), "
				"FOREIGN KEY (`banner`) REFERENCES `players`(`id`))";
			
			sql.once << "CREATE TABLE IF NOT EXISTS `ip-bans` ("
				"`id` INTEGER PRIMARY KEY NOT NULL AUTO_INCREMENT, "
				"`ip` TEXT, "
				"`banner` INT UNSIGNED NOT NULL, "
				"`reason` TEXT, "
				"`ban_time` BIGINT UNSIGNED, "
				"FOREIGN KEY (`banner`) REFERENCES `players`(`id`))";
			
			sql.once << "CREATE TABLE IF NOT EXISTS `unbans` ("
				"`id` INTEGER PRIMARY KEY NOT NULL AUTO_INCREMENT, "
				"`target` INT UNSIGNED NOT NULL, "
				"`unbanner` INT UNSIGNED NOT NULL, "
				"`reason` TEXT, "
				"`unban_time` BIGINT UNSIGNED, "
				"FOREIGN KEY (`target`) REFERENCES `players`(`id`), "
				"FOREIGN KEY (`unbanner`) REFERENCES `players`(`id`))";
			
			sql.once << "CREATE TABLE IF NOT EXISTS `warns` ("
				"`target` INT UNSIGNED NOT NULL, "
				"`num` INT UNSIGNED NOT NULL, "
				"`warner` INT UNSIGNED NOT NULL, "
				"`reason` TEXT, "
				"`warn_time` BIGINT UNSIGNED, "
				"PRIMARY KEY (`target`, `num`))";
			
			sql.once << "CREATE TABLE IF NOT EXISTS `player-logout-data` ("
				"`name` TEXT , "
				"`world` TEXT, "
				"`pos_x` DOUBLE, "
				"`pos_y` DOUBLE, "
				"`pos_z` DOUBLE, "
				"`pos_r` DOUBLE, "
				"`pos_l` DOUBLE, "
				"`gm` INTEGER)";
				
			sql.once << "CREATE TABLE IF NOT EXISTS `autoload-worlds` (`name` TEXT)";
		}
	}
	
	void
	server::destroy_sql ()
	{
		// TODO: clear SQL pool
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
		mkdir ("data/undo", 0744);
		
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
		
		this->players = new player_list ();
		this->entity_id_counter = 0;
		this->world_id_counter = 0;
		
		physics_block::init_blocks ();
		
		this->get_scheduler ().new_task (hCraft::server::cleanup_players, this)
			.run_forever (10 * 1000);
		
		this->get_scheduler ().new_task (hCraft::server::handle_muted, this)
			.run_forever (1 * 1000);
		
		this->get_scheduler ().new_task (hCraft::server::save_worlds, this)
			.run_forever (5 * 60 * 1000, 5 * 60 * 1000);
		
		this->get_scheduler ().new_task (hCraft::server::ping_players, this)
			.run_forever (8 * 1000);
		
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
		std::set<std::string>& dcmds, const char *name)
	{
		if (dcmds.find (name) != dcmds.end ())
			return;
		
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
		
		_add_command (this->perms, this->commands, this->cfg.dcmds, "help");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "me");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "ping");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "wcreate");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "wload");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "goto");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "tp");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "nick");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "wunload");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "physics");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "select");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "fill");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "gm");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "cuboid");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "line");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "bezier");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "aid");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "circle");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "ellipse");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "sphere");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "kill");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "polygon");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "curve");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "rank");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "status");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "money");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "kick");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "ban");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "unban");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "mute");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "wsetspawn");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "unmute");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "say");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "block-physics");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "block-type");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "portal");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "whodid");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "undo");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "world");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "realm");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "rules");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "players");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "warn");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "warnlog");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "worlds");
		_add_command (this->perms, this->commands, this->cfg.dcmds, "top");
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
		group* grp_spectator = groups.add (0, "Spectator");
		grp_spectator->color = '8';
		grp_spectator->can_build = false;
		grp_spectator->add ("command.info.help");
		grp_spectator->add ("command.info.rules");
		grp_spectator->add ("command.info.players");
		grp_spectator->add ("command.info.warnlog");
		grp_spectator->msuffix = "§f:";
		
		group* grp_guest = groups.add (1, "Guest");
		grp_guest->color = '7';
		grp_guest->add ("command.world.world.owner.change-members");
		grp_guest->add ("command.world.realm");
		grp_guest->add ("command.world.goto");
		grp_guest->add ("command.world.worlds");
		grp_guest->add ("command.info.help");
		grp_guest->add ("command.info.rules");
		grp_guest->add ("command.info.players");
		grp_guest->add ("command.info.warnlog");
		grp_guest->add ("command.misc.kill");
		grp_guest->msuffix = "§f:";
		
		group* grp_member = groups.add (2, "Member");
		grp_member->color = 'a';
		grp_member->inherit (grp_guest);
		grp_member->add ("command.chat.me");
		grp_member->add ("command.info.status");
		grp_member->add ("command.info.status.nick");
		grp_member->add ("command.info.status.balance");
		grp_member->add ("command.info.money");
		grp_member->add ("command.info.money.pay");
		grp_member->add ("command.world.world");
		grp_member->add ("place.sign");
		grp_member->msuffix = "§f:";
		
		group* grp_builder = groups.add (3, "Builder");
		grp_builder->color = '2';
		grp_builder->inherit (grp_member);
		grp_builder->add ("command.world.tp");
		grp_builder->add ("command.draw.cuboid");
		grp_builder->add ("command.draw.aid");
		grp_builder->msuffix = "§f:";
		grp_builder->fill_limit = 2000;
		grp_builder->select_limit = 2000;
		
		group* grp_designer = groups.add (4, "Designer");
		grp_designer->color = 'b';
		grp_designer->inherit (grp_builder);
		grp_designer->add ("command.draw.select");
		grp_designer->add ("command.draw.fill");
		grp_designer->add ("command.draw.sphere");
		grp_designer->add ("command.world.block-physics");
		grp_designer->add ("command.world.block-type");
		grp_designer->msuffix = "§f:";
		grp_designer->fill_limit = 8000;
		grp_designer->select_limit = 8000;
		
		group* grp_architect = groups.add (5, "Architect");
		grp_architect->color = '3';
		grp_architect->inherit (grp_designer);
		grp_architect->add ("command.draw.circle");
		grp_architect->add ("command.draw.line");
		grp_architect->add ("command.draw.bezier");
		grp_architect->add ("command.draw.ellipse");
		grp_architect->add ("command.draw.polygon");
		grp_architect->add ("command.draw.curve");
		grp_architect->msuffix = "§f:";
		grp_architect->fill_limit = 32000;
		grp_architect->select_limit = 32000;
		
		group* grp_moderator = groups.add (6, "Moderator");
		grp_moderator->color = 'c';
		grp_moderator->inherit (grp_designer);
		grp_moderator->add ("command.misc.ping");
		grp_moderator->add ("command.info.status.rank");
		grp_moderator->add ("command.info.status.logins");
		grp_moderator->add ("command.admin.mute");
		grp_moderator->add ("command.admin.unmute");
		grp_moderator->add ("command.admin.warn");
		grp_moderator->add ("command.info.whodid");
		grp_spectator->add ("command.admin.warnlog.others");
		grp_moderator->msuffix = "§f:";
		grp_moderator->fill_limit = 8000;
		grp_moderator->select_limit = 8000;
		
		group* grp_admin = groups.add (7, "Admin");
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
		grp_admin->add ("command.misc.kill.others");
		grp_admin->add ("command.misc.top");
		grp_admin->add ("command.world.tp.*");
		grp_admin->add ("command.chat.say.*");
		grp_admin->add ("command.world.portal.*");
		grp_admin->add ("command.world.world.get-perms");
		grp_admin->add ("command.world.world.change-members");
		grp_admin->add ("command.world.world.change-owners");
		grp_admin->add ("place.sign.colors");
		
		grp_admin->text_color = 'c';
		grp_admin->msuffix = "§f:";
		grp_admin->color_codes = true;
		grp_admin->fill_limit = 120000;
		grp_admin->select_limit = 120000;
		
		group* grp_executive = groups.add (8, "Executive");
		grp_executive->color = 'e';
		grp_executive->inherit (grp_admin);
		grp_executive->add ("command.world.wcreate");
		grp_executive->add ("command.world.wload");
		grp_executive->add ("command.world.wunload");
		grp_executive->add ("command.world.physics");
		grp_executive->add ("command.world.wsetspawn");
		grp_executive->add ("command.world.world.set-perms");
		grp_executive->add ("command.world.world.resize");
		grp_executive->add ("command.world.world.regenerate");
		grp_executive->add ("command.chat.nick");
		grp_executive->add ("command.admin.unban");
		grp_executive->add ("command.admin.ban.*");
		grp_executive->text_color = 'c';
		grp_executive->msuffix = "§f:";
		grp_executive->color_codes = true;
		grp_executive->fill_limit = 2000000;
		grp_executive->select_limit = 2000000;
		
		group* grp_owner = groups.add (9, "Owner");
		grp_owner->color = '6';
		grp_owner->text_color = 'b';
		grp_owner->add ("*");
		grp_owner->msuffix = "§f:";
		grp_owner->color_codes = true;
		grp_owner->fill_limit = 15000000;
		grp_owner->select_limit = 60000000;
		
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
		
		
		groups.default_rank.set ("@Guest[normal]", groups);
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
				main_world = new world (WT_NORMAL, *this, this->get_config ().main_world, this->log, 
					world_generator::create ("experiment"),
					world_provider::create ("hw", "data/worlds", this->get_config ().main_world));
				main_world->set_size (192, 192);
				main_world->prepare_spawn (15, true);
				main_world->save_all ();
			}
		else
			{
				try
					{
						main_world = world::load_world (*this, this->get_config ().main_world);
					}
				catch (const std::exception& ex)
					{
						throw server_error (("failed to load main world (" + std::string (ex.what ()) + ")").c_str ());
					}
			}
		
		main_world->start ();
		this->register_world (main_world);
		this->main_world = main_world;
		
		// load worlds from the autoload list.
		{
			soci::session sql (this->spool);
			soci::rowset<std::string> rs = (sql.prepare << "SELECT `name` FROM `autoload-worlds`");
			for (auto itr = rs.begin (); itr != rs.end (); ++itr)
				to_load.push_back (*itr);
		}
		for (std::string& wname : to_load)
			{
				world *wr;
				try
					{
						wr = world::load_world (*this, wname.c_str ());
						if (!wr)
							continue;
					}
				catch (const std::exception& ex)
					{
						throw server_error (("failed to load world (" + std::string (ex.what ()) + ")").c_str ());
					}
				
				wr->start ();
				if (!this->register_world (wr))
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
		
		// clear worlds
		{
			logger &log = this->log;
			this->worlds.all (
				[&log] (world *w)
					{
						log (LT_SYSTEM) << "  - Saving world \"" << w->get_name () << "\"" << std::endl;
						w->stop ();
						w->save_all ();
					});
			this->worlds.clear (true);
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
	// init_irc (), destroy_irc ():
	/* 
	 * Initializes the IRC client, and connects it to the IRC network/channel
	 * specified in the configuration file.
	 */
	
	void
	server::init_irc ()
	{
		if (!this->cfg.irc_enabled)
			return;
		
		log (LT_SYSTEM) << "Initializing IRC client" << std::endl;
		
		/* resolve domain name */
		struct hostent *he;
		struct in_addr net_addr;
			{
				struct in_addr **addr_list;
			
				if (!(he = gethostbyname (this->cfg.irc_net.c_str ())))
					{
						log (LT_ERROR) << "Failed to resolve IRC network address" << std::endl;
						return;
					}
			
				addr_list = (struct in_addr **)he->h_addr_list;
				if (!*addr_list)
					{
						log (LT_ERROR) << "Failed to resolve IRC network address" << std::endl;
						return;
					}
			
				net_addr = **addr_list;
			}
		
		log (LT_INFO) << "  - Network: " << this->cfg.irc_net << "/" << this->cfg.irc_port << std::endl;
		log (LT_INFO) << "  - Channel: " << this->cfg.irc_chan << std::endl;
		log (LT_INFO) << "  - Nickname: " << this->cfg.irc_nick << std::endl;
	
		/* create socket */
		int sock = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock < 0)
			{
				log (LT_ERROR) << "Failed to create IRC socket" << std::endl;
				return;
			}
		struct sockaddr_in s_addr;
		std::memset (&s_addr, 0, sizeof s_addr);
		s_addr.sin_family = AF_INET;
		std::memcpy (&s_addr.sin_addr.s_addr, &net_addr, he->h_length);
		s_addr.sin_port = htons (this->cfg.irc_port);
		
		/* connect */
		if (connect (sock, (struct sockaddr *)&s_addr, sizeof s_addr) < 0)
			{
				close (sock);
				log (LT_ERROR) << "Failed to connect to IRC server" << std::endl;
				return;
			}
		
		worker &w = this->get_min_worker ();
		this->ircc = new irc_client (*this, w.evbase, sock);
		this->ircc->connect (this->cfg.irc_nick, this->cfg.irc_chan);
	}
	
	void
	server::destroy_irc ()
	{
		if (!this->cfg.irc_enabled || !this->ircc)
			return;
		
		delete this->ircc;
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
		// we need to stop some things before we dispose of any players, or it
		// could cause some nasty segfaults.
		this->cgen.stop ();
		this->global_physics.stop ();
		
		
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


