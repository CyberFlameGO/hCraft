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
#include <memory>
#include <fstream>
#include <cstring>
#include <libconfig.h++>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <algorithm>


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
			spool (sql_pool_size, "database.sqlite"),
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
		
		std::strcpy (out.ip, "0.0.0.0");
		out.port = 25565;
	}
	
	static void
	write_config (logger& log, libconfig::Config& cfg, const server_config& in)
	{
		libconfig::Setting& grp_server = cfg.getRoot ().add ("server",
			libconfig::Setting::TypeGroup);
		
		/* 'general' group */
		{
			libconfig::Setting& grp_general = grp_server.add ("general",
				libconfig::Setting::TypeGroup);
			
			grp_general.add ("server-name", libconfig::Setting::TypeString)
				= in.srv_name;
			grp_general.add ("server-motd", libconfig::Setting::TypeString)
				= in.srv_motd;
			grp_general.add ("max-players", libconfig::Setting::TypeInt)
				= in.max_players;
			grp_general.add ("main-world", libconfig::Setting::TypeString)
				= in.main_world;
		}
		
		/* 'network' group */
		{
			libconfig::Setting& grp_network = grp_server.add ("network",
				libconfig::Setting::TypeGroup);
			
			grp_network.add ("ip-address", libconfig::Setting::TypeString)
				= in.ip;
			grp_network.add ("port", libconfig::Setting::TypeInt)
				= in.port;
		}
		
		try
			{
				cfg.writeFile ("config.cfg");
			}
		catch (const libconfig::FileIOException& ex)
			{
				log (LT_ERROR) << "Failed to save configuration file to \"config.cfg\"" << std::endl;
			}
	}
	
	
	static void
	_cfg_read_general_grp (logger& log, libconfig::Setting& grp_general, server_config& out)
	{
		std::string str;
		int num;
		bool error = false;
		
		// server name
		if (grp_general.lookupValue ("server-name", str))
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
		str.clear ();
		if (grp_general.lookupValue ("server-motd", str))
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
		
		// max players
		if (grp_general.lookupValue ("max-players", num))
			{
				if (num > 0 && num <= 1024)
					out.max_players = num;
				else
					{
						if (!error)
							log (LT_ERROR) << "Config: at group \"server.general\":" << std::endl;
						log (LT_INFO) << " - \"max_players\" must be in the range of 1-1024." << std::endl;
						error = true;
					}
			}
		
		// main world
		str.clear ();
		if (grp_general.lookupValue ("main-world", str))
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
	_cfg_read_network_grp (logger& log, libconfig::Setting& grp_network, server_config& out)
	{
		std::string str;
		int num;
		bool error = false;
		
		// ip address
		if (grp_network.lookupValue ("ip-address", str))
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
		if (grp_network.lookupValue ("port", num))
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
	_cfg_read_server_grp (logger& log, libconfig::Setting& grp_server, server_config& out)
	{
		try
			{
				libconfig::Setting& grp_general = grp_server["general"];
				_cfg_read_general_grp (log, grp_general, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"server.general\" not found, using defaults" << std::endl;
			}
		
		try
			{
				libconfig::Setting& grp_network = grp_server["network"];
				_cfg_read_network_grp (log, grp_network, out);
			}
		catch (const std::exception& ex)
			{
				log (LT_WARNING) << "Config: Group \"server.network\" not found, using defaults" << std::endl;
			}
	}
	
	static void
	read_config (logger& log, libconfig::Config& cfg, server_config& out)
	{
		libconfig::Setting& root = cfg.getRoot ();
		try
			{
				libconfig::Setting& grp_server = root["server"];
				_cfg_read_server_grp (log, grp_server, out);
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
		
		log () << "Loading configuration from \"config.cfg\"" << std::endl;
		libconfig::Config cfg;
		
		// check if the file exists
		{
			std::ifstream strm ("config.cfg");
			if (strm.is_open ())
				{
					strm.close ();
					cfg.readFile ("config.cfg");
					read_config (this->log, cfg, this->cfg);
					return;
				}
		}
		
		log () << "Configuration file does not exist, creating one with default settings." << std::endl;
		write_config (this->log, cfg, this->cfg);
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
			"CREATE TABLE IF NOT EXISTS `players` (`id` INTEGER PRIMARY KEY "
			"AUTOINCREMENT, `name` TEXT, `groups` TEXT, `nick` TEXT);"
			
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
		this->sched.start ();
		
		this->players = new playerlist ();
		this->id_counter = 0;
		
		this->get_scheduler ().new_task (hCraft::server::cleanup_players, this)
			.run_forever (30000);
		this->tpool.start (6); // 6 pooled threads
	}
	
	void
	server::destroy_core ()
	{
		this->tpool.stop ();
		this->sched.stop ();
		
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
				/*
				// register permissions
				const char **perms = cmd->get_permissions ();
				while (*perms != nullptr)
					perm_man.add (*perms++);*/
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
		_add_command (this->perms, this->commands, "selection");
		_add_command (this->perms, this->commands, "fill");
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
		group* grp_guest = groups.add (1, "guest");
		grp_guest->set_color ('7');
		grp_guest->add ("command.info.help");
		
		group* grp_member = groups.add (2, "member");
		grp_member->set_color ('a');
		grp_member->inherit (grp_guest);
		grp_member->add ("command.chat.me");
		
		group* grp_builder = groups.add (3, "builder");
		grp_builder->set_color ('2');
		grp_builder->inherit (grp_member);
		grp_builder->add ("command.world.world");
		grp_builder->add ("command.world.tp");
		grp_builder->add ("command.draw.selection");
		grp_builder->add ("command.draw.fill");
		
		group* grp_designer = groups.add (4, "designer");
		grp_designer->set_color ('b');
		grp_designer->inherit (grp_builder);
		
		group* grp_architect = groups.add (5, "architect");
		grp_architect->set_color ('3');
		grp_architect->inherit (grp_designer);
		
		group* grp_moderator = groups.add (6, "moderator");
		grp_moderator->set_color ('c');
		grp_moderator->inherit (grp_designer);
		grp_moderator->add ("command.misc.ping");
		
		group* grp_admin = groups.add (7, "admin");
		grp_admin->set_color ('4');
		grp_admin->inherit (grp_architect);
		grp_admin->inherit (grp_moderator);
		grp_builder->add ("command.world.tp.others");
		grp_admin->set_text_color ('c');
		
		group* grp_executive = groups.add (8, "executive");
		grp_executive->set_color ('e');
		grp_executive->inherit (grp_admin);
		grp_executive->add ("command.world.wcreate");
		grp_executive->add ("command.world.wload");
		grp_executive->add ("command.world.wunload");
		grp_executive->add ("command.world.physics");
		grp_executive->add ("command.chat.nick");
		grp_executive->set_text_color ('c');
		
		group* grp_owner = groups.add (9, "owner");
		grp_owner->set_color ('6');
		grp_owner->set_text_color ('7');
		grp_owner->add ("*");
		
		groups.default_rank.set ("@guest", groups);
	}
	
	
	static void
	write_ranks (logger& log, libconfig::Config& cfg, group_manager& groups)
	{
		std::vector<group *> sorted_groups;
		for (auto itr = groups.begin (); itr != groups.end (); ++itr)
			sorted_groups.push_back (itr->second);
		std::sort (sorted_groups.begin (), sorted_groups.end (),
			[] (const group* a, const group* b) -> bool
				{ return (*a) < (*b); });
		
		libconfig::Setting& root = cfg.getRoot ();
		
		{
			std::string def_rank;
			groups.default_rank.get_string (def_rank);
			root.add ("default-rank", libconfig::Setting::TypeString)
				= def_rank;
		}
		
		libconfig::Setting& grp_groups = root.add ("groups",
			libconfig::Setting::TypeGroup);
		
		for (group *grp : sorted_groups)
			{
				libconfig::Setting& grp_set = grp_groups.add (grp->get_name (),
					libconfig::Setting::TypeGroup);
				
				// inheritance
				if (grp->get_parents ().size () > 0)
					{
						libconfig::Setting& arr_inh = grp_set.add ("inheritance",
							libconfig::Setting::TypeArray);
						for (group *parent : grp->get_parents ())
							{
								arr_inh.add (libconfig::Setting::TypeString) = parent->get_name ();
							}
					}
				
				grp_set.add ("power", libconfig::Setting::TypeInt)
					= grp->get_power ();
				
				std::string str;
				str.push_back (grp->get_color ());
				grp_set.add ("color", libconfig::Setting::TypeString)
					= str;
				
				str.clear ();
				str.push_back (grp->get_text_color ());
				grp_set.add ("text-color", libconfig::Setting::TypeString)
					= str;
				
				grp_set.add ("prefix", libconfig::Setting::TypeString)
					= grp->get_prefix ();
				grp_set.add ("suffix", libconfig::Setting::TypeString)
					= grp->get_suffix ();
				grp_set.add ("mprefix", libconfig::Setting::TypeString)
					= grp->get_mprefix ();
				grp_set.add ("msuffix", libconfig::Setting::TypeString)
					= grp->get_msuffix ();
				grp_set.add ("can-chat", libconfig::Setting::TypeBoolean)
					= grp->can_chat ();
				grp_set.add ("can-build", libconfig::Setting::TypeBoolean)
					= grp->can_build ();
				grp_set.add ("can-move", libconfig::Setting::TypeBoolean)
				= grp->can_move ();
				
				libconfig::Setting& arr_perms = grp_set.add ("permissions",
					libconfig::Setting::TypeArray);
				for (permission perm : grp->get_perms ())
					{
						arr_perms.add (libconfig::Setting::TypeString)
							= groups.get_permission_manager ().to_string (perm);
					}
			}
		
		try
			{
				cfg.writeFile ("ranks.cfg");
			}
		catch (const std::exception& ex)
			{
				log (LT_ERROR) << "Failed to save ranks to \"ranks.cfg\"" << std::endl;
			}
	}
	
	
	using group_inheritance_map
		= std::unordered_map<std::string, std::vector<std::string>>;
	
	static void
	_ranks_read_group (logger& log, libconfig::Setting& grp_set,
		const std::string& group_name, group_manager& groups,
		group_inheritance_map& ihmap)
	{
		int grp_power;
		char grp_color;
		char grp_text_color;
		std::string grp_prefix;
		std::string grp_mprefix;
		std::string grp_suffix;
		std::string grp_msuffix;
		bool grp_can_build;
		bool grp_can_move;
		bool grp_can_chat;
		std::vector<std::string> perms;
		
		// inheritance
		try
			{
				libconfig::Setting& arr_inh = grp_set["inheritance"];
				if (arr_inh.getType () == libconfig::Setting::TypeArray)
					{
						for (int i = 0; i < arr_inh.getLength (); ++i)
							{
								std::string parent = arr_inh[i];
								auto itr = ihmap.find (group_name);
								std::vector<std::string>* seq;
								if (itr == ihmap.end ())
									{
										ihmap[group_name] = std::vector<std::string> ();
										seq = &ihmap[group_name];
									}
								else
									seq = &itr->second;
								seq->push_back (std::move (parent));
							}
					}
			}
		catch (const std::exception&) { }
		
		// power
		try
			{ grp_power = grp_set["power"]; }
		catch (const std::exception&)
			{ throw server_error ("in \"ranks.yaml\": group \"power\" field not found."); }
		
		// color
		try
			{ grp_color = ((const char *)grp_set["color"])[0]; }
		catch (const std::exception&)
			{ grp_color = 'f'; }
		
		// text-color
		try
			{ grp_text_color = ((const char *)grp_set["text-color"])[0]; }
		catch (const std::exception&)
			{ grp_text_color = 'f'; }
		
		// prefix
		try
			{
				grp_prefix.assign (grp_set["prefix"].c_str ());
				if (grp_prefix.size () > 32)
					grp_prefix.resize (32);
			}
		catch (const std::exception&) { }
		
		// mprefix
		try
			{
				grp_mprefix.assign (grp_set["mprefix"].c_str ());
				if (grp_mprefix.size () > 32)
					grp_mprefix.resize (32);
			}
		catch (const std::exception&) { }
		
		// suffix
		try
			{
				grp_suffix.assign (grp_set["suffix"].c_str ());
				if (grp_suffix.size () > 32)
					grp_suffix.resize (32);
			}
		catch (const std::exception&) { }
		
		// msuffix
		try
			{
				grp_msuffix.assign (grp_set["msuffix"].c_str ());
				if (grp_msuffix.size () > 32)
					grp_msuffix.resize (32);
			}
		catch (const std::exception&) { }
		
		// can-build
		try
			{ grp_can_build = grp_set["can-build"]; }
		catch (const std::exception&)
			{ grp_can_build = true; }
		
		// can-move
		try
			{ grp_can_move = grp_set["can-move"]; }
		catch (const std::exception&)
			{ grp_can_move = true; }
		
		// can-chat
		try
			{ grp_can_chat = grp_set["can-chat"]; }
		catch (const std::exception&)
			{ grp_can_chat = true; }
		
		// permissions
		try
			{
				libconfig::Setting& arr_perms = grp_set["permissions"];
				if (arr_perms.getType () == libconfig::Setting::TypeArray)
					{
						for (int i = 0; i < arr_perms.getLength (); ++i)
							{
								std::string perm = arr_perms[i];
								perms.push_back (std::move (perm));
							}
					}
			}
		catch (const std::exception&) { }
		
		// create the group and add it to the list.
		group *grp = groups.add (grp_power, group_name.c_str ());
		grp->set_color (grp_color);
		grp->set_text_color (grp_text_color);
		grp->set_prefix (grp_prefix.c_str ());
		grp->set_mprefix (grp_mprefix.c_str ());
		grp->set_suffix (grp_suffix.c_str ());
		grp->set_msuffix (grp_msuffix.c_str ());
		grp->can_build (grp_can_build);
		grp->can_move (grp_can_move);
		grp->can_chat (grp_can_chat);
		for (auto& perm : perms)
			grp->add (perm.c_str ());
	}
	
	static void
	_ranks_read_groups_grp (logger& log, libconfig::Setting& grp_groups,
		group_manager& groups)
	{
		group_inheritance_map ihmap;
		for (int i = 0; i < grp_groups.getLength (); ++i)
			{
				libconfig::Setting& grp_set = grp_groups[i];
				std::string group_name ((grp_set.getName ()));
				_ranks_read_group (log, grp_set, group_name, groups, ihmap);
			}
		
		// resolve inheritance
		for (auto itr = ihmap.begin (); itr != ihmap.end (); ++itr)
			{
				std::string name = itr->first;
				std::vector<std::string>& parents = itr->second;
				
				group *child = groups.find (name.c_str ());
				for (auto& parent : parents)
					{
						group *grp = groups.find (parent.c_str ());
						if (grp)
							{
								child->inherit (grp);
							}
					}
			}
	}
	
	static void
	read_ranks (logger& log, libconfig::Config& cfg, group_manager& groups)
	{
		libconfig::Setting& root = cfg.getRoot ();
		
		std::string def_rank;
		if (!root.lookupValue ("default-rank", def_rank))
			throw server_error ("in \"ranks.cfg\": \"default-rank\" not found or invalid");
		
		libconfig::Setting& grp_groups = root["groups"];
		_ranks_read_groups_grp (log, grp_groups, groups);
		
		groups.default_rank.set (def_rank.c_str (), groups);
	}
	
	
	void
	server::init_ranks ()
	{
		libconfig::Config cfg;
		
		std::ifstream istrm ("ranks.cfg");
		if (istrm)
			{
				istrm.close ();
				log () << "Loading ranks from \"ranks.cfg\"" << std::endl;
				cfg.readFile ("ranks.cfg");
				read_ranks (this->log, cfg, this->groups);
				log (LT_INFO) << " - Loaded " << this->groups.size () << " groups." << std::endl;
				return;
			}
		
		create_default_ranks (this->groups);
		log () << "\"ranks.cfg\" does not exist... Saving defaults." << std::endl;
		write_ranks (this->log, cfg, this->groups);
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
		mkdir ("worlds", 0744);
		std::vector<std::string> to_load;
		
		world *main_world;
		std::string prov_name;
		
		log () << "Loading worlds:" << std::endl;
		
		// load main world
		prov_name = world_provider::determine ("worlds",
			this->get_config ().main_world);
		if (prov_name.empty ())
			{
				// main world does not exist
				log () << " - Main world does not exist, creating..." << std::endl;
				main_world = new world (this->get_config ().main_world, this->log, 
					world_generator::create ("plains"),
					world_provider::create ("hw", "worlds", this->get_config ().main_world));
				main_world->set_size (64, 64);
				main_world->prepare_spawn (10);
			}
		else
			{
				log () << " - Loading \"" << this->get_config ().main_world << "\"" << std::endl;
				world_provider *prov = world_provider::create (prov_name.c_str (),
					"worlds", this->get_config ().main_world);
				if (!prov)
					throw server_error ("failed to load main world (invalid provider)");
				
				const world_information& winf = prov->info ();
				world_generator *gen = world_generator::create (winf.generator.c_str (), winf.seed);
				if (!gen)
					{
						delete prov;
						throw server_error ("failed to load main world (invalid generator)");
					}
				
				main_world = new world (this->get_config ().main_world, this->log, gen, prov);
				main_world->set_size (winf.width, winf.depth);
			}
		main_world->prepare_spawn (10);
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
				std::string prov_name = world_provider::determine ("worlds", wname.c_str ());
				if (prov_name.empty ())
					{
						log (LT_WARNING) << " - World \"" << wname << "\" does not exist." << std::endl;
						continue;
					}
				
				world_provider *prov = world_provider::create (prov_name.c_str (),
					"worlds", wname.c_str ());
				if (!prov)
					{
						log (LT_ERROR) << "Failed to load world \"" << wname
							<< "\": Invalid provider." << std::endl;
						continue;
					}
				
				const world_information& winf = prov->info ();
				world_generator *gen = world_generator::create (winf.generator.c_str (), winf.seed);
				if (!gen)
					{
						delete prov;
						log (LT_ERROR) << "Failed to load world \"" << wname
							<< "\": Invalid generator." << std::endl;
						continue;
					}
				
				log () << " - Loading \"" << wname << std::endl;
				world *wr = new world (wname.c_str (), this->log, gen, prov);
				wr->set_size (winf.width, winf.depth);
				wr->prepare_spawn (10);
				wr->start ();
				if (!this->add_world (wr))
					{
						log (LT_ERROR) << "Failed to load world \"" << wname << "\": Already loaded." << std::endl;
						delete wr;
						continue;
					}
			}
	}
	
	void
	server::destroy_worlds ()
	{
		
		// clear worlds
		{
			std::lock_guard<std::mutex> guard {this->world_lock};
			for (auto itr = this->worlds.begin (); itr != this->worlds.end (); ++itr)
				{
					world *w = itr->second;
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
		if (this->worker_count == 0)
			this->worker_count = 2;
		this->workers.reserve (this->worker_count);
		log () << "Creating " << this->worker_count << " server workers." << std::endl;
		
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

