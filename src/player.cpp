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

#include "player.hpp"
#include "server.hpp"
#include "stringutils.hpp"
#include "wordwrap.hpp"
#include "commands/command.hpp"
#include "utils.hpp"
#include "entities/pickup.hpp"

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
		
		this->logged_in = false;
		this->fail = false;
		this->kicked = false;
		this->handshake = false;
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
		
		this->last_tick = std::chrono::steady_clock::now ();
		this->last_heart_regen = this->last_tick;
		this->heal_delay = std::chrono::milliseconds (4000);
		this->tick_counter = 0;
		
		this->curr_gamemode = GT_SURVIVAL;
		this->curr_world = nullptr;
		this->joining_world = false;
		this->chcurr = chunk_pos (0, 0);
		this->ping_waiting = false;
		this->sb_block.set (BT_STILL_WATER);
		this->need_new_chunks = false;
		this->streaming_chunks = false;
		
		this->curr_sel = nullptr;
		this->last_ping = std::chrono::system_clock::now ();
		this->keep_alives_received = 0;
		
		this->last_portal_use = std::chrono::steady_clock::now ();
		
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
		if (pl->bad () || pl->srv.is_shutting_down ())return;
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
				
				/*
				// DEBUG
				if (pl->total_read == 1)
					{
						pl->log (LT_DEBUG) << "Packet [" << (int)pl->rdbuf[0] << "]" << std::endl;
					}
				*/
				
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
				
				// a small check...
				if (!pl->handshake && (pl->total_read == 1))
					{
						if (pl->rdbuf[0] != 0x02 && pl->rdbuf[0] != 0xFE)
							{
								if (pl->rdbuf[0] == 0xFA)
									{
										// plugin messages...
										// TODO
										
									}
								else
									{
										pl->log (LT_WARNING) << "Expected handshake from @" << pl->get_ip () << " (got " << (int)pl->rdbuf[0] << ")" << std::endl;
										pl->reading = false; 
										pl->disconnect ();
										return;
									}
							}
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
				opcode = pack->data[0];
				delete pack;
				
				if (pl->kicked && (opcode == 0xFF))
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
				
						if (this->handshake && !silent)
							{
								// leave message
								this->get_server ().get_players ().message (
									messages::compile (this->get_server ().msgs.server_leave, messages::environment (this)));;
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
		
		// dispose of selection
		for (auto itr =  std::begin (this->selections);
		 				 	itr != std::end (this->selections);
		 				 	++itr)
		 	delete itr->second;
		this->selections.clear ();
		this->curr_sel = nullptr;
		
		// and finally
		//if (this->logged_in)
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
		
		this->kicked = true;
		if (sanitize)
			this->send (packet::make_kick (msg));
		else if (msg[0] == '\0')
			{
				// server list ping responses are identified by an empty kick
				// message and @{sanitize} being false.
				
				std::ostringstream ss;
				auto& cfg = this->get_server ().get_config ();
				auto& players = this->get_server ().get_players ();
				
				this->send (packet::make_ping_kick (cfg.srv_motd, players.count (),
					cfg.max_players));
			}
	}
	
	
	
	// returns true if the packet can be disposed of, without being sent.
	static bool
	_redundancy_test (player *pl, packet *pack)
	{
		packet_reader reader {pack->data};
		
		switch (reader.read_byte ())
			{
				// multi block change
				case 0x34:
					{
						int cx = reader.read_int ();
						int cz = reader.read_int ();
						if (!pl->can_see_chunk (cx, cz))
							return true;
					}
					break;
				
				// block change
				case 0x35:
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
	player::join_world (world* w, bool broadcast)
	{
		this->join_world_at (w, w->get_spawn (), broadcast);
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
		msg = messages::compile (pl->get_server ().msgs.world_join_self, env);
		pl->message (msg);
	
		// new players
		msg = messages::compile (pl->get_server ().msgs.world_enter, env);
		for (player *p : new_players)
			p->message (msg);
	
		// old players
		msg = messages::compile (pl->get_server ().msgs.world_depart, env);
		for (player *p : old_players)
			p->message (msg);
	
		// others
		msg = messages::compile (pl->get_server ().msgs.world_join, env);
		for (player *p : others)
			p->message (msg);
	}
	
	/* 
	 * Sends the player to the given world at the specified location.
	 */
	void
	player::join_world_at (world *w, entity_pos destpos, bool broadcast)
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
				this->send (packet::make_player_pos_and_look (
					epos.x, epos.y, epos.z, epos.y + 1.65, epos.r, epos.l, true));
			}
		
		this->curr_world = w;
		this->pos = destpos;
		this->curr_world->get_players ().add (this);
		this->load_tab_list ();
		this->add_to_tab_list (false);
		this->last_ground_height = -128.0;
		
		block_pos bpos = destpos;
		this->send (packet::make_spawn_pos (bpos.x, bpos.y, bpos.z));
		
		this->need_new_chunks = true;
		
		// start physics
		this->srv.global_physics.queue_physics (w, this->eid);
		
		log () << this->get_username () << " joined world \"" << w->get_name () << "\"" << std::endl;
		if (had_prev_world)
			_broadcast_join_message (this, w, prev_world);
	}
	
	/* 
	 * Reloads the map for the player.
	 */
	void
	player::rejoin_world ()
	{
		std::lock_guard<std::mutex> wguard {this->world_lock};
		
		for (auto itr = this->known_chunks.begin (); itr != this->known_chunks.end (); ++ itr)
			{
				known_chunk kc = *itr;
				
				this->send (packet::make_empty_chunk (kc.cx, kc.cz));
				
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
						
						this->send (packet::make_chunk (resp.cx, resp.cz, resp.ch));
						this->known_chunks.push_back ({w, resp.cx, resp.cz});
						
						// is this our new home chunk? (When switching between worlds)
						if (this->joining_world && (my_cpos.x == resp.cx && my_cpos.z == resp.cz))
							{
								entity_pos epos = this->pos;
								this->send (packet::make_player_pos_and_look (
									epos.x, epos.y, epos.z, epos.y + 1.65, epos.r, epos.l, true));
								this->rej_mov = 3; // reject the next 3 movement packets
								
								this->update_home_chunk ();
								
								this->joining_world = false;
							}
					
						// selection blocks / editstages
						this->sb_updates.preview_chunk_to (this, resp.cx, resp.cz, false);
						for (edit_stage *es : this->edstages)
							if (es->get_world () == w)
								es->preview_chunk_to (this, resp.cx, resp.cz, true);
						
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
					}
			}
		
		// unload chunks
		for (auto p : unload_list)
			{
				known_chunk kc = p.first;
				bool full_unload = p.second;
				
				if (full_unload)
					this->send (packet::make_empty_chunk (kc.cx, kc.cz));
				
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
		this->send (packet::make_block_change (x, y, z, bd.id, bd.meta));
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
								else if (!this->has_access (w->get_join_perms ()))
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
						this->send (packet::make_player_pos_and_look (
							dest.x, dest.y, dest.z, dest.y + 1.65, dest.r, dest.l, dest.on_ground));
					}
			}
		
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
								pl->send (packet::make_entity_look (this->get_eid (), dest.r, dest.l));
								pl->send (packet::make_entity_head_look (this->get_eid (), dest.r));
							}
					}
			}
		else
			{
				// position has changed.
				
				std::lock_guard<std::mutex> guard {this->visible_player_lock};
				for (player *pl : this->visible_players)
					{
						pl->send (packet::make_entity_teleport (this->get_eid (),
							std::round (dest.x * 32.0), std::round (dest.y * 32.0),
							std::round (dest.z * 32.0), dest.r, dest.l));
						pl->send (packet::make_entity_head_look (this->get_eid (), dest.r));
					}
			}
		
		this->handle_falls_and_jumps (prev_pos.on_ground, this->pos.on_ground, prev_pos);
		this->handle_portals ();
	}
	
	/* 
	 * Teleports the player to the given position.
	 */
	void
	player::teleport_to (entity_pos dest)
	{
		this->send (packet::make_player_pos_and_look (
			dest.x, dest.y, dest.z, dest.y + 1.65, dest.r, dest.l, dest.on_ground));
		this->move_to (dest);
		this->send (packet::make_player_pos_and_look (
			dest.x, dest.y, dest.z, dest.y + 1.65, dest.r, dest.l, dest.on_ground));
	}
	
	/* 
	 * Checks whether this player can be seen by player @{pl}.
	 */
	bool
	player::visible_to (player *pl)
	{
		chunk_pos me_pos = this->pos;
		chunk_pos pl_pos = pl->pos;
		
		return (
			(utils::iabs (me_pos.x - pl_pos.x) <= player::chunk_radius ()) &&
			(utils::iabs (me_pos.z - pl_pos.z) <= player::chunk_radius ()));
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
		this->send (packet::make_ping (this->ping_id));
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
		col_name.append ("§");
		col_name.append (this->get_colored_username ());
		
		entity_pos me_pos = this->pos;
		entity_metadata me_meta;
		this->build_metadata (me_meta);
		pl->send (packet::make_spawn_named_entity (
			this->get_eid (), col_name.c_str (),
			me_pos.x, me_pos.y, me_pos.z, me_pos.r, me_pos.l, 0, me_meta));
		pl->send (packet::make_entity_head_look (this->get_eid (), me_pos.r));
		pl->send (packet::make_entity_equipment (this->eid, 0, this->inv.get (this->held_slot)));
		
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
						pl->send (packet::make_player_list_item (me->get_colored_username (), true, me->ping_time_ms));
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
						pl->send (packet::make_player_list_item (me->get_colored_username (), false, me->ping_time_ms));
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
					me->send (packet::make_player_list_item (pl->get_colored_username (), false, me->ping_time_ms));
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
					me->send (packet::make_player_list_item (pl->get_colored_username (), true, me->ping_time_ms));
				});
	}
	
	
	
//----
	
	/* 
	 * Sends the given message to the player.
	 */
	
	void
	player::message (const char *msg)
	{
		this->send (packet::make_message (msg));
	}
	
	void
	player::message (const std::string& msg)
	{
		this->send (packet::make_message (msg.c_str ()));
	}
	
	void
	player::message_wrapped (const char *msg, const char *prefix, bool first_line)
	{
		std::vector<std::string> lines;
		
		wordwrap::wrap_prefix (lines, msg, 64, prefix, first_line);
		for (auto& line : lines)
			{
				this->send (packet::make_message (line.c_str ()));
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
				this->send (packet::make_message (line.c_str ()));
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
	
	/* 
	 * Syntax:
	 *   term1|term2|term3|...|termN
	 * 
	 * Where term is one of:
	 *   <group name>   : e.g. moderator or builder
	 *   ^<player name> : e.g. ^BizarreCake
	 *   *<permission>  : e.g. *command.admin.say
	 */
	bool 
	player::has_access (const std::string& access_str)
	{
		if (access_str.empty ())
			return true;
		
		enum term_type
			{
				TT_GROUP,
				TT_ATLEAST_GROUP,
				TT_PLAYER,
				TT_PERM
			};
		
		const char *str = access_str.c_str ();
		const char *ptr = str;
		while (*ptr)
			{
				term_type typ = TT_GROUP;
				std::string word;
				int neg = 0;
				
				while (*ptr && (*ptr != '|'))
					{
						int c = *ptr;
						switch (c)
							{
								case '!':
									++ neg;
									if (neg > 1)
										return false;
									break;
								
								case '^':
									if (typ == TT_PLAYER)
										return false;
									typ = TT_PLAYER;
									break;
								
								case '*':
									if (typ == TT_PERM)
										return false;
									typ = TT_PERM;
									break;
								
								case '>':
									if (typ == TT_ATLEAST_GROUP)
										return false;
									typ = TT_ATLEAST_GROUP;
									break;
								
								default:
									word.push_back (c);
							}
						++ ptr;
					}
				
				if (*ptr == '|')
					++ ptr;
				
				bool res = true;
				switch (typ)
					{
						case TT_GROUP:
							{
								group *grp = this->srv.get_groups ().find (word.c_str ());
								if (!grp)
									res = false;
								else if (!this->get_rank ().contains (grp))
									res = false;
							}
							break;
						
						case TT_ATLEAST_GROUP:
							{
								group *grp = this->srv.get_groups ().find (word.c_str ());
								if (grp && (this->get_rank ().power () < grp->power))
									res = false;
							}
							break;
						
						case TT_PLAYER:
							res = sutils::iequals (word, this->username);
							break;
						
						case TT_PERM:
							res = this->has (word.c_str ());
							break;
					}
				
				if (neg == 1)
					res = !res;
				if (res)
					return true;
			}
		
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
							std::cout << "!found_player, dbid: " << this->dbid << std::endl;
						}
				}
			catch (const std::exception& ex)
				{
					log (LT_ERROR) << "Failed to save player data! (\"" << this->username << ") [" << ex.what () << "]" << std::endl;
					this->message ("§c * §4Failed to save player data§c.");
				}
			
			// load previous position
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
				this->curr_world = this->srv.get_worlds ().find (world.c_str ());
				if (this->curr_world)
					{
						this->pos = last_pos;
						this->curr_gamemode = (gm == 1) ? GT_CREATIVE : GT_SURVIVAL;
					}
			}
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
		if (!this->handshake)
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
				this->send (packet::make_block_change (
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
		this->send (packet::make_block_change (x, y, z, this->sb_block.id,
			this->sb_block.meta));
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
		this->send (packet::make_change_game_state (3, (gm == GT_CREATIVE) ? 1 : 0));
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
		this->send (packet::make_update_health (p_hearts, p_hunger, hunger_saturation));
		if (hurt)
			{
				this->send (packet::make_entity_status (this->eid, 2));
				
				// notify others too
				this->get_world ()->get_players ().send_to_all_visible (
					packet::make_entity_status (this->eid, 2), this);
				
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
		else if (this->hunger <= 0)
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
						this->send (packet::make_entity_status (this->eid, 9));
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
		
		this->last_tick = now;
		++ tick_counter;
		return false;
	}
	
	
	
	bool
	player::handle_crafting (unsigned char wid)
	{
		if (wid == 0)
			{
				
			}
		
		return false;
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
		
		this->send (
			packet::make_empty_encryption_key_response ());
		
		// begin encryption
		this->encryptor = new CryptoPP::CFB_Mode<CryptoPP::AES>::Encryption (this->ssec, 16, this->ssec, 1);
		this->encrypted = true;
		
		/* 
		 * The player will spawn as soon as the 0xCD packet is received.
		 */
	}
	
	
	/* 
	 * Sends the world and spawns the player.
	 */
	void
	player::login ()
	{
		if (this->logged_in)
			return;
		
		this->send (packet::make_login (this->get_eid (), "hCraft", this->curr_gamemode,
			0, 0, (this->get_server ().get_config ().max_players > 64)
				? 64 : (this->get_server ().get_config ().max_players)));
		this->logged_in = true;
		if (!this->get_server ().done_connecting (this))
			{
				this->disconnect ();
				return;
			}

		// join message
		this->get_server ().get_players ().message (
			messages::compile (this->get_server ().msgs.server_join, messages::environment (this)));
		
		this->inv.subscribe (this);
		this->inv.add (slot_item (IT_FEATHER, 0, 1));
		this->inv.add (slot_item (BT_STONE, 0, 1));
		
		// make the player move at normal speed
		{
			std::vector<entity_property> props;
			props.push_back ({"generic.movementSpeed", 0.1});
			this->send (packet::make_entity_properties (this->eid, props));
		}
		
		if (this->curr_world)
			this->join_world_at (this->curr_world, this->pos);
		else
			this->join_world (this->get_server ().get_main_world ());

		slot_item sword (IT_IRON_SWORD, 0, 1);
		sword.lore.emplace_back ("§7Poison I");
		sword.enchants.push_back ({ENC_KNOCKBACK, 1});
		this->inv.add (sword);
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
	
	
	int
	player::handle_packet_00 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in)
			{ return -1; }		
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
		
		if ((pl->keep_alives_received++ % 5) == 0)
			{
				/*
				// update self
				pl->send (packet::make_player_list_item (pl->get_colored_username (), true, pl->ping_time_ms));
	
				// update other players
				player *me = pl;
				pl->get_world ()->get_players ().all (
					[me] (player *pl)
						{
							pl->send (packet::make_player_list_item (me->get_colored_username (), true, me->ping_time_ms));
						});
				*/
			}
		
		return 0;
	}
	
	
	static bool
	is_valid_username (const char *str)
	{
		int i;
		for (i = 0; i < 16; ++i)
			{
				char c = str[i];
				if (c == '\0')
					{
						if (i == 0)
							return false;
						return true;
					}
				if (!((c >= 'A' && c <= 'Z') ||
							(c >= 'a' && c <= 'z') ||
							(c >= '0' && c <= '9') ||
							(c == '_')))
					return false;
			}
		 
		 if (str[i] != '\0')
		 	return false;
		
		return true;
	}
	
	int
	player::handle_packet_02 (player *pl, packet_reader reader)
	{
		int protocol_version = reader.read_byte ();
		if (protocol_version != packet::protocol_version)
			{ 
				if (protocol_version < packet::protocol_version)
					pl->kick ("§cOutdated client", "outdated protocol version");
				else
					pl->kick ("§ahCraft has not been updated yet", "newer protocol version");
				return 0;
			}
		
		char username[64];
		/*
		int ulen = reader.read_string (username, 16);
		if ((ulen < 2 || ulen > 16) || !is_valid_username (username))
			{
				pl->log (LT_WARNING) << "@" << pl->get_ip () << " connected with an invalid username." << std::endl;
				return -1;
			}
		//*/
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
		
		
		pl->log () << "Player " << username << " has logged in from @" << pl->get_ip () << std::endl;
		std::strcpy (pl->username, username);
		
		if (!pl->load_data ())
			return -1;
		if (pl->kicked)
			// player got kicked by load_data ()
			return 0;
			
		{
			std::string str;
			str.append ("§");
			str.push_back (pl->rnk.main ()->color);
			str.append (pl->username);
			std::strcpy (pl->colored_username, str.c_str ());
		}
		pl->handshake = true;
		
		
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
					packet::make_encryption_key_request (pl->srv.auth_id (), pkey, pl->vtoken));
			}
		else
			{
				pl->login ();
			}
		
		return 0;
	}
	
	
	
	static void
	handle_message (player *pl, std::string& msg)
	{
		if (pl->get_rank ().main ()->color_codes || pl->is_op ())
			{
				// convert color codes from &X to §X
				
				std::string str;
				str.reserve (msg.size ());
				
				for (size_t i = 0; i < msg.size (); ++i)
					{
						if ((msg[i] == '&') && ((i + 1) < msg.size ()) && is_chat_code (msg[i + 1]))
							{
								str.push_back (0xC2);
								str.push_back (0xA7);
								
								++ i;
								str.push_back (msg[i]);
							}
						else
							str.push_back (msg[i]);
					}
				
				msg.assign (str);
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
	player::handle_packet_03 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		bool is_global_message = false;
		
		char text[384];
		int  text_len = reader.read_string (text, 360);
		if (text_len <= 0)
			{
				pl->log (LT_WARNING) << "Received an invalid string from player " << pl->get_username () << std::endl;
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
		
		if (pl->get_server ().is_player_muted (pl->get_username ()) && !pl->is_op ())
			{
				pl->message ("§cYou have been muted§4, §cstay quiet§4.");
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
			}
		else
			{
				pl->get_logger () (LT_CHAT) << pl->get_username () << ": " << msg << std::endl;
				target = &pl->get_world ()->get_players ();
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
	
	
	
	int
	player::handle_packet_07 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in)
			{ return -1; }
		
		
		return 0;
	}
	
	int
	player::handle_packet_0a (player *pl, packet_reader reader)
	{
		if (!pl->handshake) return -1;
		if (pl->is_dead ()) return 0;
		if (pl->joining_world) return 0;
		
		if (pl->rej_mov > 0)
			{
				pl->send (packet::make_player_pos_and_look (
					pl->pos.x, pl->pos.y, pl->pos.z, pl->pos.y + 1.65, pl->pos.r, pl->pos.l, true));
				-- pl->rej_mov;
				return 0;
			}
		
		bool on_ground;
		
		on_ground = reader.read_byte ();
		
		entity_pos curr_pos = pl->pos;
		entity_pos new_pos {curr_pos.x, curr_pos.y, curr_pos.z, curr_pos.r,
			curr_pos.l, on_ground};
		pl->move_to (new_pos);
		
		pl->try_ping (5000);
		
		return 0;
	}
	
	int
	player::handle_packet_0b (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		if (pl->joining_world) return 0;
		
		if (pl->rej_mov > 0)
			{
				pl->send (packet::make_player_pos_and_look (
					pl->pos.x, pl->pos.y, pl->pos.z, pl->pos.y + 1.65, pl->pos.r, pl->pos.l, true));
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
		
		pl->try_ping (5000);
		
		return 0;
	}
	
	int
	player::handle_packet_0c (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		if (pl->joining_world) return 0;
		
		if (pl->rej_mov > 0)
			{
				pl->send (packet::make_player_pos_and_look (
					pl->pos.x, pl->pos.y, pl->pos.z, pl->pos.y + 1.65, pl->pos.r, pl->pos.l, true));
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
		
		pl->try_ping (5000);
		
		return 0;
	}
	
	int
	player::handle_packet_0d (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		if (pl->joining_world) return 0;
		
		if (pl->rej_mov > 0)
			{
				pl->send (packet::make_player_pos_and_look (
					pl->pos.x, pl->pos.y, pl->pos.z, pl->pos.y + 1.65, pl->pos.r, pl->pos.l, true));
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
		
		pl->try_ping (5000);
		
		return 0;
	}
	
	
	
	int
	player::handle_packet_0e (player *pl, packet_reader reader)
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
				pl->send (packet::make_block_change (
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
				else if (!pl->has_access (w.get_build_perms ()))
					{
						pl->message ("§4 * §cYou are not allowed to build here§4.");
						pl->send_orig_block (x, y, z);
						return 0;
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
	
	int
	player::handle_packet_0f (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		int x = reader.read_int ();
		int y = reader.read_byte ();
		int z = reader.read_int ();
		char direction = reader.read_byte ();
		slot_item item = reader.read_slot ();
		reader.read_byte (); // cursor X
		reader.read_byte (); // cursor Y
		reader.read_byte (); // cursor Z
		
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
								pl->get_world ()->get_players ().send_to_all_visible (
									packet::make_animation (pl->eid, 5), pl);
							}
					}
				else
					{
						// TODO: handle arrows
					}
				return 0;
			}
		
		item = pl->held_item ();
		if (!item.is_valid () || item.empty ())
			return 0;
		if (!item.is_block ())
			return 0;
		
		int nx = x, ny = y, nz = z;
		
		if (pl->get_world ()->get_id (x, y, z) != BT_TALL_GRASS)
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
		
		int w_width = pl->get_world ()->get_width ();
		int w_depth = pl->get_world ()->get_depth ();
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
		else if (!pl->has_access (pl->get_world ()->get_build_perms ()))
			{
				pl->message ("§4 * §cYou are not allowed to build here§4.");
				pl->send_orig_block (nx, ny, nz);
				return 0;
			}
		
		++ pl->bl_created;
		pl->get_world ()->queue_update (nx, ny, nz,
			item.id (), item.damage (), 0, 0, nullptr, pl);
		if (pl->gamemode () != GT_CREATIVE)
			pl->inv.set (pl->held_slot, slot_item (item.id (), item.damage (),
				item.amount () - 1));
		
		return 0;
	}
	
	int
	player::handle_packet_10 (player *pl, packet_reader reader)
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
			packet::make_entity_equipment (pl->eid, 0, pl->inv.get (pl->held_slot)), pl);
		
		return 0;
	}
	
	int
	player::handle_packet_12 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		reader.read_int (); // entity ID
		char animation = reader.read_byte ();
		
		// DEBUG
		{
			std::vector<block_change_record> records;
			records.push_back({15, 70, 15, 20, 0});
			pl->send (packet::make_multi_block_change (500, 200, records));
		}
		
		//pl->inv.update ();
		
		if (animation == 1)
			{
				pl->get_world ()->get_players ().send_to_all_visible (
					packet::make_animation (pl->eid, animation), pl);
			}
		
		if (pl->curr_gamemode == GT_CREATIVE && pl->inv.get (pl->held_slot).id () == IT_FEATHER)
			{
				// TODO: check permissions
				
				double ux = -std::cos (pl->pos.l * 0.017453293) * std::sin (pl->pos.r * 0.017453293);
				double uy = -std::sin (pl->pos.l * 0.017453293);
				double uz =  std::cos (pl->pos.l * 0.017453293) * std::cos (pl->pos.r * 0.017453293);
				
				int speed = pl->is_crouched () ? 32767 : 13500;
				pl->send (packet::make_entity_velocity (pl->eid,
					ux * speed, uy * speed, uz * speed));
			}
		
		return 0;
	}
	
	int
	player::handle_packet_13 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in)
			{ return -1; }
		
		int eid = reader.read_int ();
		int action = reader.read_byte ();
		
		if (eid != pl->get_eid ())
			return -1;
		
		switch (action)
			{
				case 1: pl->crouched = true; break;
				case 2: pl->crouched = false; break;
				case 3: /* leave bed */ break;
				case 4: pl->sprinting = true; break;
				case 5: pl->sprinting = false; break;
				
				default: return -1;
			}
		
		return 0;
	}
	
	int
	player::handle_packet_cd (player *pl, packet_reader reader)
	{
		char payload = reader.read_byte ();
		if (payload == 0)
			{
				/* 
				 * Finished logging in.
				 */
				
				pl->login ();
			}
		else if (payload == 1)
			{
				/* 
				 * Respawn
				 */
				
				pl->send (packet::make_respawn (0, 0, pl->curr_gamemode, "hCraft"));
				
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
	
	int
	player::handle_packet_65 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in)
			return -1;
		
		pl->log (LT_DEBUG) << "close window" << std::endl;
		return 0;
	}
	
	int
	player::handle_packet_66 (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		if (pl->is_dead ()) return 0;
		
		unsigned char wid = reader.read_byte ();
		short slot = reader.read_short ();
		char button = reader.read_byte ();
		short action = reader.read_short ();
		char mode = reader.read_byte ();
		
		//slot_item item = reader.read_slot ();
		
		if (wid != 0)
			return 0; // TODO: we currently only handle player inventory
		
		window *win = &pl->inv;
		pl->log (LT_DEBUG) << "| Mode [" << (int)mode << "] Button [" << (int)button << "] Slot [" << slot << "]" << std::endl;
		
		switch (mode)
			{
				/* 
				 * MODE 0:
				 *   button 0 = left mouse click
				 *   button 1 = right mouse click
				 */
				case 0:
					if (slot < 0 || slot >= win->slot_count ())
						pl->log (LT_WARNING) << "Player \"" << pl->get_username ()
							<< "\" tried to use a slot outside of window." << std::endl;
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
											}
									}
								else
									{
										if (!win->can_place_at (slot, item))
											return 0;
										
										if (pl->cursor_slot.compatible_with (item))
											{
												// put stack down
												int take = pl->cursor_slot.amount ();
												if (item.amount () + take > item.max_stack ())
													take = item.max_stack () - item.amount ();
												
												if (item.empty ())
													item.set (pl->cursor_slot);		
												else
													item.give (take);
												pl->cursor_slot.take (take);
												pl->log (LT_DEBUG) << "Putting " << take << " down [" << pl->cursor_slot.amount () << " left in hand]" << std::endl;
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
								if (pl->cursor_slot.empty ())
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
														if (item.empty ())
															{
																item.set (pl->cursor_slot);
																item.set_amount (1);
															}
														else
															item.give (1);
														pl->cursor_slot.take (1);
														pl->log (LT_DEBUG) << "Putting one down [" << pl->cursor_slot.amount () << " left in hand]" << std::endl;
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
						{
							pl->log (LT_WARNING) << "Player \"" << pl->get_username ()
								<< "\" tried to use a slot outside of window." << std::endl;
							return -1;
						}
					{
						slot_item& item = win->get (slot);
						if (item.empty ())
							break;
						
						auto range = win->shift_range (slot);
						if (range.first == -1 || range.second == -1)
							break;
							
						pl->log (LT_DEBUG) << "Shifting [" << slot << " of type " << item.id () << "] to [" << range.first << " -> " << range.second << "]" << std::endl;
						
						// we perform two passes - try items with matching IDs first, and
						// then finally try empty slots too.
						bool first_pass = true;
						for (int j = 0; j < 2; ++j)
							{
								int i, a = (range.first < range.second) ? 1 : -1;
								for (i = range.first; i != (range.second + a) && !item.empty (); i += a)
									{
										slot_item& sl = win->get (i);
										if (first_pass ? (item.id () == sl.id ()) : sl.compatible_with (item))
											{
												//pl->log (LT_DEBUG) << "Trying [" << i << "]: yes" << std::endl;
												int take = item.amount ();
												if (sl.amount () + take > sl.max_stack ())
													take = sl.max_stack () - sl.amount ();
												
												int dbg_then = sl.amount ();
												if (sl.empty ())
													{
														sl.set (item);
														sl.set_amount (take);
													}	
												else
													sl.give (take);
												item.take (take);
												
												pl->log (LT_DEBUG) << "  Adding " << take << " to [" << i << "] [of type: " << sl.id () << "] [then: " << dbg_then << "] [now: " << sl.amount () << "]" << std::endl;
											}
										//else
										//	pl->log (LT_DEBUG) << "Trying [" << i << "]: nope (" << sl.id () << ") (our: " << item.id () <<  << std::endl;
									} 
								
								first_pass = false;
							}
						
						pl->log (LT_DEBUG) << "  Done, " << item.amount () << " left" << std::endl;
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
						{
							pl->log (LT_WARNING) << "Player \"" << pl->get_username ()
								<< "\" tried to use a slot outside of window." << std::endl;
							return -1;
						}
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
						{
							pl->log (LT_WARNING) << "Player \"" << pl->get_username ()
								<< "\" tried to use a slot outside of window." << std::endl;
							return -1;
						}
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
		
		return 0;
	}
	
	int
	player::handle_packet_6a (player *pl, packet_reader reader)
	{
		if (!pl->logged_in) return -1;
		
		pl->log (LT_DEBUG) << "confirm transaction" << std::endl;
		return 0;
	}
	
	int
	player::handle_packet_6b (player *pl, packet_reader reader)
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
		
		pl->inv.set (index, item, false);
		return 0;
	}
	
	int
	player::handle_packet_fa (player *pl, packet_reader reader)
	{
		char str[1024];
		reader.read_string (str, 1024);
		
		pl->log (LT_DEBUG) << "Plugin Message: " << str << std::endl;
		
		return 0;
	}
	
	int
	player::handle_packet_fc (player *pl, packet_reader reader)
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
	
	int
	player::handle_packet_fe (player *pl, packet_reader reader)
	{
		int magic = reader.read_byte ();
		if (magic != 1)
			{
				pl->log (LT_WARNING) << "Received an invalid server list ping packet from @"
					<< pl->get_ip () << std::endl;
				return -1;
			}
		
		pl->kick ("", "server list ping", false);
		return 0;
	}
	
	int
	player::handle_packet_ff (player *pl, packet_reader reader)
	{
		return -1;
	}
	
	/* 
	 * Executes the appropriate packet handler for the given byte array.
	 */
	int
	player::handle (const unsigned char *data)
	{
		static int (*handlers[0x100]) (player *, packet_reader) =
			{
				handle_packet_00, handle_packet_xx, handle_packet_02, handle_packet_03, // 0x03
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_07, // 0x07
				handle_packet_xx, handle_packet_xx, handle_packet_0a, handle_packet_0b, // 0x0B
				handle_packet_0c, handle_packet_0d, handle_packet_0e, handle_packet_0f, // 0x0F
				
				handle_packet_10, handle_packet_xx, handle_packet_12, handle_packet_13, // 0x13
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x17
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
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x47
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x4B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x4F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x53
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x57
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x5B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x5F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x63
				handle_packet_xx, handle_packet_65, handle_packet_66, handle_packet_xx, // 0x67
				handle_packet_xx, handle_packet_xx, handle_packet_6a, handle_packet_6b, // 0x6B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x6F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x73
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x77
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x7B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x7F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x83
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x87
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x8B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x8F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x93
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x97
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x9B
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0x9F
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xA3
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xA7
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xAB
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xAF
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xB3
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xB7
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xBB
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xBF
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xC3
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xC7
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xCB
				handle_packet_xx, handle_packet_cd, handle_packet_xx, handle_packet_xx, // 0xCF
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xD3
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xD7
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xDB
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xDF
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xE3
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xE7
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xEB
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xEF
				
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xF3
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xF7
				handle_packet_xx, handle_packet_xx, handle_packet_fa, handle_packet_xx, // 0xFB
				handle_packet_fc, handle_packet_xx, handle_packet_fe, handle_packet_ff, // 0xFF
			};
		
		if (this->bad ()) return 0;
		packet_reader reader {data};
		return handlers[reader.read_byte ()] (this, reader);
	}



//----
	
	static bool
	test_packet_66 (packet_reader reader)
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
		// if the last packet is different compared to the existing ones,
		// then consider it as the end of chain.
		if (this->exec_queue.size () > 1)
			{
				int last_op = -1;
				for (auto itr = this->exec_queue.rbegin (); itr != this->exec_queue.rend (); ++itr)
					{
						if (last_op == -1)
							last_op = (*itr)[0];
						else
							{
								if (last_op != (*itr)[0])
									return true;
							}
					}
			} 
		
		// we only test the last packet
		unsigned char *data = this->exec_queue.back ();
		packet_reader reader {data};
		
		switch (reader.read_byte ())
			{
				// window click
				case 0x66: return test_packet_66 (reader);
				case 0x6B: return false;
				
				default:
					return true;
			}
	}
}

