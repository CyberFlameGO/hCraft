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

#include "player/player.hpp"
#include "system/server.hpp"
#include "util/stringutils.hpp"
#include "util/wordwrap.hpp"
#include "commands/command.hpp"
#include "util/utils.hpp"
#include "entities/pickup.hpp"
#include "util/json.hpp"
#include "util/uuid.hpp"

#include <ctime>
#include <memory>
#include <algorithm>
#include <string>
#include <vector>
#include <cstring>
#include <event2/buffer.h>
#include <sstream>
#include <set>
#include <functional>
#include <cmath>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <random>
#include <sys/stat.h>

#include <cryptopp/integer.h>
#include <cryptopp/osrng.h>

#include <iostream> // DEBUG


namespace hCraft {
	
	/* 
	 * Constructs a new player around the given socket.
	 */
	player::player (server &srv, struct event_base *evbase, evutil_socket_t sock,
		const char *ip)
		: living_entity (srv), log (srv.get_logger ()), sock (sock)
	{
		std::strcpy (this->ip, ip);
		
		this->username[0] = '@';
		std::strcpy (this->username + 1, this->ip);
		std::strcpy (this->nick, this->username);
		std::strcpy (this->colored_username, this->username);
		std::strcpy (this->colored_nick, this->nick);
		
		this->held_slot = 36;
		this->inv_painting = false;
		this->open_win = nullptr;
		
		this->uuid = generate_uuid ();
		this->pstate = PS_HANDSHAKE;
		this->logged_in = false;
		this->fail = false;
		this->kicked = false;
		this->op = false;
		this->authenticated = false;
		this->encrypted = false;
		this->banned = false;
		this->rej_mov = 0;
		
		this->disconnecting = false;
		this->reading = false;
		this->writing = false;
		this->handlers_scheduled = 0;
		this->total_read = 0;
		this->read_rem = 1;
		this->dbid = -1;
		
		this->eating = false;
		this->hearts = 20;
		this->hunger = 20;
		this->hunger_saturation = 20.0;
		this->exhaustion = 0.0;
		this->total_walked = this->total_run = this->total_walked_old = this->total_run_old = 0.0;
		
		this->chat_spam_warnings = 0;
		this->last_tick = std::chrono::steady_clock::now ();
		this->last_heart_regen = this->last_tick;
		this->heal_delay = std::chrono::milliseconds (4000);
		this->tick_counter = 0;
		
		this->curr_gamemode = GT_SURVIVAL;
		this->joining_world = false;
		this->chcurr = chunk_pos (0, 0);
		this->ping_waiting = false;
		this->sb_block.set (BT_STILL_WATER);
		this->need_new_chunks = false;
		this->streaming_chunks = false;
		this->bundo = nullptr;
		
		this->curr_sel = nullptr;
		this->last_ping = std::chrono::system_clock::now ();
		this->keep_alives_received = 0;
		
		this->last_portal_use = std::chrono::steady_clock::now ();
		this->last_hit = std::chrono::steady_clock::now ();
		
		this->evbase = evbase;
		this->bufev  = bufferevent_socket_new (evbase, sock,
			BEV_OPT_CLOSE_ON_FREE);
		if (!this->bufev)
			{ this->fail = true; this->get_server ().schedule_destruction (this); return; }
		
		this->encryptor = nullptr;
		this->decryptor = nullptr;
		
		// set timeouts
		{
			struct timeval read_tv, write_tv;
			
			read_tv.tv_sec = 10;
			read_tv.tv_usec = 0;
			
			write_tv.tv_sec = 10;
			write_tv.tv_usec = 0;
			
			bufferevent_set_timeouts (this->bufev, &read_tv, &write_tv);
		}
		
		bufferevent_setcb (this->bufev, &hCraft::player::handle_read,
			&hCraft::player::handle_write, &hCraft::player::handle_event, this);
		bufferevent_enable (this->bufev, EV_READ | EV_WRITE);
	}
	
	/* 
	 * Class destructor.
	 */
	player::~player ()
	{
		this->disconnect ();
		
		if (this->encryptor)
			delete this->encryptor;
		if (this->decryptor)
			delete this->decryptor;
		
		while (this->is_disconnecting ())
			std::this_thread::sleep_for (std::chrono::milliseconds (1));
		
		{
			std::lock_guard<std::mutex> guard {this->out_lock};
			while (!this->out_queue.empty ())
				{
					packet *top = this->out_queue.front ();
					delete top;
					this->out_queue.pop ();
				}
		}
		
		delete this->bundo;
		
		// delete extra data
		{
			std::lock_guard<std::mutex> guard {this->data_lock};
			for (auto itr = this->extra_data.begin (); itr != this->extra_data.end (); ++itr)
				itr->second.dctor (itr->second.data);
		}
	}
	
	
	
//----
	/* 
	 * libevent callback functions:
	 */
	
	
	struct handle_context
	{
		player *pl;
		unsigned char **packets;
		int count;
	};
	
	/* 
	 * Executed in a thread pool task, spawned by handle ().
	 */
	void
	handle_func (void *ptr)
	{
		handle_context *ctx = static_cast<handle_context *> (ptr);
		player *pl = ctx->pl;
		unsigned char **packets = ctx->packets;
		int count = ctx->count;
		delete ctx; // no longer needed
		
		// this will destroy all pointers in case of failure
		std::vector<std::unique_ptr<unsigned char []> > vec;
		for (int i = 0; i < count; ++i)
			vec.emplace_back (packets [i]);
		delete[] packets;
		
		if (pl->srv.is_shutting_down ())
			return;
		
		for (std::unique_ptr<unsigned char []>& data : vec)
			{
				try
					{
						int err = pl->handle (data.get ());
						-- pl->handlers_scheduled;
						if (pl->is_disconnecting ())
							return;
						else if (err != 0)
							{
								pl->disconnect ();
								return;
							}
					}
				catch (const std::exception& ex)
					{
						pl->log (LT_ERROR) << "Exception: " << ex.what () << std::endl;
						pl->disconnect (false, false);
					}
			}
	}
	
	void
	player::handle_read (struct bufferevent *bufev, void *ctx)
	{
		player *pl = static_cast<player *> (ctx);
		if (pl->bad () || pl->srv.is_shutting_down ()) return;
		pl->reading = true;
		
		struct evbuffer *buf = bufferevent_get_input (bufev);
		size_t buf_size;
		int n;
		int tl = 0;
		
		while ((buf_size = evbuffer_get_length (buf)) > 0)
			{
				n = evbuffer_remove (buf, pl->rdbuf + pl->total_read, pl->read_rem);
				if (n <= 0)
					{ pl->reading = false; pl->disconnect (); return; }
				
				tl = pl->total_read;
				pl->total_read += n;
				
				if (pl->encrypted)
					{
						// decrypt data.
						std::string src ((char *)(pl->rdbuf + tl), pl->total_read - tl);
						std::string tar;
						
						try
							{
								CryptoPP::StringSource (src, true,
									new CryptoPP::StreamTransformationFilter (*pl->decryptor,
										new CryptoPP::StringSink (tar)));
							}
						catch (CryptoPP::Exception& ex)
							{
								pl->log (LT_ERROR) << "Packet decryption failed (Player \"" << pl->get_username () << "\")" << std::endl;
								pl->disconnect ();
								return;
							}
						
						std::memcpy (pl->rdbuf + tl, tar.data (), tar.size ());
						pl->total_read = tar.size () + tl; // might not be necessary...
					}
				
				pl->read_rem = packet::remaining (pl->rdbuf, pl->total_read);
				if (pl->read_rem == -1)
					{
						pl->log (LT_WARNING) << "Received an invalid packet from @"
							<< pl->get_ip () << " (opcode: " << std::hex << std::setfill ('0')
							<< std::setw (2) << (pl->rdbuf[0] & 0xFF) << ")" << std::setfill (' ')
							<< std::endl;
						pl->reading = false; 
						pl->disconnect ();
						return;
					}
				else if (pl->read_rem == 0)
					{
					  // DEBUG
				    /*
				    if (!(pl->rdbuf[1] >= 0x03 && pl->rdbuf[1] <= 0x06))
			        pl->log (LT_DEBUG) << "Packet [" << (int)pl->rdbuf[1] << "]" << std::endl;
				    //*/
					  
						/* finished reading packet */
						unsigned char *data = new unsigned char [pl->total_read];
						std::memcpy (data, pl->rdbuf, pl->total_read);
						pl->exec_queue.push_back (data);
						if (pl->test_packet_chain ())
							{
								// copy queue into an array, so we could use it in a pooled thread.
								int packet_count = pl->exec_queue.size ();
								unsigned char **packets = new unsigned char* [packet_count];
								for (int i = 0; !pl->exec_queue.empty (); ++i)
									{
										packets[i] = pl->exec_queue.front ();
										pl->exec_queue.pop_front ();
									}
								
								// wrap everything up
								handle_context *ctx = new handle_context;
								ctx->pl = pl;
								ctx->packets = packets;
								ctx->count = packet_count;
								
								++ pl->handlers_scheduled;
								pl->get_server ().get_thread_pool ().enqueue (
									handle_func, ctx);
							}
						
						pl->total_read = 0;
						pl->read_rem = 1;
					}
			}
		
		pl->reading = false; 
	}
	
	void
	player::handle_write (struct bufferevent *bufev, void *ctx)
	{
		player *pl = static_cast<player *> (ctx);
		if (pl->bad ()) return;
		pl->writing = true;
		
		int opcode;
		
		std::lock_guard<std::mutex> guard {pl->out_lock};
		if (!pl->out_queue.empty ())
			{
				// dispose of the packet that we just completed sending.
				packet *pack = pl->out_queue.front ();
				pl->out_queue.pop ();
				delete pack;
				
				if (pl->kicked && ((pl->pstate == PS_PLAY && opcode == 0x40)
					|| (pl->pstate == PS_LOGIN && opcode == 0x00)))
					{
						if (pl->kick_msg[0] == '\0')
							pl->log () << pl->get_username () << " has been kicked." << std::endl;
						else
							pl->log () << pl->get_username () << " has been kicked: " << pl->kick_msg << std::endl;	
						
						while (!pl->out_queue.empty ())
							{
								pack = pl->out_queue.front ();
								delete pack;
								pl->out_queue.pop ();
							}
						
						pl->writing = false;
						pl->disconnect (true);
						return;
					}
				
				// if the queue has more packets, send the next one.
				if (!pl->out_queue.empty ())
					{
						packet *pack = pl->out_queue.front ();
						bufferevent_write (bufev, pack->data, pack->size);
					}
			}
		
		pl->writing = false;
	}
	
	void
	player::handle_event (struct bufferevent *bufev, short events, void *ctx)
	{
		player *pl = static_cast<player *> (ctx);
		if (pl->bad ()) return;
		
		pl->disconnect ();
	}
	
	
	
//----
	
	/* 
	 * Marks the player invalid, forcing the server that spawned the player to
	 * eventually destroy it.
	 */
	void
	player::disconnect (bool silent, bool wait_for_callbacks_to_finish)
	{
		if (this->bad ()) return;
		std::lock_guard<std::mutex> guard {this->world_lock};
		this->srv.deregister_entity (this);
		
		this->fail_time = std::chrono::system_clock::now ();
		this->fail = true;
		this->disconnecting = true;
		
		bufferevent_disable (this->bufev, EV_READ | EV_WRITE);
		bufferevent_setcb (this->bufev, nullptr, nullptr, nullptr, nullptr);
		
		this->save_data ();
		
		/*
		// wait for the I/O to stop.
		if (wait_for_callbacks_to_finish)
			while (this->is_reading () || this->is_writing () || this->is_handling_packets ())
				{
					std::this_thread::sleep_for (std::chrono::milliseconds (1));
				}*/
		
		if (!silent)
			log () << this->get_username () << " has disconnected." << std::endl;
		
		this->get_server ().get_players ().remove (this);
		if (this->curr_world)
			{
				if (!this->get_server ().is_shutting_down ())
					{
						this->curr_world->get_players ().remove (this);
						this->remove_from_tab_list (false);
					
						if (this->pstate == PS_PLAY && !silent)
							{
								// leave message
								this->get_server ().get_players ().message (
									messages::compile (this->get_server ().msgs.server_leave, messages::environment (this)));
								if (this->get_server ().get_irc ())
									this->get_server ().get_irc ()->chan_msg (std::string ("- ") + this->get_colored_username () + " §0has left the server");
							}
				
						chunk *curr_chunk = this->curr_world->get_chunk (
							this->chcurr.x, this->chcurr.z);
						if (curr_chunk)
							curr_chunk->remove_entity (this);
						this->known_chunks.clear ();
				
						// despawn from other players.
						std::lock_guard<std::mutex> guard {this->visible_player_lock};
						for (player *pl : this->visible_players)
							{
								this->despawn_from (pl);
							}
					}
			}
		
		{	
			std::lock_guard<std::mutex> guard ((this->get_server ().get_player_lock ()));
			bufferevent_free (this->bufev);
		}
		
		{	
			std::lock_guard<std::mutex> guard {this->data_lock};
			delete this->open_win;
		}
		
		if (this->bundo)
			this->bundo->close ();
		
		// dispose of selection
		for (auto itr =  std::begin (this->selections);
		 				 	itr != std::end (this->selections);
		 				 	++itr)
		 	delete itr->second;
		this->selections.clear ();
		this->curr_sel = nullptr;
		
		// and finally
		this->get_server ().schedule_destruction (this);
		this->disconnecting = false;
	}
	
	
	
	/* 
	 * Kicks the player with the given message.
	 * @{slp}: Server list ping
	 */
	void
	player::kick (const char *msg, const char *log_msg, bool sanitize)
	{
		if (log_msg)
			std::strcpy (this->kick_msg, log_msg);
		else
			std::strcpy (this->kick_msg, msg);
		
		json::object js;
		js.insert_string ("text", msg);
		std::ostringstream ss;
		js.write (ss);
		
		this->kicked = true;
		if (sanitize)
			{
				if (this->pstate == PS_PLAY)
					this->send (packets::play::make_disconnect (ss.str ().c_str ()));
				else if (this->pstate == PS_LOGIN)
					this->send (packets::login::make_disconnect (ss.str ().c_str ()));
			}
	}
	
	
	
	// returns true if the packet can be disposed of, without being sent.
	static bool
	_redundancy_test (player *pl, packet *pack)
	{
		packet_reader reader {pack->data};
		reader.read_varint (); // skip length
		
		switch (reader.read_varint ())
			{
				// multi block change
				case 0x22:
					{
						int cx = reader.read_int ();
						int cz = reader.read_int ();
						if (!pl->can_see_chunk (cx, cz))
							return true;
					}
					break;
				
				// block change
				case 0x23:
					{
						int x = reader.read_int ();
						reader.read_byte (); // y
						int z = reader.read_int ();
						
						if (!pl->can_see_chunk (x >> 4, z >> 4))
							return true;
					}
					break;
			}
		
		return false;
	}
	
	/* 
	 * Inserts the specified packet into the player's queue of outgoing packets.
	 */
	void
	player::send (packet *pack)
	{
		if (this->bad () || _redundancy_test (this, pack))
			{ delete pack; return; }
		
		std::lock_guard<std::mutex> guard {this->out_lock};
		
		// encrypt contents
		if (this->encrypted)
			{
				std::string src ((const char *)pack->data, (int)pack->size);
				std::string tar;
				
				try
					{
						CryptoPP::StringSource (src, true,
							new CryptoPP::StreamTransformationFilter (*this->encryptor,
								new CryptoPP::StringSink (tar)));
					}
				catch (CryptoPP::Exception& ex)
					{
						log (LT_ERROR) << "Packet encryption failed (Player \"" << this->get_username () << "\")" << std::endl;
						this->disconnect ();
						return;
					}
		
				if (pack->cap < tar.size ())
					pack->resize (tar.size ());
				pack->clear ();
				pack->put_bytes ((const unsigned char *)tar.data (), tar.size ());
			}
		
		this->out_queue.push (pack);
		if (this->out_queue.size () == 1)
			{
				// initiate write
				bufferevent_write (this->bufev, pack->data, pack->size);
			}
	}
	
	
	
	/* 
	 * Sends the player to the given world.
	 */
	void
	player::join_world (world* w, bool broadcast, bool first_join)
	{
		this->join_world_at (w, w->get_spawn (), broadcast, first_join);
	}
	
	
	static void
	_broadcast_join_message (player *pl, world *wr, world *prev_world)
	{
		messages::environment env {pl};
		env.prev_world = prev_world;
		env.curr_world = wr;
	
		std::vector<player *> new_players;
		std::vector<player *> old_players;
		std::vector<player *> others;
	
		// fill the vectors
		pl->get_server ().get_players ().populate (others);
		prev_world->get_players ().populate (old_players);
		wr->get_players ().populate (new_players);
		others.erase (std::remove_if (others.begin (), others.end (),
			[&old_players, &new_players] (const player *pl) -> bool
				{
					return (std::find (old_players.begin (), old_players.end (), pl) != old_players.end ())
							|| (std::find (new_players.begin (), new_players.end (), pl) != new_players.end ());
				}), others.end ());
		new_players.erase (std::remove (new_players.begin (), new_players.end (), pl), new_players.end ());
	
		std::string msg;
	
		// self
		if (!pl->get_server ().msgs.world_join_self.empty ())
			{
				msg = messages::compile (pl->get_server ().msgs.world_join_self, env);
				pl->message (msg);
			}
	
		// new players
		if (!pl->get_server ().msgs.world_enter.empty ())
			{
				msg = messages::compile (pl->get_server ().msgs.world_enter, env);
				for (player *p : new_players)
					p->message (msg);
			}
	
		// old players
		if (!pl->get_server ().msgs.world_depart.empty ())
			{
				msg = messages::compile (pl->get_server ().msgs.world_depart, env);
				for (player *p : old_players)
					p->message (msg);
			}
	
		// others
		if (!pl->get_server ().msgs.world_join.empty ())
			{
				msg = messages::compile (pl->get_server ().msgs.world_join, env);
				for (player *p : others)
					p->message (msg);
			}
	}
	
	static void
	_inv_from_str (inventory& inv, const std::string& inv_str)
	{
		const char *ptr = inv_str.c_str ();
		const char *scol;
		std::string str;
		do
			{
				str.clear ();
				
				scol = std::strchr (ptr, ';');
				if (scol)
					{
						while (ptr != scol)
							str.push_back (*ptr++);
						++ ptr;
					}
				else
					{
						while (*ptr)
							str.push_back (*ptr++);
					}
				
				// id:meta{amount}
				std::string sid, smeta, samount;
				bool got_col = false, got_lbrace = false, good = true;
				const char *ptr2 = str.c_str ();
				while (*ptr2)
					{
						char c = *ptr2++;
						if (std::isdigit (c))
							{
								if (got_lbrace)
									samount.push_back (c);
								else if (got_col)
									smeta.push_back (c);
								else
									sid.push_back (c);
							}
						else if (c == '{')
							{
								if (got_lbrace)
									{ good = false; break; }
								got_lbrace = true;
							}
						else if (c == '}')
							break;
						else if (c == ':')
							{
								if (got_col)
									{ good = false; break; }
								got_col = true;
							}
					}
				
				if (good && !sid.empty ())
					{
						unsigned short id;
						{
							std::istringstream ss {sid};
							ss >> id;
						}
						
						short meta = 0;
						if (!smeta.empty ())
							{
								std::istringstream ss {smeta};
								ss >> meta;
							}
						
						unsigned short amount = 1;
						if (!samount.empty ())
							{
								std::istringstream ss {samount};
								ss >> amount;
							}
						
						inv.add (slot_item (id, meta, amount));
					}
			}
		while (scol);
	}
	
	/* 
	 * Sends the player to the given world at the specified location.
	 */
	void
	player::join_world_at (world *w, entity_pos destpos, bool broadcast, bool first_join)
	{
		if (this->bad ())
			return;
			
		world *prev_world = nullptr;
		
		std::lock_guard<std::mutex> guard {this->world_lock};
		bool had_prev_world = ((this->curr_world != w) && this->curr_world);
		
		// this is set to false once the player's home chunk is successfully sent,
		// at stream_chunks ().
		this->joining_world = true;
		
		// this will keep the player safe from fall damage right after spawning
		this->fall_flag = true;
		
		// selection blocks
		this->sb_updates.clear ();
		this->sb_updates.set_world (w, true);
		if (had_prev_world)
			{
				prev_world = this->curr_world;
				
				this->clear_tab_list ();
				this->remove_from_tab_list (false);
				this->curr_world->get_players ().remove (this);
				
				chunk *prev_chunk = this->curr_world->get_chunk (this->chcurr.x, this->chcurr.z);
				if (prev_chunk)
					prev_chunk->remove_entity (this);
				
				// destroy selections
				for (auto itr = this->selections.begin (); itr != this->selections.end (); ++itr)
					{
						world_selection *sel = itr->second;
						delete sel;
					}
				this->selections.clear ();
				
				// stop player from moving
				entity_pos epos = destpos;
				this->send (packets::play::make_player_pos_and_look (
					epos.x, epos.y, epos.z, epos.r, epos.l, true));
			}
		
		// undo data
		mkdir ((std::string ("data/undo/") + w->get_name ()).c_str (), 0744);
		if (this->bundo)
			{
				this->bundo->flush ();
				this->bundo->set_path (std::string ("data/undo/") + w->get_name () + "/" + this->get_username () + ".undo");
			}
		else
			this->bundo = new block_undo (std::string ("data/undo/") + w->get_name () + "/" + this->get_username () + ".undo");
		
		this->set_world (w);
		this->pos = destpos;
		this->curr_world->get_players ().add (this);
		this->load_tab_list ();
		this->add_to_tab_list (false);
		this->last_ground_height = -128.0;
		
		block_pos bpos = destpos;
		this->send (packets::play::make_spawn_position (bpos.x, bpos.y, bpos.z));
		
		if (!first_join)
			{
				this->change_gamemode ((gamemode_type)w->def_gm);
			}
		
		this->send (packets::play::make_time_update (w->get_time (), w->get_time ()));
		this->need_new_chunks = true;
		
		// start physics
		this->srv.global_physics.queue_physics (w, this->eid);
		
		log () << this->get_username () << " joined world \"" << w->get_name () << "\"" << std::endl;
		if (had_prev_world)
			_broadcast_join_message (this, w, prev_world);
		
		// load inventory
		if (w->use_def_inv)
			{
				this->inv.clear ();
				_inv_from_str (this->inv, w->def_inv);
			}
		else
			{
				// TODO
			}
	}
	
	/* 
	 * Reloads the map for the player.
	 */
	void
	player::rejoin_world (bool respawn)
	{
		std::lock_guard<std::mutex> wguard {this->world_lock};
		for (auto itr = this->known_chunks.begin (); itr != this->known_chunks.end (); ++ itr)
			{
				known_chunk kc = *itr;
				
				this->send (packets::play::make_empty_chunk (kc.cx, kc.cz));
				
				// despawn entities from self and vice-versa.
				chunk *ch = (kc.w)->get_chunk (kc.cx, kc.cz);
				if (ch)
					{
						player *me = this;
						ch->all_entities (
							[me] (entity *e)
								{
									e->despawn_from (me);
									if (e->get_type () == ET_PLAYER)
										{
											player *pl = dynamic_cast<player *> (e);
											if (pl == me) return;
											me->despawn_from (pl);
										}
								});
					}
			}
		this->known_chunks.clear ();
		
		if (respawn)
			{
				entity_pos epos = this->pos = this->get_world ()->get_spawn ();
				this->send (packets::play::make_player_pos_and_look (
					epos.x, epos.y, epos.z, epos.r, epos.l, true));
			}
		
		this->need_new_chunks = true;
		this->joining_world = true;
		this->fall_flag = true;
	}
	
	
	
//--
	
	/* 
	 * Loads new close chunks to the player and unloads those that are too
	 * far away.
	 */
	void
	player::stream_chunks (int radius)
	{
		if (this->streaming_chunks)
			return;
		
		std::lock_guard<std::mutex> wguard {this->world_lock};
		this->streaming_chunks = true;
		
		std::vector<std::pair<known_chunk, bool>> unload_list;
		world *w = this->curr_world;
		
		if (this->need_new_chunks)
			{
				// compile a list of chunks that we no longer need
				for (auto itr = this->known_chunks.begin (); itr != this->known_chunks.end (); )
					{
						known_chunk kc = *itr;
						
						if (!this->can_see_chunk (kc.cx, kc.cz))
							{
								unload_list.push_back (std::make_pair (kc, true));
								itr = this->known_chunks.erase (itr);
							}
						else
							{
								if (kc.w != w)
									unload_list.push_back (std::make_pair (kc, false));
								++ itr;
							}
					}
				
				// get a sorted list of chunk coordinates.
				std::vector<chunk_pos> coords;
				{
					player *pl = this;
					chunk_pos cp = this->pos;
					
					for (int cx = (cp.x - radius); cx <= (cp.x + radius); ++cx)
						for (int cz = (cp.z - radius); cz <= (cp.z + radius); ++cz)
							coords.emplace_back (cx, cz);
					std::sort (coords.begin (), coords.end (),
						[pl] (const chunk_pos a, const chunk_pos b) -> bool
							{
								double a_dx = (a.x * 16.0) - pl->pos.x;
								double a_dz = (a.z * 16.0) - pl->pos.z;
								double b_dx = (b.x * 16.0) - pl->pos.x;
								double b_dz = (b.z * 16.0) - pl->pos.z;
								
								return (a_dx*a_dx + a_dz*a_dz) < (b_dx*b_dx + b_dz*b_dz);
							});
				}
				
				for (chunk_pos cpos : coords)
					{
						int cx = cpos.x, cz = cpos.z;
						
						// do we already have this chunk in our known chunk list?
						bool found = false;
						for (auto itr = this->known_chunks.begin (); itr != this->known_chunks.end (); ++itr)
							{
								known_chunk kc = *itr;
								if (kc.cx == cx && kc.cz == cz)
									{
										if (kc.w != w)
											this->known_chunks.erase (itr);
										else
											found = true;
										break;
									}
							}
				
						if (!found)
							{
								bool p_found = false;
								for (auto itr = this->pending_chunks.begin (); itr != this->pending_chunks.end (); ++itr)
									{
										known_chunk c = *itr;
										if (c.cx == cx && c.cz == cz)
											{
												if (c.w == w) 
													p_found = true;
												else
													this->pending_chunks.erase (itr);
												break;
											}
									}
								
								if (!p_found)
									{
										// fetch all chunks around it first!
										// to ensure that the world generator doesn't produce any
										// glitched structures (such as trees cut in half)
										for (int xx = (cx - 1); xx <= (cx + 1); ++xx)
											for (int zz = (cz - 1); zz <= (cz + 1); ++zz)
												if (!(xx == cx && zz == cz))
													this->srv.cgen.request (w, xx, zz, this->eid, GFL_NODELIVER | GFL_NOABORT);
										
										this->srv.cgen.request (w, cx, cz, this->eid);
										this->pending_chunks.push_back ({w, cx, cz});
									}
							}
					}
				
				this->need_new_chunks = false;
			}
		
		chunk_pos my_cpos = this->pos;
		
		// check if we got any chunks from the generator
		if (!this->response_chunks.empty ())
			{
				std::lock_guard<std::mutex> guard {this->response_chunks_lock};
				while (!this->response_chunks.empty ())
					{
						gen_response resp = this->response_chunks.front ();
						this->response_chunks.pop ();
						
						// remove from pending chunk list
						for (auto itr = this->pending_chunks.begin (); itr != this->pending_chunks.end (); ++itr)
							if (itr->cx == resp.cx && itr->cz == resp.cz)
								{ this->pending_chunks.erase (itr); break; }
						
						if (!resp.ch || resp.flags == GFL_ABORTED)
							continue;
						
						if (resp.w != w)
							continue; // wrong world
					
						// do we still need this chunk?
						if (!this->can_see_chunk (resp.cx, resp.cz))
							continue;
						
						// selection blocks / editstages
						std::vector<edit_stage *> es_vec;
						es_vec.push_back (&this->sb_updates);
						for (edit_stage *es : this->edstages)
							if (es->get_world () == w)
								es_vec.push_back (es);
							
						this->send (packets::play::make_chunk (resp.cx, resp.cz, resp.ch, es_vec));
						
						this->known_chunks.push_back ({w, resp.cx, resp.cz});
						
						// is this our new home chunk? (When switching between worlds)
						if (this->joining_world && (my_cpos.x == resp.cx && my_cpos.z == resp.cz))
							{
								entity_pos epos = this->pos;
								this->send (packets::play::make_player_pos_and_look (
									epos.x, epos.y, epos.z, epos.r, epos.l, true));
								this->rej_mov = 3; // reject the next 3 movement packets
								
								this->update_home_chunk ();
								
								this->joining_world = false;
							}
						
						// spawn entities to self and vice-versa
						player *me = this;
						(resp.ch)->all_entities (
							[me] (entity *e)
								{
									e->spawn_to (me);
									if (e->get_type () == ET_PLAYER)
										{
											player* pl = dynamic_cast<player *> (e);
											if (pl == me) return;
											me->spawn_to (pl);
										}
								});
						
						// send signs
						{
							std::lock_guard<std::mutex> guard {(resp.ch)->ly_signs.lock};
							for (auto itr = (resp.ch)->ly_signs.signs.begin ();
								itr != (resp.ch)->ly_signs.signs.end (); )
								{
									block_pos pos = itr->first;
									
									int id = (resp.ch)->get_id (pos.x & 0xF, pos.y, pos.z & 0xF);
									if (id != BT_SIGN_POST && id != BT_WALL_SIGN)
										{
											// sign got deleted
											itr = (resp.ch)->ly_signs.signs.erase (itr);
										}
									else
										{
											auto& sign = itr->second;
								
											this->send (packets::play::make_update_sign (pos.x, pos.y, pos.z,
												sign.l1.c_str (), sign.l2.c_str (), sign.l3.c_str (),
												sign.l4.c_str ()));
											++ itr;
										}
								}
						}
					}
			}
		
		// unload chunks
		for (auto p : unload_list)
			{
				known_chunk kc = p.first;
				bool full_unload = p.second;
				
				if (full_unload)
					this->send (packets::play::make_empty_chunk (kc.cx, kc.cz));
				
				// despawn entities from self and vice-versa.
				chunk *ch = (kc.w)->get_chunk (kc.cx, kc.cz);
				if (ch)
					{
						player *me = this;
						ch->all_entities (
							[me] (entity *e)
								{
									e->despawn_from (me);
									if (e->get_type () == ET_PLAYER)
										{
											player *pl = dynamic_cast<player *> (e);
											if (pl == me) return;
											me->despawn_from (pl);
										}
								});
					}
			}
			
		this->streaming_chunks = false;
	}
	
	/* 
	 * Checks whether the specified chunk is within the visible chunk range
	 * of the player.
	 */
	bool
	player::can_see_chunk (int x, int z)
	{
		chunk_pos me_pos = this->pos;
		return (
			(utils::iabs (me_pos.x - x) <= player::chunk_radius ()) &&
			(utils::iabs (me_pos.z - z) <= player::chunk_radius ()));
	}
	
	/* 
	 * Used by the chunk_generator class to inform the player that a chunk
	 * has been generated.
	 */
	void
	player::deliver_chunk (world *w, int x, int z, chunk *ch, int flags, int extra)
	{
		std::lock_guard<std::mutex> guard {this->response_chunks_lock};
		this->response_chunks.push ({w, x, z, ch, flags, extra});
	}
	
	
	
	/* 
	 * Resends the block located at the given block coordinates.
	 */
	void
	player::send_orig_block (int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		block_data bd = this->curr_world->get_block (x, y, z);
		this->send (packets::play::make_block_change (x, y, z, bd.id, bd.meta));
		if (bd.id == BT_SIGN_POST || bd.id == BT_WALL_SIGN)
			{
				// resend sign contents
				chunk *ch = this->get_world ()->get_chunk_at (x, z);
				if (ch)
					{
						auto& sign = ch->ly_signs.get_sign (x, y, z);
						this->send (packets::play::make_update_sign (x, y, z,
							sign.l1.c_str (), sign.l2.c_str (), sign.l3.c_str (), sign.l4.c_str ()));
					}
			}
	}
	
//--
	
	
	
	void
	player::update_home_chunk ()
	{
		chunk_pos curr_cpos = this->pos;
		
		chunk *prev_chunk = this->curr_world->get_chunk (this->chcurr.x, this->chcurr.z);
		if (prev_chunk)
			prev_chunk->remove_entity (this);

		chunk *new_chunk = this->curr_world->get_chunk (curr_cpos.x, curr_cpos.z);
		if (new_chunk)
			{
				new_chunk->add_entity (this);
				this->chcurr.set (curr_cpos.x, curr_cpos.z);
			}
	}
	
	
	void
	player::handle_portals ()
	{
		if (this->is_dead ())
			return;
		
		world *w = this->get_world ();
		block_pos pos = this->pos;
		if (pos.y < 0 || pos.y >= 254)
			return;
		
		for (int y = pos.y + 1; y >= pos.y; --y)
			{
				if (w->get_extra (pos.x, y, pos.z) == BE_PORTAL)
					{
						int elapsed = std::chrono::duration_cast<std::chrono::seconds> (
							std::chrono::steady_clock::now () - this->last_portal_use).count ();
						if (elapsed < 1)
							break;
						
						this->last_portal_use = std::chrono::steady_clock::now ();
						
						portal *ptl = w->get_portal (pos.x, y, pos.z);
						if (ptl)
							{
								world *w = this->get_server ().get_worlds ().find (ptl->dest_world.c_str ());
								if (!w)
									{
										this->message ("§4Error§7: §cDestination world not loaded");
										continue;
									}
								else if (!w->security ().can_join (this))
									{
										this->message ("§cYou are not allowed to go through this portal.");
										continue;
									}
								
								if (w == this->get_world ())
									this->teleport_to (ptl->dest_pos);
								else
									this->join_world_at (w, ptl->dest_pos);
								
								break;
							}
						else
							{
								this->message ("§4Error§7: §cDestination not found");
							}
					}
			}
	}
	
	
	
	bool
	player::handle_zones (entity_pos dest)
	{
		std::lock_guard<std::mutex> guard {this->world_lock};
		if (this->is_dead ())
			return false;
		
		block_pos bp = dest;
		std::vector<zone *> zones;
		this->get_world ()->get_zones ().find (bp.x, bp.y, bp.z, zones);
		
		// build the list of zones that the player is entering.
		std::vector<zone *> entering;
		for (zone *zn : zones)
			if (this->curr_zones.find (zn) == this->curr_zones.end ())
				entering.push_back (zn);
		
		// build the list of zones that the player is leaving.
		std::vector<zone *> leaving;
		for (zone *zn : this->curr_zones)
			if (std::find (zones.begin (), zones.end (), zn) == zones.end ())
				leaving.push_back (zn);
		
		for (zone *zn : entering)
			if (!zn->get_security ().can_enter (this))
				{
					dest = block_pos (this->last_good_pos);
					dest.x += 0.5;
					dest.z += 0.5;
					dest.r = this->last_good_pos.r;
					dest.l = this->last_good_pos.l;
				
					this->send (packets::play::make_player_pos_and_look (
						dest.x, dest.y, dest.z, dest.r, dest.l, dest.on_ground));
					this->pos = dest;
					this->message ("§4 * §cYou are not allowed to enter this zone§4.");
					return true;
				}
		
		
		for (zone *zn : leaving)
			if (!zn->get_security ().can_leave (this))
				{
					dest = block_pos (this->last_good_pos);
					dest.x += 0.5;
					dest.z += 0.5;
					dest.r = this->last_good_pos.r;
					dest.l = this->last_good_pos.l;
				
					this->send (packets::play::make_player_pos_and_look (
						dest.x, dest.y, dest.z, dest.r, dest.l, dest.on_ground));
					this->pos = dest;
					this->message ("§4 * §cYou are not allowed to leave this zone§4.");
					return true;
				}
		
		// rebuild the list of zones that the player is currently in.
		this->curr_zones.clear ();
		for (zone *zn : zones)
			this->curr_zones.insert (zn);
		
		// leave/enter messages.
		for (zone *zn : entering)
			if (!zn->get_enter_msg ().empty ())
				this->message (zn->get_enter_msg ());
		for (zone *zn : leaving)
			if (!zn->get_leave_msg ().empty ())
				this->message (zn->get_leave_msg ());
		
		this->last_good_pos = pos;
		return false;
	}
	
	
	/* 
	 * Moves the player to the specified position.
	 */
	void
	player::move_to (entity_pos dest)
	{
		int w_width = this->get_world ()->get_width ();
		int w_depth = this->get_world ()->get_depth ();
		if ((w_width > 0) || (w_depth > 0))
			{
				bool changed = false;
				if (w_width > 0) {
					if (dest.x < 0.0) { dest.x = 1.0; changed = true; }
					else if (dest.x >= w_width) { dest.x = w_width - 1; changed = true; }
				}
				if (w_depth > 0) {
					if (dest.z < 0.0) { dest.z = 1.0; changed = true; }
					else if (dest.z >= w_depth) { dest.z = w_depth - 1; changed = true; }
				}
				
				if (changed && !this->joining_world)
					{
						this->send (packets::play::make_player_pos_and_look (
							dest.x, dest.y, dest.z, dest.r, dest.l, dest.on_ground));
					}
			}
		
		// zones
		if (this->handle_zones (dest))
			return;
		
		entity_pos prev_pos = this->pos;
		this->pos = dest;
		
		chunk_pos curr_cpos = dest;
		chunk_pos prev_cpos = prev_pos;
		if ((curr_cpos.x != prev_cpos.x) || (curr_cpos.z != prev_cpos.z))
			{
				this->need_new_chunks = true;
			}
		
		if (prev_pos == dest)
			return;
		
	//----
		// set home chunk
		if (!(curr_cpos.x == this->chcurr.x && curr_cpos.z == this->chcurr.z))
			{
				this->update_home_chunk ();
			}
	
	//----
		
		double x_delta = dest.x - prev_pos.x;
		double y_delta = dest.y - prev_pos.y;
		double z_delta = dest.z - prev_pos.z;
		float r_delta = dest.r - prev_pos.r;
		float l_delta = dest.l - prev_pos.l;
		
	//----
		/* 
		 * Handle exhaustion from running\walking.
		 */
		double dist = std::sqrt (x_delta*x_delta + y_delta*y_delta + z_delta*z_delta);
		if (this->is_sprinting ())
			this->total_run += dist;
		else
			this->total_walked += dist;
		if ((this->total_walked - this->total_walked_old) >= 1.0)
			{
				this->increment_exhaustion (0.01);
				this->total_walked_old = this->total_walked;
			}
		else if ((this->total_run - this->total_run_old) >= 1.0)
			{
				this->increment_exhaustion (0.1);
				this->total_run_old = this->total_run;
			}
	//----
		
		
		if (x_delta == 0.0 && y_delta == 0.0 && z_delta == 0.0)
			{
				// position hasn't changed.
				
				if (r_delta == 0.0f && l_delta == 0.0f)
					{
						// the player hasn't moved at all.
						
					}
				else
					{
						// only orientation changed.
						std::lock_guard<std::mutex> guard {this->visible_player_lock};
						for (player *pl : this->visible_players)
							{
								pl->send (packets::play::make_entity_look (this->get_eid (), dest.r, dest.l));
								pl->send (packets::play::make_entity_head_look (this->get_eid (), dest.r));
							}
					}
			}
		else
			{
				// position has changed.
				
				std::lock_guard<std::mutex> guard {this->visible_player_lock};
				for (player *pl : this->visible_players)
					{
						pl->send (packets::play::make_entity_move (this->get_eid (),
							dest.x, dest.y, dest.z, dest.r, dest.l));
						pl->send (packets::play::make_entity_head_look (this->get_eid (), dest.r));
					}
			}
		
		this->handle_falls_and_jumps (prev_pos.on_ground, this->pos.on_ground, prev_pos);
		this->handle_portals ();
		
		this->old_pos = this->pos;
	}
	
	/* 
	 * Teleports the player to the given position.
	 */
	void
	player::teleport_to (entity_pos dest)
	{
		this->send (packets::play::make_player_pos_and_look (
			dest.x, dest.y, dest.z, dest.r, dest.l, dest.on_ground));
		this->move_to (dest);
		this->send (packets::play::make_player_pos_and_look (
			dest.x, dest.y, dest.z, dest.r, dest.l, dest.on_ground));
	}
	
	
	
//----
	
	/* 
	 * Sends a ping packet to the player and waits for a response.
	 */
	void
	player::ping ()
	{
		this->ping_waiting = true;
		this->last_ping = std::chrono::system_clock::now ();
		this->ping_id = std::chrono::system_clock::to_time_t (this->last_ping) & 0xFFFF;
		this->send (packets::play::make_keep_alive (this->ping_id));
	}
	
	/* 
	 * Sends a ping packet to the player only if the specified amount of
	 * milliseconds have passed since the last ping packet has been sent.
	 * 
	 * If the player is still waiting for a ping response, the function
	 * will kick the player.
	 */
	void
	player::try_ping (int ms)
	{
		if ((this->last_ping + std::chrono::milliseconds (ms))
			<= std::chrono::system_clock::now ())
			{
				if (this->ping_waiting)
					{
						this->kick ("§cPing timeout");
						return;
					}
				
				this->ping ();
			}
	}
	
	
	
//----
	
	/* 
	 * Spawns self to the specified player.
	 */
	void
	player::spawn_to (player *pl)
	{
		if (pl == this || (this->bad () || pl->bad ()) || !this->curr_world)
			return;
		
		std::string col_name;
		col_name.append (this->get_colored_username ());
		
		entity_pos me_pos = this->pos;
		entity_metadata me_meta;
		this->build_metadata (me_meta);
		pl->send (packets::play::make_spawn_player (
			this->get_eid (), this->get_uuid ().to_str ().c_str (), col_name.c_str (),
			me_pos.x, me_pos.y, me_pos.z, me_pos.r, me_pos.l, 0, me_meta));
		pl->send (packets::play::make_entity_head_look (this->get_eid (), me_pos.r));
		
		pl->send (packets::play::make_entity_equipment (this->eid, 0, this->inv.get (this->held_slot)));
		
		{
			std::lock_guard<std::mutex> guard {pl->visible_player_lock};
			pl->visible_players.insert (this);
		}
		{
			std::lock_guard<std::mutex> guard {this->visible_player_lock};
			this->visible_players.insert (pl);
		}
	}
	
	void
	player::spawn_to_all ()
	{
		if (this->bad () || !this->curr_world)
			return;
		
		player *me = this;
		chunk_pos me_pos = me->pos;
		
		this->get_world ()->get_players ().all (
			[me, me_pos] (player *pl)
				{
					if ((pl != me) && pl->can_see_chunk (me_pos.x, me_pos.z))
						{
							me->spawn_to (pl);
						}
				});
	}
	
	/* 
	 * Despawns self from the specified player.
	 */
	bool
	player::despawn_from (player *pl)
	{
		if (pl->bad () || !this->curr_world)
			return false;
		
		if (!entity::despawn_from (pl))
			return false;
		
		std::lock_guard<std::mutex> guard {pl->visible_player_lock};
		pl->visible_players.erase (this);
		return true;
	}
	
	void
	player::despawn_from_all ()
	{
		if (this->bad () || !this->curr_world)
			return;
		
		player *me = this;
		chunk_pos me_pos = me->pos;
		
		this->get_world ()->get_players ().all (
			[me, me_pos] (player *pl)
				{
					if ((pl != me) && pl->can_see_chunk (me_pos.x, me_pos.z))
						{
							me->despawn_from (pl);
						}
				});
	}
	
	
	
	void
	player::add_to_tab_list (bool self)
	{
		if (this->bad () || !this->curr_world)
			return;
		
		player *me = this;
		this->get_world ()->get_players ().all (
			[self, me] (player *pl)
				{
					if (self || (me != pl))
						pl->send (packets::play::make_player_list_item (me->get_colored_username (), true, me->ping_time_ms));
				});
	}
	
	void
	player::remove_from_tab_list (bool self)
	{
		if (!this->curr_world)
			return;
		
		player *me = this;
		this->get_world ()->get_players ().all (
			[self, me] (player *pl)
				{
					if (self || (me != pl))
						pl->send (packets::play::make_player_list_item (me->get_colored_username (), false, me->ping_time_ms));
				});
	}
	
	void
	player::clear_tab_list ()
	{
		if (this->bad () || !this->curr_world)
			return;
		
		player *me = this;
		this->get_world ()->get_players ().all (
			[me] (player *pl)
				{
					me->send (packets::play::make_player_list_item (pl->get_colored_username (), false, me->ping_time_ms));
				});
	}
	
	void
	player::load_tab_list ()
	{
		if (this->bad () || !this->curr_world)
			return;
		
		player *me = this;
		this->get_world ()->get_players ().all (
			[me] (player *pl)
				{
					me->send (packets::play::make_player_list_item (pl->get_colored_username (), true, me->ping_time_ms));
				});
	}
	
	
	
//----
	
	/* 
	 * Sends the given message to the player.
	 */
	
	void
	player::message (const char *msg)
	{
		json::object js;
		js.insert_string ("text", msg);
		
		std::ostringstream ss;
		js.write (ss);
		
		this->send (packets::play::make_chat_message (ss.str ().c_str ()));
	}
	
	void
	player::message (const std::string& msg)
	{
		this->message (msg.c_str ());
	}
	
	void
	player::message_wrapped (const char *msg, const char *prefix, bool first_line)
	{
		std::vector<std::string> lines;
		
		wordwrap::wrap_prefix (lines, msg, 64, prefix, first_line);
		for (auto& line : lines)
			{
				this->message (line);
			}
	}
	
	void
	player::message_wrapped (const std::string& msg, const char *prefix, bool first_line)
	{
		this->message_wrapped (msg.c_str (), prefix, first_line);
	}
	
	void
	player::message_spaced (const char *msg, bool remove_from_first)
	{
		std::vector<std::string> lines;
		
		wordwrap::wrap_spaced (lines, msg, 64, remove_from_first);
		for (auto& line : lines)
			{
				this->message (line);
			}
	}
	
	void
	player::message_spaced (const std::string& msg, bool remove_from_first)
	{
		this->message_spaced (msg.c_str (), remove_from_first);
	}
	
	
	
//----
	
	/* 
	 * Checks whether the player's rank has the given permission node.
	 */
	bool
	player::has (const char *perm)
	{
		// operators can do anything
		if (this->is_op ())
			return true;
		
		return this->get_rank ().has (perm);
	}
	
	/* 
	 * Utility function used by commands.
	 * Returns true if the player has the given permission; otherwise, it prints
	 * an error message to the player and return false.
	 */
	bool
	player::perm (const char *perm)
	{
		if (this->has (perm))
			return true;
		
		this->message (messages::insufficient_permissions (
			this->get_server ().get_groups (), perm));
		return false;
	}
	
	
	
//----
	
	/* 
	 * Loads information about the player from the server's SQL database.
	 * Returns false on failure.
	 */
	bool
	player::load_data ()
	{
		{
			soci::session sql (this->get_server ().sql_pool ());
			
			if (sqlops::player_count (sql) == 0)
				{
					this->message ("§4Congratulations§c!");
					this->message ("§cYou are the first player to log in§7, §cand thus you have been");
					this->message ("§cgiven the highest rank and have been granted §4operator §cstatus§7.");
					
					this->op = true;
					this->rnk.set (("@" + this->get_server ().get_groups ().highest ()->name).c_str (),
						this->get_server ().get_groups ());
				}
			
			this->log_last = std::time (nullptr);
			
			sqlops::player_info pd;
			bool found_player = false;
			try
				{
					found_player = sqlops::player_data (sql, this->username, this->srv, pd);
				}
			catch (const std::exception& ex)
				{
					log (LT_ERROR) << "Failed to load player data! (\"" << this->username << ") [" << ex.what () << "]" << std::endl;
					return false;
				}
			
			// common fields
			pd.last_login = this->log_last;
			pd.ip.assign (this->ip);
			
			if (found_player)
				{ 
					if (pd.banned)
						{
							this->kick ("§c[ §4You are banned from this server §c]");
							return true;
						}
					else if (sqlops::is_ip_banned (sql, this->ip))
						{
							this->kick ("§c[ §4You are §5IP§c-§4banned from this server §c]");
							return true;
						}
					
					// modify some fields
					if (pd.login_count == 0)
						pd.first_login = std::time (nullptr);
					if (pd.name.compare (this->get_username ()) != 0)
						{
							// can happen
							pd.name.assign (this->get_username ());
							pd.nick.assign (this->get_username ());
						}
						
					++ pd.login_count;
					this->dbid = pd.id;
				}
			else
				{
					pd.name.assign (this->username);
					pd.nick = pd.name;
					pd.op = this->op;
					pd.rnk = this->op ? this->rnk : this->srv.get_groups ().default_rank;
					pd.blocks_destroyed = pd.blocks_created = pd.messages_sent = 0;
					pd.first_login = std::time (nullptr);
					pd.login_count = 1;
					pd.balance = 0.0;
					pd.banned = false;
				}
			
			this->rnk = pd.rnk;
			std::strcpy (this->nick, pd.nick.c_str ());
			this->op = pd.op;
			this->bl_destroyed = pd.blocks_destroyed;
			this->bl_created = pd.blocks_created;
			this->msgs_sent = pd.messages_sent;
			this->log_first = pd.first_login;
			this->log_count = pd.login_count;
			this->bal = pd.balance;
			this->banned = pd.banned;
			
			// save to db
			try
				{
					sqlops::save_player_data (sql, this->username, this->srv, pd);
					if (!found_player)
						{
							this->dbid = sqlops::player_id (sql, this->username);
						}
				}
			catch (const std::exception& ex)
				{
					log (LT_ERROR) << "Failed to save player data! (\"" << this->username << ") [" << ex.what () << "]" << std::endl;
					this->message ("§c * §4Failed to save player data§c.");
				}
			
			// load previous position
			if (this->srv.get_config ().load_prev_pos)
				{
					std::string world;
					double pos_x, pos_y, pos_z, pos_r, pos_l;
					int gm;
				
					try
						{
							sql << "SELECT world,pos_x,pos_y,pos_z,pos_r,pos_l,gm FROM `player-logout-data` WHERE `name`='" << this->get_username () << "'",
								soci::into (world), soci::into (pos_x), soci::into (pos_y),
								soci::into (pos_z), soci::into (pos_r), soci::into (pos_l), 
								soci::into (gm);
						}
					catch (const std::exception& ex)
						{ }
					
					entity_pos last_pos = {pos_x, pos_y, pos_z, (float)pos_r, (float)pos_l, true};
					this->set_world (this->srv.get_worlds ().find (world.c_str ()));
					if (this->curr_world)
						{
							this->pos = last_pos;
							this->curr_gamemode = (pd.login_count == 1) ? (gamemode_type)this->curr_world->def_gm : ((gm == 1) ? GT_CREATIVE : GT_SURVIVAL);
						}
					else
						this->curr_gamemode = (gamemode_type)this->srv.get_main_world ()->def_gm;
				}
			else
				this->curr_gamemode = (gamemode_type)this->srv.get_main_world ()->def_gm;
		}
		
		
		std::string str;
		str.append ("§");
		str.push_back (this->rnk.main_group->color);
		str.append (this->nick);
		std::strcpy (this->colored_nick, str.c_str ());
		return true;
	}
	
	void
	player::save_data ()
	{
		if (this->pstate != PS_PLAY || !this->logged_in)
			return;
		
		{
			soci::session sql (this->get_server ().sql_pool ());
			
			sqlops::player_info pd;
			this->player_data (pd);
			try
				{
					sqlops::save_player_data (sql, this->username, this->srv, pd);
				}
			catch (const std::exception& ex)
				{
					log (LT_ERROR) << "Failed to save player data! (\"" << this->username << ") [" << ex.what () << "]" << std::endl;
				}
			
			// save current position, world, etc...
			if (this->logged_in && this->curr_world)
				{
					int count;
					sql << "SELECT Count(*) FROM `player-logout-data`", soci::into (count);
					if (count == 1)
						{
							sql.once << "UPDATE `player-logout-data` SET "
								   "`world`='" << this->curr_world->get_name () << "'"
								<< ", `pos_x`=" << this->pos.x
								<< ", `pos_y`=" << this->pos.y
								<< ", `pos_z`=" << this->pos.z
								<< ", `pos_r`=" << this->pos.r
								<< ", `pos_l`=" << this->pos.l
								<< ", `gm`=" << ((this->curr_gamemode == GT_CREATIVE) ? 1 : 0)
								<< " WHERE `name`='" << this->get_username () << "'";
						}
					else
						{
							sql.once << "INSERT INTO `player-logout-data` VALUES ("
								<< "'" << this->get_username () << "'"
								<< ", '" << this->curr_world->get_name () << "'"
								<< ", " << this->pos.x
								<< ", " << this->pos.y
								<< ", " << this->pos.z
								<< ", " << this->pos.r
								<< ", " << this->pos.l
								<< ", " << ((this->curr_gamemode == GT_CREATIVE) ? 1 : 0)
								<< ")";
						}
				}
		}
	}
	
	
	
	/* 
	 * Modifies the player's nickname.
	 */
	void
	player::set_nickname (const char *nick, bool modify_sql)
	{
		if (std::strlen (nick) > 36)
			return;
		
		std::strcpy (this->nick, nick);
		
		std::string str;
		str.append ("§");
		str.push_back (this->rnk.main ()->color);
		str.append (this->nick);
		std::strcpy (this->colored_nick, str.c_str ());
		
		if (modify_sql)
			{
				soci::session sql (this->get_server ().sql_pool ());
				sql.once << "UPDATE `players` SET `nick`='" << nick << "' WHERE `name`='"
					<< this->get_username () << "'";
			}
	}
	
	/* 
	 * Modifies the player's rank.
	 * NOTE: This does NOT update the database.
	 */
	void
	player::set_rank (const rank& rnk)
	{
		this->remove_from_tab_list ();
		this->despawn_from_all ();
	
		this->rnk = rnk;
		
		// update colored names
		
		std::string str;
		str.append ("§");
		str.push_back (this->rnk.main ()->color);
		str.append (this->nick);
		std::strcpy (this->colored_nick, str.c_str ());
		
		str.clear ();
		str.append ("§");
		str.push_back (this->rnk.main ()->color);
		str.append (this->username);
		std::strcpy (this->colored_username, str.c_str ());
		
		this->add_to_tab_list ();
		this->spawn_to_all ();
	}
	
	
	
	/* 
	 * Fills the specified player_info structure with information about the
	 * player.
	 */
	void
	player::player_data (sqlops::player_info& pd)
	{
		pd.id = this->dbid;
		pd.name.assign (this->username);
		pd.nick.assign (this->nick);
		pd.ip.assign (this->ip);
		pd.op = this->op;
		pd.rnk = this->rnk;
		pd.blocks_destroyed = this->bl_destroyed;
		pd.blocks_created = this->bl_created;
		pd.messages_sent = this->msgs_sent;
		pd.first_login = this->log_first;
		pd.last_login = this->log_last;
		pd.login_count = this->log_count;
		pd.balance = this->bal;
		pd.banned = this->banned;
	}
	
	
	
//----
	
	bool
	player::have_marking_callbacks ()
	{
		if (this->mark_callbacks.empty ())
			return false;
		
		for (auto itr = this->mark_callbacks.begin (); itr != this->mark_callbacks.end (); ++itr)
			{
				auto& cb = *itr;
				if (cb.size () > 0)
					return true;
			}
		return false;
	}
	
	callback<bool (player *, block_pos[], int)>&
	player::get_nth_marking_callback (int n)
	{
		if ((int)this->mark_callbacks.size () < n)
			this->mark_callbacks.resize (n);
		return this->mark_callbacks[n - 1];
	}
	
	bool
	player::mark_block (int x, int y, int z)
	{
		if (this->have_marking_callbacks ())
			{
				// undo the change.
				this->send (packets::play::make_block_change (
					x, y, z,
					this->get_world ()->get_id (x, y, z),
					this->get_world ()->get_meta (x, y, z)));
				
				this->marked_blocks.emplace_back (x, y, z);
				
				bool executed = false;
				if (this->mark_callbacks.size () >= this->marked_blocks.size ())
					{
						// execute the callback(s)
						for (size_t i = 0; i < this->marked_blocks.size (); ++i)
							{
								auto& cb = this->mark_callbacks[i];
								if (cb.size () == 0) continue;
								
								block_pos *arr = new block_pos[i + 1];
								for (size_t j = 0; j < (i + 1); ++j)
									arr[j] = this->marked_blocks[j];
								cb (this, arr, i + 1);
								executed = true;
							}
					}
				
				if (executed)
					this->marked_blocks.clear ();
				
				return true;
			}
		
		return false;
	}
	
	void
	player::stop_marking ()
	{
		this->mark_callbacks.clear ();
	}
	
	
	
	/* 
	 * Used by world selections:
	 */
	
	void
	player::sb_add_nolock (int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		auto itr = this->sel_blocks.find (selection_block (x, y, z));
		if (itr != this->sel_blocks.end ())
			{ ++ itr->counter; return; }
		
		this->sb_updates.set (x, y, z, this->sb_block.id, this->sb_block.meta);
		this->sel_blocks.insert (selection_block (x, y, z, 1));
	}
	
	void
	player::sb_remove_nolock (int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		auto itr = this->sel_blocks.find (selection_block (x, y, z));
		if (itr == this->sel_blocks.end ())
			return;
		
		-- itr->counter;
		if (itr->counter == 0)
			{
				this->sel_blocks.erase (itr);
				//block_data bd = this->curr_world->get_block (x, y, z);
				this->sb_updates.set (x, y, z, ES_REM);
			}
	}
	
	bool
	player::sb_exists_nolock (int x, int y, int z)
	{
		if (y < 0 || y > 255) return false;
		return (this->sel_blocks.find (selection_block (x, y, z))
			!= this->sel_blocks.end ());
	}
	
	void
	player::sb_commit_nolock ()
	{
		this->sb_updates.preview_to (this, false);
	}
	
	void
	player::sb_add (int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->sb_lock};
		this->sb_add_nolock (x, y, z);
	}
	
	void
	player::sb_remove (int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->sb_lock};
		this->sb_remove_nolock (x, y, z);
	}
	
	bool
	player::sb_exists (int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->sb_lock};
		return this->sb_exists_nolock (x, y, z);
	}
	
	void
	player::sb_commit ()
	{
		std::lock_guard<std::mutex> guard {this->sb_lock};
		this->sb_commit_nolock ();
	}
	
	void
	player::sb_send (int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		this->send (packets::play::make_block_change (x, y, z, this->sb_block.id,
			this->sb_block.meta));
	}
	
	/* 
	 * Returns the next unused selection number (@1, @2, ...).
	 */
	int
	player::sb_next_unused ()
	{
		int num = 1;
		std::ostringstream ss;
		
		for (;;)
			{
				ss << num;
				auto itr = this->selections.find (ss.str ().c_str ());
				if (itr == this->selections.end ())
					return num;
				
				ss.clear (); ss.str (std::string ());
				++ num;
			}
	}
	
	
	
	/* 
	 * 
	 */
	
	void
	player::es_add (edit_stage *es)
	{
		std::lock_guard<std::mutex> wguard {this->world_lock};
		this->edstages.insert (es);
	}
	
	void
	player::es_remove (edit_stage *es)
	{
		std::lock_guard<std::mutex> wguard {this->world_lock};
		this->edstages.erase (es);
	}
	
	
	
	/* 
	 * These three functions can be used to store additional general-purpose
	 * data for various things (such as, say, drawing operations).
	 */
	 
	void
	player::create_data (const char *name, void *data, void (*dctor) (void *))
	{
		std::lock_guard<std::mutex> guard {this->data_lock};
		
		auto itr = this->extra_data.find (name);
		if (itr != this->extra_data.end ())
			{
				if (itr->second.dctor)
					itr->second.dctor (itr->second.data);
				this->extra_data.erase (itr);
			}
		
		player_extra_data d;
		d.data = data;
		d.dctor = dctor;
		this->extra_data[name] = d;
	}
	
	void
	player::delete_data (const char *name, bool destruct)
	{
		std::lock_guard<std::mutex> guard {this->data_lock};
		
		auto itr = this->extra_data.find (name);
		if (itr == this->extra_data.end ())
			return;
		
		if (destruct)
			itr->second.dctor (itr->second.data);
		this->extra_data.erase (itr);
	}
	
	void*
	player::get_data (const char *name)
	{
		std::lock_guard<std::mutex> guard {this->data_lock};
		auto itr = this->extra_data.find (name);
		if (itr == this->extra_data.end ())
			return nullptr;
		return itr->second.data;
	}
	
	
	
	/* 
	 * Modifies the player's gamemode.
	 */
	void
	player::change_gamemode (gamemode_type gm)
	{
		if (this->gamemode () == gm)
			return;
		
		this->curr_gamemode = gm;
		this->send (packets::play::make_change_game_state (3, (gm == GT_CREATIVE) ? 1 : 0));
	}
	
	
	
	/* 
	 * Modifies the entity's health.
	 */
	void
	player::set_health (int hearts, int hunger, float hunger_saturation)
	{
		int p_hearts = hearts;
		if (p_hearts < 0) p_hearts = 0;
		if (p_hearts > 20) p_hearts = 20;
		
		int p_hunger = hunger;
		if (p_hunger < 0) p_hunger = 0;
		if (p_hunger > 20) p_hunger = 20;
		
		bool hurt = p_hearts < this->hearts;
		this->send (packets::play::make_update_health (p_hearts, p_hunger, hunger_saturation));
		if (hurt)
			{
				this->send (packets::play::make_entity_status (this->eid, 2));
				
				// keep player from regenerating hearts immediately.
				this->last_heart_regen = std::chrono::steady_clock::now ();
				
				// notify others too
				this->get_world ()->get_players ().send_to_all_visible (
					packets::play::make_entity_status (this->eid, 2), this);
				
				if (hearts <= 0)
					{
						// die
						this->handle_death ();
					}
			}
		
		living_entity::set_health (hearts, hunger, hunger_saturation);
	}
	
	
	
	void
	player::handle_falls_and_jumps (bool prev, bool curr, entity_pos old_pos)
	{
		if ((!prev && curr) && (this->curr_gamemode != GT_CREATIVE))
			{
				/* 
				 * The player has hit the ground.
				 */
				
				if (this->fall_flag)
					this->fall_flag = false;
				else
					{
						double delta = this->pos.y - this->last_ground_height;
						if (delta <= -3.0)
							{
								double fd = (-delta) - 2.0;
								this->last_heart_regen = std::chrono::steady_clock::now ();
								this->set_hearts (this->hearts - (int)fd);
							}
					}
			}
		else if ((prev && !curr) && (this->pos.y > old_pos.y))
			{
				/* 
				 * The player has jumped.
				 */
				if (this->is_sprinting ())
					this->increment_exhaustion (0.8);
				else
					this->increment_exhaustion (0.2);
			}
		
		if (curr)
			this->last_ground_height = this->pos.y;
	}
	
	
	void 
	player::handle_death ()
	{
		this->despawn_from_all ();
		
		// drop everything
		if (this->curr_gamemode != GT_CREATIVE)
			{
				std::minstd_rand rnd (utils::ns_since_epoch ());
				std::uniform_real_distribution<> dis (0, 2);
				
				entity_pos pos = this->pos;
				for (int i = 0; i <= 44; ++i)
					{
						slot_item s = this->inv.get (i);
						if (!s.empty ())
							{
								e_pickup *pick = new e_pickup (this->srv, s);
								pick->pos.set_pos (pos.x + dis (rnd), pos.y + dis (rnd), pos.z + dis (rnd));
								this->get_world ()->spawn_entity (pick);
							}
					}
			}
		this->inv.clear ();
		
	}
	
	
	
	bool
	player::got_known_chunks_for (world *w)
	{
		std::lock_guard<std::mutex> wguard {this->world_lock};
		for (known_chunk kc : this->known_chunks)
			if (kc.w == w)
				return true;
		
		return false;
	}
	
	
	
	/* 
	 * Sets the specified window as the active one, and closes any already open
	 * windows. 
	 */
	void
	player::open_window (window *win)
	{
		std::lock_guard<std::mutex> wguard {this->data_lock};
		if (this->open_win == win)
			return;
		
		if (this->open_win)
			{
				// close it
				this->send (packets::play::make_close_window (this->open_win->wid ()));
			}
		
		this->open_win = win;
		win->show (this);
	}
	
	
	
//----
	
	/* 
	 * Called by the world that's holding the entity every tick (50ms).
	 * A return value of true will cause the world to destroy the entity.
	 */
	bool
	player::tick (world &w)
	{
		if (this->bad ())
			return true;
		
		std::chrono::steady_clock::time_point now
			= std::chrono::steady_clock::now ();
		
		// regenerate hearts
		if (this->hearts < 20 && (this->hunger >= 18))
			{
				if ((now - this->last_heart_regen) >= this->heal_delay)
					{
						this->set_hearts (this->hearts + 1);
						this->last_heart_regen = now;
					}
			}
		else if ((this->hunger <= 0) && (this->curr_gamemode != GT_CREATIVE))
			{
				if ((now - this->last_heart_regen) >= this->heal_delay)
					{
						this->set_hearts (this->hearts - 1);
						this->last_heart_regen = now;
					}
			}
		
		// eat
		if (this->eating && ((now - this->eat_time) >= std::chrono::seconds (2)))
			{
				slot_item s = this->inv.get (this->held_slot);
				food_info finf = item_info::get_food_info (s.id ());
				if (finf.hunger != 0) 
					{
						s.set_amount (s.amount () - 1);
						this->inv.set (this->held_slot, s, true);
						
						// tell the client to stop eating
						// the entity status packet seems to restore hunger itself.
						this->send (packets::play::make_entity_status (this->eid, 9));
						this->set_health (this->hearts, this->hunger + finf.hunger,
							this->hunger_saturation + finf.saturation);
					}
				
				this->eat_time = now;
				this->eating = false;
			}
		
		// stream chunks
		if (!this->streaming_chunks && (tick_counter % 10 == 0))
			this->srv.get_thread_pool ().enqueue (
				[] (void *ptr) { (static_cast<player *> (ptr))->stream_chunks (); }, this);
		
		// send time
		if (this->tick_counter % 40 == 0)
			this->send (packets::play::make_time_update (w.get_time (), w.get_time ()));
		
		this->last_tick = now;
		++ tick_counter;
		return false;
	}
	
	
	
	bool
	player::handle_crafting (unsigned char wid)
	{
		window *win = this->open_win;
		if (!win)
			{
				if (wid != 0)
					return false;
				win = &this->inv;
			}
		else if (win->type () != WT_WORKBENCH)
			return false;
		
		log (LT_DEBUG) << "handle_crafting ():" << std::endl;
		
		slot_item **grid = new slot_item* [3];
		for (int i = 0; i < 3; ++i)
			grid[i] = new slot_item [3];
			
		// lay down crafting grid
		if (win->type () == WT_INVENTORY)
			{
				grid[0][0] = win->get (1);
				grid[0][1] = win->get (2);
				grid[1][0] = win->get (3);
				grid[1][1] = win->get (4);
			}
		else
			{
				grid[0][0] = win->get (1);
				grid[0][1] = win->get (2);
				grid[0][2] = win->get (3);
				grid[1][0] = win->get (4);
				grid[1][1] = win->get (5);
				grid[1][2] = win->get (6);
				grid[2][0] = win->get (7);
				grid[2][1] = win->get (8);
				grid[2][2] = win->get (9);
			}
		
		// try to find a match
		crafting_manager& cman = this->srv.get_crafting_manager ();
		const crafting_recipe *recipe = cman.match (grid);
		if (recipe)
			{
				log (LT_DEBUG) << "  Matched a recipe!" << std::endl;
				win->set (0, recipe->result);
			}
		else
			{
				win->set (0, slot_item ());
			}
		
		for (int i = 0; i < 3; ++i)
			delete[] grid[i];
		delete[] grid;
		
		return false;
	}
	
	void
	player::handle_crafting_take (unsigned char wid)
	{
		window *win = this->open_win;
		if (!win)
			{
				if (wid != 0)
					return;
				win = &this->inv;
			}
		else if (win->type () != WT_WORKBENCH)
			return;
			
		// claim items
		if (win->type () == WT_INVENTORY)
			{
				for (int i = 1; i <= 4; ++i)
					win->get (i).take (1);
			}
		else
			{
				for (int i = 1; i <= 9; ++i)
					win->get (i).take (1);
			}
		
		this->handle_crafting (wid);
	}
	
	static slot_item*
	_craft_all_inv (inventory& inv, crafting_manager& cman)
	{
		slot_item **grid = new slot_item* [3];
		for (int i = 0; i < 3; ++i)
			grid[i] = new slot_item [3];
		
		const crafting_recipe *recipe = nullptr;
		slot_item *result = new slot_item ();
		
		// lay down crafting grid
		grid[0][0] = inv.get (1);
		grid[0][1] = inv.get (2);
		grid[1][0] = inv.get (3);
		grid[1][1] = inv.get (4);
		
		for (;;)
			{
				recipe = cman.match (grid);
				if (recipe)
					{
						if (result->empty ())
							result->set (recipe->result);
						else if (result->compatible_with (recipe->result))
							result->give (1);
						else
							break;
					}
				else
					break;
				
				// claim items
				for (int i = 0; i < 2; ++i)
					for (int j = 0; j < 2; ++j)
					grid[i][j].take (1);
			}
		
		for (int i = 0; i < 3; ++i)
			delete[] grid[i];
		delete[] grid;
		
		if (result->empty ())
			{
				delete result;
				return nullptr;
			}
		return result;
	}
	
	
	
	
//----
	
	static void
	_place_sign (player *pl, int x, int y, int z, char dir)
	{
		if (!pl->has ("place.sign"))
			{
				pl->message (messages::not_allowed ());
				return;
			}
		
		if (dir == 1)
			{
				int meta = ((int)(std::fmod (pl->pos.r - 11.25, 360.0) / 22.5) + 8) & 0xF;
			
				pl->get_world ()->queue_update (x, y, z, BT_SIGN_POST, meta, 0, 0, nullptr, pl);
				++ pl->bl_created;
				
				pl->send (packets::play::make_open_sign_editor (x, y, z));
			}
		else if (dir > 1 && dir < 6)
			{
				pl->get_world ()->queue_update (x, y, z, BT_WALL_SIGN, dir, 0, 0, nullptr, pl);
				++ pl->bl_created;
				
				pl->send (packets::play::make_open_sign_editor (x, y, z));
			}
	}
	
	
	static void
	_place_slab (player *pl, int x, int y, int z, int dir, const slot_item& held, int cy)
	{
		unsigned short slab = held.id ();
		unsigned short dslab = (slab == BT_WOODEN_SLAB) ? BT_WOODEN_DSLAB : BT_DSLAB;
		
		int nx = x, ny = y, nz = z;
		blocki bl = { held.id (), (unsigned char)held.damage () };
		
		world *wr = pl->get_world ();
		block_data old = wr->get_block (x, y, z);
		switch (dir)
			{
				// -y
				case 0:
					if (old.id == slab && (old.meta & 0x7) == held.damage () && (old.meta & 0x8))
						bl = { dslab, (unsigned char)held.damage () };
					else
						{
							-- ny;
							bl = { held.id (), (unsigned char)(0x8 | held.damage ()) };
						}
					break;
				
				// +y
				case 1:
					if (old.id == slab && (old.meta == held.damage ()))
						bl = { dslab, (unsigned char)held.damage () };
					else
						++ ny;
					break;
				
				default:
					switch (dir)
						{
							case 2: -- nz; break;
							case 3: ++ nz; break;
							case 4: -- nx; break;
							case 5: ++ nx; break;
							
							default:
								return; // kick player?
						}
					
					old = wr->get_block (nx, ny, nz);
					if (cy >= 8)
						{
							if (old.id == BT_AIR)
								bl = { held.id (), (unsigned char)(0x8 | held.damage ()) };
							else if ((old.id == held.id ()) && (old.meta == held.damage ()))
								bl = { dslab, (unsigned char)held.damage () };
							else
								return;
						}
					else
						{
							if (old.id == BT_AIR)
								bl = { held.id (), (unsigned char)held.damage () };
							else if ((old.id == held.id ()) && ((old.meta & 0x7) == held.damage () && (old.meta & 0x8)))
								bl = { dslab, (unsigned char)held.damage () };
							else
								return;
						}
					break;
			}
		
		wr->queue_update (nx, ny, nz, bl.id, bl.meta, 0, 0, nullptr, pl);
		++ pl->bl_created;
	}
	
	
	static bool
	_is_stairs_block (int id)
	{
		switch (id)
			{
				case BT_OAK_STAIRS:
				case BT_COBBLE_STAIRS:
				case BT_BRICK_STAIRS:
				case BT_STONE_BRICK_STAIRS:
				case BT_NETHER_BRICK_STAIRS:
				case BT_SANDSTONE_STAIRS:
				case BT_SPRUCE_STAIRS:
				case BT_BIRCH_STAIRS:
				case BT_JUNGLE_STAIRS:
				case BT_QUARTZ_STAIRS:
				case BT_ACACIA_STAIRS:
				case BT_DARK_OAK_STAIRS:
					return true;
				
				default:
					return false;
			}
	}
	
	static void
	_place_stairs (player *pl, int x, int y, int z, int dir, const slot_item& held, int cy)
	{
		double rot = pl->pos.r + 45.0;
		if (rot < 0.0)
			rot += ((int)(std::abs (rot) / 360.0) + 1) * 360.0;
		
		const static int arr[] = { 2, 1, 3, 0 };
		int ind = (int)(std::fmod (rot, 360.0) / 90.0);
		int meta = arr[ind];
		
		if (dir == 0 || (dir >= 2 && cy >= 8))
			meta |= 0x4; // upside down
		
		pl->get_world ()->queue_update (x, y, z, held.id (), meta, 0, 0, nullptr, pl);
		++ pl->bl_created;
	}
	
	
	static void
	_place_trunk (player *pl, int x, int y, int z, int dir, const slot_item& held)
	{
		int d = 0;
		switch (dir)
			{
				// -y +y
				case 0:
				case 1:
					d = 0;
					break;
				
				// -z +z
				case 2:
				case 3:
					d = 8;
					break;
				
				// -x +x
				case 4:
				case 5:
					d = 4;
					break;
			}
		
		pl->get_world ()->queue_update (x, y, z, held.id (), d | held.damage (), 0, 0, nullptr, pl);
		++ pl->bl_created;
	}
	
	
	static void
	_place_torch (player *pl, int x, int y, int z, int dir)
	{
	  world *wr = pl->get_world ();
	  if (wr->get_id (x, y, z) != BT_AIR)
	    return;
	  
	  int m = 0;
	  switch (dir)
	    {
	      case 0:
	        pl->send_orig_block (x, y, z);
	        return;
	      
	      case 1: m = 0; break;
	      case 2: m = 4; break;
	      case 3: m = 3; break;
	      case 4: m = 2; break;
	      case 5: m = 1; break;
	    }
	  
	  wr->queue_update (x, y, z, BT_TORCH, m, 0, 0, nullptr, pl);
	}
	
	
	
//----
	
	/* 
	 * Called by the authenticator to inform the player that it's ready to spawn.
	 */
	void
	player::done_authenticating ()
	{
		this->authenticated = true;
		
		this->decryptor = new CryptoPP::CFB_Mode<CryptoPP::AES>::Decryption (this->ssec, 16, this->ssec, 1);
		
		// begin encryption
		this->encryptor = new CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption (this->ssec, 16, this->ssec, 1);
		this->encrypted = true;
		
		this->login ();
	}
	
	
	/* 
	 * Sends the world and spawns the player.
	 */
	void
	player::login ()
	{
		if (this->logged_in)
			return;
		
		log () << "Player " << this->username << " has logged in from @" << this->get_ip ()
		       << " [uuid: " << this->get_uuid ().to_str () << "]" << std::endl;
		
		this->send (packets::login::make_login_success (
			this->uuid.to_str ().c_str (), this->username));
		this->pstate = PS_PLAY;
		
		if (!this->load_data ())
			{
				this->disconnect ();
				return;
			}
		else if (this->kicked) // kicked by load_data()
			return;
		
		{
			// build colored username
			std::string str;
			str.append ("§");
			str.push_back (this->rnk.main ()->color);
			str.append (this->username);
			std::strcpy (this->colored_username, str.c_str ());
		}
		
		this->send (packets::play::make_join_game (this->get_eid (),
			this->curr_gamemode, 0, 0, (this->get_server ().get_config ().max_players > 64)
				? 64 : (this->get_server ().get_config ().max_players), "hCraft"));
		this->logged_in = true;
		if (!this->get_server ().done_connecting (this))
			{
				this->disconnect ();
				return;
			}
		
		// join message
		this->get_server ().get_players ().message (
			messages::compile (this->srv.msgs.server_join, messages::environment (this)));
		for (const std::string& str : this->get_server ().msgs.join_msg)
			this->message (messages::compile (str, messages::environment (this)));
		if (this->get_server ().get_irc ())
			this->get_server ().get_irc ()->chan_msg (std::string ("+ ") + this->get_colored_username () + " §0has joined the server");
		
		this->inv.subscribe (this);
		
		// make the player move at normal speed
		{
			std::vector<entity_property> props;
			props.push_back ({"generic.movementSpeed", 0.1});
			this->send (packets::play::make_entity_properties (this->eid, props));
		}
		
		if (this->curr_world)
			this->join_world_at (this->curr_world, this->pos, true, true);
		else
			this->join_world (this->get_server ().get_main_world (), true, true);
	}
	
	
	
//----
	
	/* 
	 * Packet handlers:
	 * NOTE: These return 0 on success (any other value will disconnect the
	 *       player).
	 */
	
	// dummy handler, doesn't do anything.
	int
	handle_packet_xx (player *pl, packet_reader reader)
		{ return 0; }
	
	
	
	
//------------------------------------------------------------------------------
	/* 
	 * Protocol state: Handshake.
	 */
	
	int
	player::handle_hs_packet_00 (player *pl, packet_reader reader)
	{
		int proto = reader.read_varint ();
		if (proto != packet::protocol_version)
			{ 
				if (proto < packet::protocol_version)
					pl->kick ("§cOutdated client", "outdated protocol version");
				else
					pl->kick ("§ahCraft has not been updated yet", "newer protocol version");
				return 0;
			}
		
		char hostname[256];
		if (reader.read_string (hostname, 255) == -1)
			return -1;
		reader.read_short (); // port
		
		int state = reader.read_varint ();
		if (state == 1)
			{
				// state: status
				pl->pstate = PS_STATUS;
			}
		else if (state == 2)
			{
				// state: login
				pl->pstate = PS_LOGIN;
			}
		else
			return -1;
		
		return 0;
	}
	
	
	
//------------------------------------------------------------------------------
	/* 
	 * Protocol state: Status
	 */
	
	int
	player::handle_st_packet_00 (player *pl, packet_reader reader)
	{
		if (pl->pstate != PS_STATUS)
			return -1;
		
		// build reponse string
		json::object js {}; 
		{
			json::object *obj_version = new json::object ();
			obj_version->insert_string ("name", packet::game_version);
			obj_version->insert_number ("protocol", packet::protocol_version);
			js.insert ("version", obj_version);
		}
		{
			json::object *obj_players = new json::object ();
			obj_players->insert_number ("max", pl->srv.get_config ().max_players);
			obj_players->insert_number ("online", pl->srv.get_players ().count ());
			obj_players->insert ("sample", new json::array ());
			js.insert ("players", obj_players);
		}
		{
			json::object *obj = new json::object ();
			obj->insert_string ("text", pl->srv.get_config ().srv_motd);
			js.insert ("description", obj);
		}
		
		std::ostringstream ss;
		js.write (ss);
		pl->send (packets::status::make_response (ss.str ().c_str ()));
		
		return 0;
	}
	
	int
	player::handle_st_packet_01 (player *pl, packet_reader reader)
	{
		if (pl->pstate != PS_STATUS)
			return -1;
		
	 	long long time = reader.read_long ();
		pl->send (packets::status::make_ping (time));
		
		return 0;
	}
	
	
	
//------------------------------------------------------------------------------
	/* 
	 * Protocol state: Login
	 */
	
	int
	player::handle_lg_packet_00 (player *pl, packet_reader reader)
	{
		if (pl->pstate != PS_LOGIN)
			return -1;
		
		char username[64];
		int ulen = reader.read_string (username, 16);
		if ((ulen < 2 || ulen > 16) || !sutils::is_valid_username (username))
			{
				pl->log (LT_WARNING) << "@" << pl->get_ip () << " connected with an invalid username." << std::endl;
				return -1;
			}
		
		///*
		// Used when testing
		{
			static const char *names[] =
				{ "BizarreCake", "testdude", "testdude2", "testdude3", "testdude4" };
			static int index = 0, count = 5;
			const char *cur = names[index++];
			if (index >= count)
				index = 0;
			std::strcpy (username, cur);
		}
		//*/
		
		std::strcpy (pl->username, username);
		
		// encryption\authentication
		if (pl->srv.get_config ().online_mode)
			{
				auto pkey = pl->srv.public_key ();
			
				// verification token
				std::minstd_rand rnd (utils::ns_since_epoch ());
				std::uniform_int_distribution<> dis (0, 255);
				for (int i = 0; i < 4; ++i)
					pl->vtoken[i] = dis (rnd);
			
				pl->send (
					packets::login::make_encryption_request (pl->srv.auth_id (), pkey, pl->vtoken));
			}
		else
			{
				pl->login ();
			}
		
		return 0;
	}
	
	int
	player::handle_lg_packet_01 (player *pl, packet_reader reader)
	{
		unsigned short ssec_len = reader.read_short ();
		unsigned char ssec[1024];
		reader.read_bytes (ssec, ssec_len);
		
		unsigned short vtoken_len = reader.read_short ();
		unsigned char vtoken[1024];
		reader.read_bytes (vtoken, vtoken_len);
		
		CryptoPP::AutoSeededRandomPool rng;
		CryptoPP::RSAES_PKCS1v15_Decryptor decrypt (pl->srv.private_key ());
		
		// check four verification bytes
		{
			CryptoPP::SecByteBlock ct_vtoken (vtoken, vtoken_len);
			size_t dpl = decrypt.MaxPlaintextLength (ct_vtoken.size ());
			CryptoPP::SecByteBlock rec ((dpl));
			auto res = decrypt.Decrypt (rng, ct_vtoken, ct_vtoken.size (), rec);
			if (res.messageLength != 4)
				{
					pl->log (LT_WARNING) << "Player \"" << pl->username << " failed token verification" << std::endl;
					return -1;
				}
			rec.resize (res.messageLength);
			for (int i = 0; i < 4; ++i)
				if (pl->vtoken[i] != rec[i])
					{
						pl->log (LT_WARNING) << "Player \"" << pl->username << " failed token verification" << std::endl;
						return -1;
					}
		}
		
		// decrypt shared secret
		{
			CryptoPP::SecByteBlock ct_ssec (ssec, ssec_len);
			size_t dpl = decrypt.MaxPlaintextLength (ct_ssec.size ());
			CryptoPP::SecByteBlock rec ((dpl));
			auto res = decrypt.Decrypt (rng, ct_ssec, ct_ssec.size (), rec);
			if (res.messageLength != 16)
				{
					pl->log (LT_WARNING) << "Player \"" << pl->username << " sent invalid shared secret" << std::endl;
					return -1;
				}
			std::memcpy (pl->ssec, rec.data (), 16);
		}
		
		pl->srv.auth.enqueue (pl);
		return 0;
	}
	
	
	
//------------------------------------------------------------------------------
	/* 
	 * Protocol state: Play
	 */
	
	int
	player::handle_pl_packet_00 (player *pl, packet_reader reader)
	{
		int id = reader.read_byte ();
		
		if (!pl->ping_waiting)
			return 0;
		
		if ((id != 0) && id != pl->ping_id)
			{
				pl->kick ("§cPing timeout");
				return 0;
			}
		
		pl->ping_time_ms = std::chrono::duration_cast<std::chrono::milliseconds> (
			std::chrono::system_clock::now () - pl->last_ping).count ();
		pl->ping_waiting = false;
		
		return 0;
	}
	
	
	
	// @player message
	static void
	_handle_private_message (player *pl, std::string& str)
	{
		if (!pl->get_rank ().main ()->can_chat) return;
		const char *ptr = str.c_str () + 1;
		
		std::string target_name;
		while (*ptr)
			{
				if (*ptr == ' ')
					break;
				target_name.push_back (*ptr++);
			}
		
		if (!*ptr || target_name.empty ())
			{
				pl->message ("§c * §7Usage§f: §b@§cplayer message");
				return;
			}
		
		while (*ptr == ' ')
			++ ptr;
		if (!*ptr)
			{
				pl->message ("§c * §7Usage§f: §b@§cplayer message");
				return;
			}
		
		std::string msg;
		while (*ptr)
			msg.push_back (*ptr++);
		
		player *target = pl->get_server ().get_players ().find (target_name.c_str ());
		if (!target)
			{
				pl->message ("§c * §7Unknown player§f: §c" + target_name);
				return;
			}
		else if (target == pl)
			{
				pl->message ("§c * §7Stop talking to yourself§c.");
				return;
			}
		
		std::ostringstream ss;
		ss << "§8§o(from " << pl->get_username () << ") §r§3" << msg;
		target->message (ss.str ());
		
		ss.str (std::string ());
		ss << "§8§o(to " << target->get_username () << ") §r§3" << msg;
		pl->message (ss.str ());
	}
	
	// *action
	static void
	_handle_action_message (player *pl, std::string& str)
	{
		if (!pl->get_rank ().main ()->can_chat) return;
		if (pl->get_server ().is_player_muted (pl->get_username ()) && !pl->is_op ())
			{
				pl->message ("§cYou have been muted§4, §cstay quiet§4.");
				return;
			}
		
		const char *ptr = str.c_str () + 1;
		
		while (*ptr == ' ')
			++ ptr;
		if (!*ptr)
			{
				pl->message ("§c * §7Usage§f: §b*§caction");
				return;
			}
		
		std::string msg {"/me "};
		while (*ptr)
			msg.push_back (*ptr++);
		
		command_reader cread {msg};
		command *cmd = pl->get_server ().get_commands ().find ("me");
		if (!cmd)
			{
				pl->message ("§c * §eNo such command§f: §cme§f.");
				return;
			}
		
		cmd->execute (pl, cread);
	}
	
	
	static void
	handle_message (player *pl, std::string& msg)
	{
		if (pl->get_rank ().main ()->color_codes || pl->is_op ())
			{
				// convert color codes from &X to §X
				msg = sutils::convert_colors (msg);
			}
	}
	
	static std::string
	build_message (const std::string& nick, const rank& rnk, const std::string& msg,
		bool global, char text_color = 0)
	{
		std::ostringstream ss;
		
		if (global)
			ss << "§c# ";
		
		group *mgrp = rnk.main ();
		ss << mgrp->mprefix;
		for (group *grp : rnk.groups)
			ss << grp->prefix;
		ss << "§" << mgrp->color << nick;
		ss << mgrp->msuffix;
		for (group *grp : rnk.groups)
			ss << grp->suffix;
		ss << " §" << ((text_color == 0) ? mgrp->text_color : text_color) << msg;
		
		return ss.str ();
	}
	
	int
	player::handle_pl_packet_01 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		
		bool is_global_message = false;
		
		char text[384];
		int  text_len = reader.read_string (text, 360);
		if (text_len <= 0)
			{
				pl->log (LT_WARNING) << "Received invalid string from player " << pl->get_username () << std::endl;
				return -1;
			}
		
		std::string msg {text};
		
		if (msg[msg.size () - 1] == '\\')
			{
				msg.pop_back ();
				if (sutils::is_empty (msg))
					{ msg.push_back ('\\'); goto continue_write; }
				
				pl->msgbuf << msg;
				pl->message ("§7 * §8Message appended to buffer§7.");
				return 0;
			}
		
		if (pl->msgbuf.tellp () > 0)
			{
				pl->msgbuf << msg;
				msg.assign (pl->msgbuf.str ());
				pl->msgbuf.str (std::string ());
				pl->msgbuf.clear ();
			}
		
		handle_message (pl, msg);
		
		// handle commands
		if (msg[0] == '/')
			{
				command_reader cread {msg};
				const std::string& cname = cread.command_name ();
				if (cname.empty ())
					{
						pl->message ("§c * §ePlease enter a command§f.");
						return 0;
					}
				
				command *cmd = pl->get_server ().get_commands ().find (cname.c_str ());
				if (!cmd)
					{
						if (cname.size () > 32)
							pl->message ("§c * §eNo such command§f.");
						else
							pl->message (("§c * §eNo such command§f: §c" + cname + "§f.").c_str ());
						return 0;
					}
				
				cmd->execute (pl, cread);
				return 0;
			}
		
		if (pl->get_server ().is_player_muted (pl->get_username ()))
			{
				pl->message ("§cYou have been muted§4, §cstay quiet§4.");
				return 0;
			}
		
		// detect spam
		{
			auto now = std::chrono::steady_clock::now ();
			if (pl->chat_spam_log.size () >= 4)
				{
					auto t = pl->chat_spam_log.front ();
					pl->chat_spam_log.pop ();
					
					if (std::chrono::duration_cast<std::chrono::seconds> (
						std::chrono::steady_clock::now () - t).count () <= 3)
						{
							++ pl->chat_spam_warnings;
							if (pl->chat_spam_warnings > 2)
								{
									pl->get_server ().mute_player (pl->get_username (), 5 * 10); // five minutes
									std::ostringstream ss;
									ss << "§c > " << pl->get_colored_nickname () << " §7has been muted for §e5 minutes";
									pl->get_server ().get_players ().message (ss.str ());
									return 0;
								}
						}
				}
			pl->chat_spam_log.push (now);
		}
		
		if (msg[0] == '@')
			{
				_handle_private_message (pl, msg);
				return 0;
			}
		else if (msg[0] == '*')
			{
				_handle_action_message (pl, msg);				
				return 0;
			}
		
		// handle chat modes
		if (msg[0] == '#')
			{
				// server-wide message
				
				if (sutils::ltrim (msg.erase (0, 1)).empty ())
					{
						msg.push_back ('#');
						goto continue_write;
					}
				
				is_global_message = true;
			}
		else if (msg[0] == '@')
			{
				// TODO: handle private messages
			}
			
	continue_write:
		if (!pl->rnk.main ()->can_chat)
			return 0;
		
		std::string out = build_message (pl->get_nickname (), pl->get_rank (), msg,
			is_global_message);
		
		player_list *target;
		if (is_global_message)
			{
				pl->get_logger () (LT_CHAT) << "#" << pl->get_username () << ": " << msg << std::endl;
				target = &pl->get_server ().get_players ();
				
				irc_client *ircc = pl->get_server ().get_irc ();
				if (ircc)
					ircc->chan_msg (std::string (pl->get_colored_username ()) + "§7[§3#§7]: §0" + msg);
			}
		else
			{
				pl->get_logger () (LT_CHAT) << pl->get_username () << ": " << msg << std::endl;
				target = &pl->get_world ()->get_players ();
				
				irc_client *ircc = pl->get_server ().get_irc ();
				if (ircc)
					ircc->chan_msg (std::string (pl->get_colored_username ()) + "§7[§3@" + pl->get_world ()->get_colored_name () + "§7]: §0" + msg);
			}
		
		target->all (
			[&out] (player *pl)
				{
					pl->message (out.c_str ());
				}, pl);
		
		// highlight message to self
		pl->message (build_message (pl->get_nickname (), pl->get_rank (), msg,
			is_global_message, pl->get_server ().get_config ().self_highlight_color));
		
		return 0;
	}
	
	
	
	/* 
	 * Calculates the amount of damage (in hearts) that should be dealt after
	 * being hit by the specified player.
	 */
	int
	_calc_hit_damage (player *pl)
	{
	  slot_item& held = pl->held_item ();
	  switch (held.id ())
	    {
	    case IT_WOODEN_SHOVEL:
	    case IT_GOLDEN_SHOVEL:
	      return 2;
	    
	    case IT_STONE_SHOVEL:
	    case IT_WOODEN_PICKAXE:
	    case IT_GOLDEN_PICKAXE:
	      return 3;
	    
	    case IT_IRON_SHOVEL:
	    case IT_STONE_PICKAXE:
	    case IT_WOODEN_AXE:
	    case IT_GOLDEN_AXE:
	      return 4;
	    
	    case IT_DIAMOND_SHOVEL:
	    case IT_IRON_PICKAXE:
	    case IT_STONE_AXE:
	    case IT_WOODEN_SWORD:
	    case IT_GOLDEN_SWORD:
	      return 5;
	    
	    case IT_DIAMOND_PICKAXE:
	    case IT_IRON_AXE:
	    case IT_STONE_SWORD:
	      return 6;
	    
	    case IT_DIAMOND_AXE:
	    case IT_IRON_SWORD:
	      return 7;
	    
	    case IT_DIAMOND_SWORD:
	      return 8;
	    }
	  
	  return 1;
	}
	
	int
	player::handle_pl_packet_02 (player *pl, packet_reader reader)
	{
	  int eid = reader.read_int ();
	  char mouse = reader.read_byte ();
	  
	  world *wr = pl->get_world ();
	  entity *ent = pl->get_server ().entity_by_id (eid);
	  if (!ent)
	    return 0;
	  
	  // left click
	  if (mouse == 1)
	    {
	      living_entity *target = dynamic_cast<living_entity *> (ent);
	      if (target)
	        {
	          // make sure PvP is enabled in this area
	          bool pvp = wr->pvp;
	          {
	            block_pos tpos = target->pos;
	            std::vector<zone *> zones;
						  wr->get_zones ().find (tpos.x, tpos.y, tpos.z, zones);
						  for (zone *zn : zones)
						    {
						      if (!zn->pvp_enabled ())
						        {
						          block_pos mypos = pl->pos;
						          if (zn->get_selection ()->contains (mypos.x, mypos.y, mypos.z))
						            pl->message ("§cPvP is disabled here.");
						          else
						            pl->message ("§cPvP is disabled there.");
						          pvp = false;
						          break;
						        }
						      else
						        pvp = true;
						    }
	          }
	          if (!pvp)
	            return 0;
						
	          if (target->get_type () == ET_PLAYER)
	            {
	              player *tpl = static_cast<player *> (target);
	              if (tpl == pl)
	                return 0; // is this possible?
	              if (tpl->gamemode () == GT_CREATIVE)
	                return 0;
	            }
	              
            // limit attack speed
            int elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (
			        std::chrono::steady_clock::now () - pl->last_hit).count ();
		        if (elapsed < 333)
			        return 0;
		        pl->last_hit = std::chrono::steady_clock::now ();
            
            if (pl->pos.distance_from (target->pos) > 4.0)
              return 0; // too far away
            
            target->set_hearts (target->get_hearts () - _calc_hit_damage (pl));
            if (target->get_type () != ET_PLAYER)
              {
                // hurt animation
                wr->get_players ().send_to_all_visible (
					        packets::play::make_entity_status (target->get_eid (), 2), target);
              }
            wr->get_players ().send_to_all_visible (
              packets::play::make_sound_effect ("damage.hit",
			          target->pos.x, target->pos.y, target->pos.z, 1.0f, 63), target);
            
            // set player flying in the opposite direction
            double ux = -std::cos (pl->pos.l * 0.017453293) * std::sin (pl->pos.r * 0.017453293);
		        double uy = /*-std::sin (pl->pos.l * 0.017453293)*/ 0.75;
		        double uz =  std::cos (pl->pos.l * 0.017453293) * std::cos (pl->pos.r * 0.017453293);
		        int speed = 2500;
		        if (target->get_type () == ET_PLAYER)
		          {
		            player *tpl = static_cast<player *> (target);
		            tpl->send (packets::play::make_entity_velocity (tpl->eid,
			            ux * speed, uy * speed, uz * speed));
		          }
		        else
		          target->velocity = vector3 (ux * 0.3, uy, uz * 0.3);
	        }
	    }
	  
	  return 0;
	}
	
	
	int
	player::handle_pl_packet_03 (player *pl, packet_reader reader)
	{
		if (pl->is_dead ()) return 0;
		if (pl->joining_world) return 0;
		
		if (pl->rej_mov > 0)
			{
				pl->send (packets::play::make_player_pos_and_look (
					pl->pos.x, pl->pos.y, pl->pos.z, pl->pos.r, pl->pos.l, true));
				-- pl->rej_mov;
				return 0;
			}
		
		bool on_ground;
		
		on_ground = reader.read_byte ();
		
		entity_pos curr_pos = pl->pos;
		entity_pos new_pos {curr_pos.x, curr_pos.y, curr_pos.z, curr_pos.r,
			curr_pos.l, on_ground};
		pl->move_to (new_pos);
		
		return 0;
	}
	
	int
	player::handle_pl_packet_04 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		if (pl->joining_world) return 0;
		
		if (pl->rej_mov > 0)
			{
				pl->send (packets::play::make_player_pos_and_look (
					pl->pos.x, pl->pos.y, pl->pos.z, pl->pos.r, pl->pos.l, true));
				-- pl->rej_mov;
				return 0;
			}
		
		double x, y, z;
		bool on_ground;
		
		x = reader.read_double ();
		y = reader.read_double ();
		reader.read_double (); // stance
		z = reader.read_double ();
		on_ground = reader.read_byte ();
		
		
		entity_pos curr_pos = pl->pos;
		entity_pos new_pos {x, y, z, curr_pos.r, curr_pos.l, on_ground};
		pl->move_to (new_pos);
		
		return 0;
	}
	
	int
	player::handle_pl_packet_05 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		if (pl->joining_world) return 0;
		
		if (pl->rej_mov > 0)
			{
				pl->send (packets::play::make_player_pos_and_look (
					pl->pos.x, pl->pos.y, pl->pos.z, pl->pos.r, pl->pos.l, true));
				-- pl->rej_mov;
				return 0;
			}
		
		float r, l;
		bool on_ground;
		
		r = reader.read_float ();
		l = reader.read_float ();
		on_ground = reader.read_byte ();
		
		entity_pos curr_pos = pl->pos;
		entity_pos new_pos {curr_pos.x, curr_pos.y, curr_pos.z, r, l, on_ground};
		pl->move_to (new_pos);
		
		return 0;
	}
	
	int
	player::handle_pl_packet_06 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		if (pl->joining_world) return 0;
		
		if (pl->rej_mov > 0)
			{
				pl->send (packets::play::make_player_pos_and_look (
					pl->pos.x, pl->pos.y, pl->pos.z, pl->pos.r, pl->pos.l, true));
				-- pl->rej_mov;
				return 0;
			}
		
		double x, y, z;
		float r, l;
		bool on_ground;
		
		x = reader.read_double ();
		y = reader.read_double ();
		reader.read_double (); // stance
		z = reader.read_double ();
		r = reader.read_float ();
		l = reader.read_float ();
		on_ground = reader.read_byte ();
		
		entity_pos new_pos {x, y, z, r, l, on_ground};
		pl->move_to (new_pos);
		
		return 0;
	}
	
	int
	player::handle_pl_packet_07 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		char status;
		int x;
		unsigned char y;
		int z;
		unsigned char face;
		
		status = reader.read_byte (); // status
		x = reader.read_int ();
		y = reader.read_byte ();
		z = reader.read_int ();
		face = reader.read_byte (); // face
		
		world& w = *pl->get_world ();
		
		int w_width = w.get_width ();
		int w_depth = w.get_depth ();
		if (((w_width > 0) && ((x >= w_width) || (x < 0))) ||
				((w_depth > 0) && ((z >= w_depth) || (z < 0))))
			{
				pl->send (packets::play::make_block_change (
					x, y, z,
					w.get_id (x, y, z),
					w.get_meta (x, y, z)));
				return 0;
			}
		
		if (pl->sb_exists (x, y, z))
			{
				// modifying a selection block
				pl->sb_send (x, y, z);
				return 0;
			}
		
		/* 
		 * Handle marking callbacks
		 */
		if ((pl->curr_gamemode == GT_CREATIVE) || (pl->curr_gamemode == GT_SURVIVAL && status == 2))
			if (pl->mark_block (x, y, z))
				return 0;
		
		block_data bd = w.get_block (x, y, z);
		if (status == 2 || (status == 0 && pl->curr_gamemode == GT_CREATIVE))
			{
				/* 
				 * Digging
				 */
				
				// make sure the player is allowed to build
				if (!pl->rnk.main ()->can_build)
					{
						pl->message ("§4 * §cYou are not allowed to break/place blocks§4.");
						pl->send_orig_block (x, y, z);
						return 0;
					}
				else if (!w.security ().can_build (pl))
					{
						pl->message ("§4 * §cYou are not allowed to build here§4.");
						pl->send_orig_block (x, y, z);
						return 0;
					}
				else
					{
						// check zones
						std::vector<zone *> zones;
						w.get_zones ().find (x, y, z, zones);
						bool can_build = true;
						for (zone *zn : zones)
							if (!zn->get_security ().can_build (pl))
								{
									can_build = false;
									break;
								}
				
						if (!can_build)
							{
								pl->message ("§4 * §cYou are not allowed to build in this zone§4.");
								pl->send_orig_block (x, y, z);
								return 0;
							}
					}
				
				// degrade tool
				if (pl->curr_gamemode != GT_CREATIVE)
					{
						slot_item& held = pl->held_item ();
						if (item_info::is_tool (held.id ()))
							{
								held.set_damage (held.damage () + 1);
							}
					}
				
				// remove sign
				if (bd.id == BT_SIGN_POST || bd.id == BT_WALL_SIGN)
					{
						chunk *ch = w.get_chunk_at (x, z);
						if (ch)
							ch->ly_signs.remove_sign (x, y, z);
					}
				
				++ pl->bl_destroyed;
				w.queue_update (x, y, z, 0, 0, 0, 0, nullptr, pl);
				if (pl->gamemode () == GT_SURVIVAL)
					{
						pl->increment_exhaustion (0.025);
						
						blocki drop = block_info::get_drop ({bd.id, bd.meta});
						if (drop.id != BT_AIR && drop.valid ())
							{
								e_pickup *pick = new e_pickup (pl->srv,
								slot_item (drop.id, drop.meta, 1));
								pick->pos.set_pos (x + 0.5, y + 0.5, z + 0.5);
								pick->velocity.y = 0.4;
								w.spawn_entity (pick);
							}
					}
			}
		else if (status == 5)
			{
				/* 
				 * Finished Eating / Shoot Arrow
				 */
				
				if (x != 0 || y != 0 || z != 0 || face != 0xFF)
					{
						pl->log (LT_WARNING) << "Invalid \"finished eating\" packet from player '"
							<< pl->get_username () << "'" << std::endl;
						return -1;
					}
				
				slot_item s = pl->inv.get (pl->held_slot);
				food_info finf = item_info::get_food_info (s.id ());
				if (finf.hunger == 0)
					{
						// TODO: handle arrows (if not a bow in hand, kick the player)
					}
				else
					{
						if (pl->eating && (std::chrono::steady_clock::now () - pl->eat_time)
							< std::chrono::seconds (2))
							pl->eating = false;
					}
			}
		
		return 0;
	}
	
	
	
	static void
	_open_workbench (player *pl)
	{
		pl->open_window (new workbench (pl->inv, window::next_id ()));
	}
	
	int
	player::handle_pl_packet_08 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		int x = reader.read_int ();
		int y = reader.read_byte ();
		int z = reader.read_int ();
		char direction = reader.read_byte ();
		slot_item item = reader.read_slot ();
		/*int cx =*/ reader.read_byte (); // cursor X
		int cy = reader.read_byte (); // cursor Y
		/*int cz =*/ reader.read_byte (); // cursor Z
		
		world *w = pl->get_world ();
		
		if (x == -1 && y == 255 && z == -1 && direction == -1)
			{
				slot_item s = pl->inv.get (pl->held_slot);
				food_info finf = item_info::get_food_info (s.id ());
				if (finf.hunger != 0)
					{
						// player started eating
						if (pl->hunger < 20)
							{
								pl->eat_time = std::chrono::steady_clock::now ();
								pl->eating = true;
								
								// eating animation
								w->get_players ().send_to_all_visible (
									packets::play::make_animation (pl->eid, 3), pl);
							}
					}
				else
					{
						// TODO: handle arrows
					}
				return 0;
			}
		
		/* 
		 * Handle special blocks (workbenches, chests, etc...)
		 */
		if (!pl->is_crouched ())
			{
				int prev_id = w->get_id (x, y, z);
				if (prev_id == BT_WORKBENCH)
					{
						_open_workbench (pl);
						return 0;
					}
			}
		
		item = pl->held_item ();
		if (!item.is_valid () || item.empty ())
			return 0;
		if (!item.is_block () && (item.id () != IT_SIGN))
			return 0;
		
		int nx = x, ny = y, nz = z;
		
		block_data bd = w->get_block (x, y, z);
		if (bd.id != BT_TALL_GRASS && !(bd.id == BT_SNOW_COVER && bd.meta == 0))
			{
				switch (direction)
					{
						case 0: -- ny; break;
						case 1: ++ ny; break;
						case 2: -- nz; break;
						case 3: ++ nz; break;
						case 4: -- nx; break;
						case 5: ++ nx; break;
					}
			}
		
		if (!item.implemented ())
			{	
				pl->send_orig_block (nx, ny, nz);
				return 0;
			}
		
		int w_width = w->get_width ();
		int w_depth = w->get_depth ();
		if (((w_width > 0) && ((nx >= w_width) || (nx < 0))) ||
				((w_depth > 0) && ((nz >= w_depth) || (nz < 0))))
			{
				pl->send_orig_block (nx, ny, nz);
				return 0;
			}
		
		/* 
		 * Handle marking callbacks
		 */
		if (pl->mark_block (nx, ny, nz))
			return 0;
		
		if (pl->sb_exists (nx, ny, nz))
			{
				// modifying a selection block
				pl->sb_send (nx, ny, nz);
				return 0;
			}
		
		if (!pl->rnk.main ()->can_build)
			{
				// player not allowed to build
				pl->message ("§4 * §cYou are not allowed to break/place blocks§4.");
				pl->send_orig_block (nx, ny, nz);
				return 0;
			}
		else if (!w->security ().can_build (pl))
			{
				pl->message ("§4 * §cYou are not allowed to build here§4.");
				pl->send_orig_block (nx, ny, nz);
				return 0;
			}
		else
			{
				// check zones
				std::vector<zone *> zones;
				w->get_zones ().find (nx, ny, nz, zones);
				bool can_build = true;
				for (zone *zn : zones)
					if (!zn->get_security ().can_build (pl))
						{
							can_build = false;
							break;
						}
				
				if (!can_build)
					{
						pl->message ("§4 * §cYou are not allowed to build in this zone§4.");
						pl->send_orig_block (nx, ny, nz);
						return 0;
					}
			}
		
		// some blocks must be handled differently.
		if (item.id () == IT_SIGN)
			_place_sign (pl, nx, ny, nz, direction);
		else if (item.id () == BT_SLAB || item.id () == BT_WOODEN_SLAB)
			_place_slab (pl, x, y, z, direction, item, cy);
		else if (_is_stairs_block (item.id ()))
			_place_stairs (pl, nx, ny, nz, direction, item, cy);
		else if (item.id () == BT_TRUNK)
			_place_trunk (pl, nx, ny, nz, direction, item);
	  else if (item.id () == BT_TORCH)
	    _place_torch (pl, nx, ny, nz, direction);
		else
			{
				w->queue_update (nx, ny, nz,
					item.id (), item.damage (), 0, 0, nullptr, pl);
				++ pl->bl_created;
			}
		
		if (pl->gamemode () != GT_CREATIVE)
			pl->inv.set (pl->held_slot, slot_item (item.id (), item.damage (),
				item.amount () - 1));
		
		return 0;
	}
	
	int
	player::handle_pl_packet_09 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		unsigned short index = reader.read_short ();
		if (index > 8)
			{
				pl->log (LT_WARNING) << "Invalid \"Held Item Change\" packet received "
					"from " << pl->get_username () << " (slot: " << index << ")" << std::endl;
				return -1;
			}
		
		pl->held_slot = index + 36;
		pl->get_world ()->get_players ().send_to_all_visible (
			packets::play::make_entity_equipment (pl->eid, 0, pl->inv.get (pl->held_slot)), pl);
		
		return 0;
	}
	
	int
	player::handle_pl_packet_0a (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		reader.read_int (); // entity ID
		char animation = reader.read_byte ();
		
		// DEBUG
		{
			std::vector<block_change_record> records;
			records.push_back({15, 70, 15, 20, 0});
			pl->send (packets::play::make_multi_block_change (500, 200, records));
		}
		
		int anim_code = -1;
		switch (animation)
			{
				// swing arm
				case 1: anim_code = 0; break;
			}
		
		if (anim_code != -1)
			pl->get_world ()->get_players ().send_to_all_visible (
				packets::play::make_animation (pl->eid, anim_code), pl);
		
		if (pl->curr_gamemode == GT_CREATIVE && pl->inv.get (pl->held_slot).id () == IT_FEATHER)
			{
				// TODO: check permissions
				
				double ux = -std::cos (pl->pos.l * 0.017453293) * std::sin (pl->pos.r * 0.017453293);
				double uy = -std::sin (pl->pos.l * 0.017453293);
				double uz =  std::cos (pl->pos.l * 0.017453293) * std::cos (pl->pos.r * 0.017453293);
				
				int speed = pl->is_crouched () ? 32767 : 13500;
				pl->send (packets::play::make_entity_velocity (pl->eid,
					ux * speed, uy * speed, uz * speed));
			}
		
		return 0;
	}
	
	int
	player::handle_pl_packet_0b (player *pl, packet_reader reader)
	{
		if (!pl->logged_in)
			{ return -1; }
		
		int eid = reader.read_int ();
		int action = reader.read_byte ();
		
		if (eid != pl->get_eid ())
			return -1;
		
		switch (action)
			{
				case 1:
					pl->get_world ()->get_players ().send_to_all_visible (
						packets::play::make_animation (pl->eid, 104), pl);
					pl->crouched = true;
					break;
					
				case 2:
					pl->get_world ()->get_players ().send_to_all_visible (
						packets::play::make_animation (pl->eid, 105), pl);
					pl->crouched = false;
					break;
				
				case 3: /* leave bed */ break;
				case 4: pl->sprinting = true; break;
				case 5: pl->sprinting = false; break;
				
				default: return -1;
			}
		
		return 0;
	}
	
	int
	player::handle_pl_packet_0d (player *pl, packet_reader reader)
	{
		unsigned char wid = reader.read_byte ();
		
		if (!pl->open_win && wid != 0)
			{
				pl->log (LT_WARNING) << "Player \"" << pl->get_username () << "\" sent "
					"an invalid \"Close Window\" packet (no window open)." << std::endl;
				return -1;
			}
			
		if (pl->open_win && pl->open_win->wid () != wid)
			{
				pl->log (LT_WARNING) << "Player \"" << pl->get_username () << "\" sent "
					"an invalid \"Close Window\" packet (mismatching windows)." << std::endl;
				return -1;
			}
		
		delete pl->open_win;
		pl->open_win = nullptr;
		
		return 0;
	}
	
	int
	player::handle_pl_packet_0e (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		unsigned char wid = reader.read_byte ();
		short slot = reader.read_short ();
		char button = reader.read_byte ();
		short action = reader.read_short ();
		char mode = reader.read_byte ();
		
		window *win = pl->open_win ? pl->open_win : &pl->inv;
		pl->log (LT_DEBUG) << "| Mode [" << (int)mode << "] Button [" << (int)button << "] Slot [" << slot << "]" << std::endl;
		
		bool craft_changed = false;
		if (win->type () == WT_INVENTORY)
			craft_changed = (slot >= 1 && slot <= 4);
		else if (win->type () == WT_WORKBENCH)
			craft_changed = (slot >= 1 && slot <= 9);
		
		switch (mode)
			{
				/* 
				 * MODE 0:
				 *   button 0 = left mouse click
				 *   button 1 = right mouse click
				 */
				case 0:
					if (slot < 0 || slot >= win->slot_count ())
						return 0;
					{
						slot_item& item = win->get (slot);
							
						if (button == 0)
							{
								// left mouse click
								if (pl->cursor_slot.empty ())
									{
										if (!item.empty ())
											{
												// pick up entire stack
												pl->log (LT_DEBUG) << "Picking up " << item.amount () << " of " << item.id () << " up from [" << slot << "]" << std::endl;
												pl->cursor_slot.set (item);
												item.clear ();
												
												if (slot == 0)
													pl->handle_crafting_take (wid);
											}
									}
								else
									{
										//if (!win->can_place_at (slot, item))
										//	return 0;
										
										if (slot == 0)
											{
												if (pl->cursor_slot.compatible_with (item))
													{
														int take = pl->cursor_slot.max_stack () - pl->cursor_slot.amount ();
														if (take > item.amount ())
															take = item.amount ();
														
														if (take > 0)
															{
																pl->cursor_slot.give (take);
																item.take (take);
																pl->handle_crafting_take (wid);
															}
														pl->log (LT_DEBUG) << "Taking " << take << " from crafting result slot" << std::endl;
													}
											}
										else if (pl->cursor_slot.compatible_with (item))
											{
												// put stack down
												int take = pl->cursor_slot.amount ();
												if (item.amount () + take > item.max_stack ())
													take = item.max_stack () - item.amount ();
												
												pl->log (LT_DEBUG) << "Putting " << take << " down [" << pl->cursor_slot.amount () << " left in hand]" << std::endl;
												pl->log (LT_DEBUG) << "   Empty? " << (item.empty ()) << std::endl;
												if (item.empty ())
													item.set (pl->cursor_slot);		
												else
													item.give (take);
												pl->cursor_slot.take (take);
												
											}
										else
											{
												// swap between held item and the one being clicked
												slot_item tmp = pl->cursor_slot;
												pl->cursor_slot = item;
												item = tmp;
												pl->log (LT_DEBUG) << "Swapping between [" << slot << "] and held item" << std::endl;
											}
									}
							}
						else if (button == 1)
							{
								// right mouse click
								if (slot == 0)
									{
										if (!item.empty ())
											{
												int take = pl->cursor_slot.max_stack () - pl->cursor_slot.amount ();
												if (take > item.amount ())
													take = item.amount ();
										
												if (take > 0)
													{
														pl->cursor_slot.give (take);
														item.take (take);
														pl->handle_crafting_take (wid);
													}
												pl->log (LT_DEBUG) << "Taking " << take << " from crafting result slot" << std::endl;
											}
									}
								else if (pl->cursor_slot.empty ())
									{
										if (!item.empty ())
											{
												// pick up half the stack
												int take = item.amount () / 2 + !!(item.amount () % 2);
												pl->cursor_slot.set (item);
												pl->cursor_slot.take (item.amount () - take);
												item.take (take);
												pl->log (LT_DEBUG) << "Halving: Picking up " << take << " [leaving " << item.amount () << "]" << std::endl;
											}
									}
								else
									{
										if (!win->can_place_at (slot, item))
											return 0;
										
										if (pl->cursor_slot.compatible_with (item))
											{
												// put one down
												if (item.amount () < item.max_stack ())
													{
														pl->log (LT_DEBUG) << "Putting one down [" << pl->cursor_slot.amount () << " left in hand]" << std::endl;
														pl->log (LT_DEBUG) << "   Empty? " << (item.empty ()) << std::endl;
														
														if (item.empty ())
															{
																item.set (pl->cursor_slot);
																item.set_amount (1);
															}
														else
															item.give (1);
														pl->cursor_slot.take (1);
													}
											}
									}
							}
						else
							return false;
						break;
					}
				
				/* 
				 * MODE 1:
				 *   button 0 = shift + left mouse click
				 *   button 1 = shift + right mouse click
				 */
				case 1:
					if (slot < 0 || slot >= win->slot_count ())
						return 0;
					{
						slot_item *item = &win->get (slot);
						if (slot == 0)
							{
								item = _craft_all_inv (pl->inv, pl->srv.get_crafting_manager ());
								if (!item)
									break;
							}
						else if (item->empty ())
							break;
						int item_amount = item->amount ();
						
						auto range = win->shift_range (slot);
						if (range.first == -1 || range.second == -1)
							{
								if (slot == 0)
									delete item;
								break;
							}
							
						pl->log (LT_DEBUG) << "Shifting [" << slot << " of type " << item->id () << "] to [" << range.first << " -> " << range.second << "]" << std::endl;
						
						// we perform two passes - try items with matching IDs first, and
						// then finally try empty slots too.
						bool first_pass = true;
						for (int j = 0; j < 2; ++j)
							{
								int i, a = (range.first < range.second) ? 1 : -1;
								for (i = range.first; i != (range.second + a) && !item->empty (); i += a)
									{
										slot_item& sl = win->get (i);
										if (first_pass ? (item->id () == sl.id ()) : sl.compatible_with (*item))
											{
												//pl->log (LT_DEBUG) << "Trying [" << i << "]: yes" << std::endl;
												int take = item->amount ();
												if (sl.amount () + take > sl.max_stack ())
													take = sl.max_stack () - sl.amount ();
												
												int dbg_then = sl.amount ();
												if (sl.empty ())
													{
														sl.set (*item);
														sl.set_amount (take);
													}	
												else
													sl.give (take);
												item->take (take);
												
												pl->log (LT_DEBUG) << "  Adding " << take << " to [" << i << "] [of type: " << sl.id () << "] [then: " << dbg_then << "] [now: " << sl.amount () << "]" << std::endl;
											}
										//else
										//	pl->log (LT_DEBUG) << "Trying [" << i << "]: nope (" << sl.id () << ") (our: " << item.id () <<  << std::endl;
									} 
								
								first_pass = false;
							}
						
						pl->log (LT_DEBUG) << "  Done, " << item->amount () << " left" << std::endl;
						if (slot == 0)
							{
								for (int j = 1; j <= 4; ++j)
									pl->inv.get (j).take (item_amount);
								delete item;
								
								pl->handle_crafting (wid);
							}
						break;
					}
				
				/* 
				 * MODE 2:
				 *   button 0 = Number key 1
				 *   ...
				 *   button 8 = Number key 9
				 */
				case 2:
					if (slot < 0 || slot >= win->slot_count ())
						return 0;
					{
						if (button < 0 || button > 8)
							{
								pl->log (LT_WARNING) << "Invalid 0x66 packet (click window) received from \"" << pl->get_username () << "\"" << std::endl;
								return -1;
							}
						
						auto hotbar_range = win->hotbar_range ();
						if (hotbar_range.first == -1 || hotbar_range.second == -1 ||
							((hotbar_range.second - hotbar_range.first) != 8))
							break;
						
						int dest_pos = hotbar_range.first + button;
						if (dest_pos >= win->slot_count ())
							break;
						
						// swap
						slot_item& item = win->get (slot);
						slot_item tmp = win->get (dest_pos);
						win->set (dest_pos, item, false);
						win->set (slot, tmp, false);
						pl->log (LT_DEBUG) << "Swapped between [" << slot << "] and [" << dest_pos << "]" << std::endl;
						break;
					}
				
				/* 
				 * MODE 3
				 *   button 2 = middle click
				 */
				case 3:
					if (button == 2)
						; // does nothing
					break;
				
				/* 
				 * MODE 4
				 *   button 0, slot not -999 = drop key
				 *   button 1, slot not -999 = ctrl + drop key
				 *   button 0, slot -999 = left click outside inventory holding nothing
				 *   button 1, slot -999 = right click outside inventory holding nothing
				 */
				case 4:
					if (slot != -999)
						{
							// TODO: handle dropping items
						}
					break;
				
				/* 
				 * MODE 5
				 *   button 0, slot -999 = starting left/middle mouse paint
				 *   button 4, slot -999 = starting right mouse paint
				 *   button 1, slot not -999 = left mouse painting progress
				 *   button 5, slot not -999 = right mouse painting progress
				 *   button 2, slot -999 = ending left mouse paint
				 *   button 6, slot -999 = ending right mouse paint
				 */
				case 5:
					craft_changed = false;
					switch (button)
						{
							// start left/middle mouse paint
							case 0:
								if (pl->inv_painting)
									return -1; // TODO: reset
								pl->inv_mb = 0;
								pl->inv_paint_slots.clear ();
								pl->inv_painting = true;
								break;
							
							// start right mouse paint
							case 4:
								if (pl->inv_painting)
									return -1; // TODO: reset
								pl->inv_mb = 1;
								pl->inv_paint_slots.clear ();
								pl->inv_painting = true;
								break;
							
							// left mouse painting progress
							case 1:
								if (!pl->inv_painting || pl->inv_mb != 0)
									return -1;
								if (slot < 0 || slot >= win->slot_count ())
									{
										pl->log (LT_WARNING) << "Player \"" << pl->get_username ()
											<< "\" tried to use a slot outside of window." << std::endl;
										return -1;
									}
								if (!win->can_place_at (slot, pl->cursor_slot))
									return 0;
								pl->inv_paint_slots.push_back (slot);
								break;
							
							// right mouse paiting progress
							case 5:
								if (!pl->inv_painting || pl->inv_mb != 1)
									return -1;
								if (slot < 0 || slot >= win->slot_count ())
									{
										pl->log (LT_WARNING) << "Player \"" << pl->get_username ()
											<< "\" tried to use a slot outside of window." << std::endl;
										return -1;
									}
								if (!win->can_place_at (slot, pl->cursor_slot))
									return 0;
								pl->inv_paint_slots.push_back (slot);
								break;
							
							// end painting
							case 2:
							case 6:
								if (!pl->inv_painting)
									return -1;
								pl->inv_painting = false;
								
								for (short s : pl->inv_paint_slots)
									if ((!pl->open_win && (s >= 1 && s <= 4)) || 
											(pl->open_win && pl->open_win->type () == WT_WORKBENCH && (s >= 1 && s <= 9)))
										{
											craft_changed = true;
											break;
										}
								
								pl->log (LT_DEBUG) << "End inventory paint [mouse: " << ((pl->inv_mb == 0) ? "left" : "right") << "]" << std::endl;
								if (!pl->inv_paint_slots.empty ())
									{
										int give = (pl->inv_mb == 1) ? 1
											: (pl->cursor_slot.amount () / pl->inv_paint_slots.size ());
										for (short s : pl->inv_paint_slots)
											{
												if (pl->cursor_slot.amount () < give)
													return 0;
												
												slot_item& prev = win->get (s);
												if (prev.compatible_with (pl->cursor_slot))
													{
														if (pl->cursor_slot.amount () < give)
															return 0;
														
														short this_give = give;
														if ((prev.id () != BT_AIR) && ((prev.amount () + this_give) > prev.max_stack ()))
															this_give = prev.max_stack () - prev.amount ();
														
														int p_amount = prev.amount ();
														if (prev.empty ())
															prev.set (pl->cursor_slot);
														prev.set_amount (p_amount + this_give);
														pl->cursor_slot.set_amount (pl->cursor_slot.amount () - this_give);
														
														pl->log (LT_DEBUG) << "  " << this_give << " added to [" << s << "]" << std::endl;
													}
											}
									}
								pl->log (LT_DEBUG) << "  " << pl->cursor_slot.amount () << " left in hand" << std::endl;
								break;
						}
					break;
					
				/* 
				 * MODE 6
				 *   button 0 = double click
				 */
				case 6:
					if (slot < 0 || slot >= win->slot_count ())
						return 0;
					
					if (button == 0)
						{
							if (pl->cursor_slot.empty ())
								break;
							
							int scount = win->slot_count ();
							for (int i = 0; i < scount && !pl->cursor_slot.full (); ++i)
								{
									slot_item& sl = win->get (i);
									if (sl.compatible_with (pl->cursor_slot))
										{
											int take = pl->cursor_slot.max_stack () - pl->cursor_slot.amount ();
											if (take > sl.amount ())
												take = sl.amount ();
											
											pl->cursor_slot.give (take);
											sl.take (take);
										}
								}
							
							pl->log (LT_DEBUG) << "Double click [collected " << pl->cursor_slot.amount () << " items]" << std::endl;
						}
					break;
			}
		
		if (craft_changed)
			pl->handle_crafting (wid);
		
		return 0;
	}
	
	int
	player::handle_pl_packet_10 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		if (pl->gamemode () != GT_CREATIVE)
			{
				pl->log (LT_WARNING) << "Received a \"Creative Inventory Action\" packet "
					"from a player in survival mode. (" << pl->get_username () << ")" << std::endl;
				return -1;
			}
		
		short index = reader.read_short ();
		slot_item item = reader.read_slot ();
		
		if (index < 0)
			{
				// TODO: drop item
				return 0;
			}
		
		if (index > 44)
			{
				// shouldn't be possible
				pl->log (LT_WARNING) << "Received an invalid \"Creative Inventory Action\" "
					"packet from " << pl->get_username () << " (attempt to fetch item into an "
					"illegal slot)." << std::endl;
				return -1;
			}
		
		if (!item.implemented () && (short)item.id () != -1)
			pl->inv.set (index, {BT_AIR}, true);
		else
			pl->inv.set (index, item, false);
		return 0;
	}
	
	int
	player::handle_pl_packet_12 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		int x = reader.read_int ();
		int y = reader.read_short ();
		int z = reader.read_int ();
		
		world *wr = pl->get_world ();
		int id = wr->get_id (x, y, z);
		if (id != BT_SIGN_POST && id != BT_WALL_SIGN)
			{
				pl->log (LT_WARNING) << "Player \"" << pl->get_username () << "\" tried to change a non-existent sign." << std::endl;
				return -1;
			}
		
		if (!pl->rnk.main ()->can_build || !wr->security ().can_build (pl))
			{
				pl->log (LT_WARNING) << "Player \"" << pl->get_username () << "\" tried to change sign without permission!" << std::endl;
				return -1;
			}
		
		chunk *ch = wr->get_chunk_at (x, z);
		if (!ch)
			return -1;
		
		std::vector<std::string> lines;
		for (int i = 0; i < 4; ++i)
			{
				char buf[16];
				if (reader.read_string (buf, 16) == -1)
					{
						pl->log (LT_WARNING) << "Player \"" << pl->get_username ()
							<< "\" sent invalid update sign packet (0x82), line(s) too long!"
							<< std::endl;
						return -1;
					}
				
				// make sure the string doesn't contain any funky characters
				// TODO
				
				// color sign
				if (pl->has ("place.sign.colors"))
					lines.push_back (sutils::convert_colors (buf));
				else
					lines.push_back (buf);
			} 
		
		{
			std::lock_guard<std::mutex> guard {ch->ly_signs.lock};
			ch->ly_signs.insert_sign (x, y, z, lines[0].c_str (), lines[1].c_str (),
				lines[2].c_str (), lines[3].c_str ());
		}
		
		// update players
		pl->get_world ()->get_players ().all (
			[ch, &lines, x, y, z] (player *pl)
				{
					if (pl->can_see_chunk (x >> 4, z >> 4))
						{
							pl->send (packets::play::make_update_sign (x, y, z, lines[0].c_str (),
								lines[1].c_str (), lines[2].c_str (), lines[3].c_str ()));
						}
				});
		
		return 0;
	}
	
	int
	player::handle_pl_packet_16 (player *pl, packet_reader reader)
	{
		char payload = reader.read_byte ();
		if (payload == 0)
			{
				/* 
				 * Respawn
				 */
				
				pl->send (packets::play::make_respawn (0, 0, pl->curr_gamemode, "hCraft"));
				
				// revert health
				pl->hearts = pl->hunger = 20;
				pl->hunger_saturation = 5.0;
				
				entity_pos spawn_pos = pl->curr_world->get_spawn ();
				pl->last_ground_height = -128.0;
				pl->teleport_to (spawn_pos);
				
				pl->spawn_to_all ();
			}
		
		return 0;
	}
	
	
	
//--------
	
	/* 
	 * Executes the appropriate packet handler for the given byte array.
	 */
	int
	player::handle (const unsigned char *data)
	{
		static int (*status_handlers[0x100]) (player *, packet_reader) =
			{
				handle_st_packet_00, handle_st_packet_01
			};
		
		static int (*login_handlers[0x100]) (player *, packet_reader) =
			{
				handle_lg_packet_00, handle_lg_packet_01,
			};
		
		static int (*play_handlers[0x100]) (player *, packet_reader) =
			{
				handle_pl_packet_00, handle_pl_packet_01, handle_pl_packet_02, handle_pl_packet_03, // 0x03
				handle_pl_packet_04, handle_pl_packet_05, handle_pl_packet_06, handle_pl_packet_07, // 0x07
				handle_pl_packet_08, handle_pl_packet_09, handle_pl_packet_0a, handle_pl_packet_0b, // 0x0B
				handle_packet_xx, handle_pl_packet_0d, handle_pl_packet_0e, handle_packet_xx, // 0x0F
				
				handle_pl_packet_10, handle_packet_xx, handle_pl_packet_12, handle_packet_xx, // 0x13
				handle_packet_xx, handle_packet_xx, handle_pl_packet_16, handle_packet_xx, // 0x17
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x1B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x1F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x23
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x27
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x2B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x2F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x33
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x37
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x3B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x3F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x43
			};
		
		if (this->bad ()) return 0;
		
		packet_reader reader {data};
		reader.read_varint (); // skip packet length
		unsigned char opcode = reader.read_varint ();
		
		switch (this->pstate)
			{
				case PS_HANDSHAKE:
					if (opcode != 0)
						{
							this->log (LT_WARNING) << "Did not receive handshake packet from player '" << this->get_username () << "'" << std::endl;
							return -1;
						}
					return handle_hs_packet_00 (this, reader);
				
				case PS_STATUS:
					if (opcode > 0x01)
						{
							this->log (LT_WARNING) << "Got invalid packet from player '" << this->get_username () << "' [status]" << std::endl;
							return -1;
						}
					return status_handlers[opcode] (this, reader);
				
				case PS_LOGIN:
					if (opcode > 0x01)
						{
							this->log (LT_WARNING) << "Got invalid packet from player '" << this->get_username () << "' [login]" << std::endl;
							return -1;
						}
					return login_handlers[opcode] (this, reader);
				
				case PS_PLAY:
					if (opcode > 0x40)
						{
							this->log (LT_WARNING) << "Got invalid packet from player '" << this->get_username () << "' [play]" << std::endl;
							return -1;
						}
					return play_handlers[opcode] (this, reader);
				
				default:
					return -1;
			}
	}



//----
	
	static bool
	test_packet_00 (packet_reader reader, int length,
		std::deque<unsigned char *>& equeue)
	{
		/* 
		 * When a client connects, we expect two packets - the handshake packet
		 * (opcode: 0x00), followed by a request/login start packet
		 * (dpending on whether the client is trying to login, or check the server's
		 * status.
		 */
		
		return !equeue.empty ();
	}
	
	static bool
	test_packet_0e (packet_reader reader, int length,
		std::deque<unsigned char *>& equeue)
	{
		reader.read_byte (); // wid
		short slot = reader.read_short ();
		char mbtn  = reader.read_byte ();
		reader.read_short (); // action number
		char mode = reader.read_byte ();
		
		if (mode == 5)
			{
				if (slot == -999 && ((mbtn == 2) || (mbtn == 6)))
					return true;
				
				return false;
			}			
		
		return true;
	}
	
	/* 
	 * Examines the queue that holds packets pending to be handled by
	 * appropriate callback methods in-order to conclude whether it is safe
	 * to handle the packets. 
	 */
	bool
	player::test_packet_chain ()
	{
		// if the last packet is different compared to the previous ones,
		// then consider it as the end of chain.
		if (this->exec_queue.size () > 1)
			{
				int last_op = -1;
				for (auto itr = this->exec_queue.rbegin (); itr != this->exec_queue.rend (); ++itr)
					{
						packet_reader reader {*itr};
						reader.read_varint (); // skip length
						
						if (last_op == -1)
							last_op = reader.read_varint ();
						else
							{
								if (last_op != (int)reader.read_varint ())
									return true;
							}
					}
			} 
		
		// we only test the last packet
		unsigned char *data = this->exec_queue.back ();
		this->exec_queue.pop_back ();
		
		packet_reader reader {data};
		int length = reader.read_varint ();
		
		bool ret = true;
		switch (reader.read_varint ())
			{
				case 0x0E: ret = test_packet_0e (reader, length, this->exec_queue); break;
				case 0x10: ret = false; break;
				
				case 0x00: ret = test_packet_00 (reader, length, this->exec_queue); break;
			}
		
		this->exec_queue.push_back (data);
		return ret;
	}
}

