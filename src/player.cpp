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

#include "player.hpp"
#include "server.hpp"
#include "stringutils.hpp"
#include "wordwrap.hpp"
#include "commands/command.hpp"
#include "utils.hpp"
#include "sql.hpp"
#include "pickup.hpp"

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


namespace hCraft {
	
	/* 
	 * Constructs a new player around the given socket.
	 */
	player::player (server &srv, struct event_base *evbase, evutil_socket_t sock,
		const char *ip)
		: entity (srv.next_entity_id ()),
			srv (srv), log (srv.get_logger ()), sock (sock)
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
		
		this->disconnecting = false;
		this->reading = false;
		this->writing = false;
		this->handlers_scheduled = 0;
		this->total_read = 0;
		this->read_rem = 1;
		
		this->eating = false;
		this->hearts = 20;
		this->hunger = 20;
		this->hunger_saturation = 20.0;
		this->exhaustion = 0.0;
		this->total_walked = this->total_run = this->total_walked_old = this->total_run_old = 0.0;
		
		this->last_tick = std::chrono::steady_clock::now ();
		this->last_heart_regen = this->last_tick;
		this->heal_delay = std::chrono::milliseconds (4000);
		
		this->curr_world = nullptr;
		this->curr_chunk = chunk_pos (0, 0);
		this->ping_waiting = false;
		this->sb_block.set (BT_GLASS);
		
		this->curr_sel = nullptr;
		this->last_ping = std::chrono::system_clock::now ();
		
		this->evbase = evbase;
		this->bufev  = bufferevent_socket_new (evbase, sock,
			BEV_OPT_CLOSE_ON_FREE);
		if (!this->bufev)
			{ this->fail = true; this->get_server ().schedule_destruction (this); return; }
		
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
		//std::cout << "destruct: start""\n";
		this->disconnect ();
		
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
		//std::cout << "destruct: end""\n";
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
		if (pl->bad ())	return;
		pl->reading = true;
		
		struct evbuffer *buf = bufferevent_get_input (bufev);
		size_t buf_size;
		int n;
		
		while ((buf_size = evbuffer_get_length (buf)) > 0)
			{
				n = evbuffer_remove (buf, pl->rdbuf + pl->total_read, pl->read_rem);
				if (n <= 0)
					{ pl->reading = false; pl->disconnect (); return; }
				
				// a small check...
				if (!pl->handshake && (pl->total_read == 0))
					{
						if (pl->rdbuf[0] != 0x02 && pl->rdbuf[0] != 0xFE)
							{
								pl->log (LT_WARNING) << "Expected handshake from @" << pl->get_ip () << std::endl;
								pl->reading = false; 
								pl->disconnect ();
								return;
							}
					}
				
				pl->total_read += n;
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
		//std::cout << "disconnect: start""\n";
		this->fail_time = std::chrono::system_clock::now ();
		this->fail = true;
		this->disconnecting = true;
		
		bufferevent_disable (this->bufev, EV_READ | EV_WRITE);
		bufferevent_setcb (this->bufev, nullptr, nullptr, nullptr, nullptr);
		
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
		if (this->curr_world && !this->get_server ().is_shutting_down ())
			{
				this->curr_world->get_players ().remove (this);
				if (this->handshake && !silent)
					{
						std::ostringstream ss;
						ss << "§e[§c-§e] §" << this->get_rank ().main_group->get_color () << this->get_username ()
							<< " §ehas left the server";
						this->get_server ().get_players ().message (ss.str ());
					}
				
				chunk *curr_chunk = this->curr_world->get_chunk (
					this->curr_chunk.x, this->curr_chunk.z);
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
		//std::cout << "disconnect: end""\n";
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
	
	
	
	/* 
	 * Inserts the specified packet into the player's queue of outgoing packets.
	 */
	void
	player::send (packet *pack)
	{
		if (this->bad ())
			{ delete pack; return; }
		
		std::lock_guard<std::mutex> guard {this->out_lock};
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
	player::join_world (world* w)
	{
		this->join_world_at (w, w->get_spawn ());
	}
	
	
	/* 
	 * Sends the player to the given world at the specified location.
	 */
	void
	player::join_world_at (world *w, entity_pos destpos)
	{
		std::lock_guard<std::mutex> guard {this->join_lock};
		bool had_prev_world = (this->curr_world != nullptr);
		
		// this will keep the player safe from fall damage right after spawning
		this->fall_flag = true;
		
		this->sb_updates.set_world (w, true);
		
		if (had_prev_world && (this->curr_world != w))
			{
				// destroy selections
				for (auto itr = this->selections.begin (); itr != this->selections.end (); ++itr)
					{
						world_selection *sel = itr->second;
						delete sel;
					}
				this->selections.clear ();
			}
		
		/* 
		 * Unload chunks from previous world.
		 */
		if (this->curr_world)
			{
				// we first stop the player from moving.
				entity_pos curr_pos = this->pos;
				this->send (packet::make_player_pos_and_look (
					curr_pos.x, curr_pos.y, curr_pos.z, curr_pos.y + 1.65, curr_pos.r,
					curr_pos.l, true));
				this->pos = curr_pos;
				
				// despawn self from other players (and vice-versa).
				player *me = this;
				this->curr_world->get_players ().remove (this);
				this->curr_world->get_players ().all (
					[me] (player *pl)
						{
							if (me->visible_to (pl))
								{
									me->despawn_from (pl);
									pl->despawn_from (me);
								}
						});
				
				// despawn from entities
				this->curr_world->all_entities (
					[me] (entity *e)
						{
							e->despawn_from (me);
						});
				
				// this ensures smooth transitions between worlds:
				this->stream_common_chunks (w, destpos);
			}
		
		this->curr_world = w;
		this->pos = destpos;
		this->curr_world->get_players ().add (this);
		
		this->last_ground_height = -128.0;
		this->stream_chunks ();
		
		entity_pos epos = this->pos;
		block_pos bpos = epos;
		
		this->send (packet::make_spawn_pos (bpos.x, bpos.y, bpos.z));
		if (!had_prev_world)
			this->send (packet::make_player_pos_and_look (
				epos.x, epos.y, epos.z, epos.y + 1.65, epos.r, epos.l, true));
			
		log () << this->get_username () << " joined world \"" << w->get_name () << "\"" << std::endl;
	}
	
	
	
//--
	class chunk_pos_less
	{
		chunk_pos origin;
		
	private:
		double
		distance (const chunk_pos c1, const chunk_pos c2) const
		{
			double xd = (double)c1.x - (double)c2.x;
			double zd = (double)c1.z - (double)c2.z;
			return std::sqrt (xd * xd + zd * zd);
		}
		
	public:
		chunk_pos_less (chunk_pos origin)
			: origin (origin)
			{ }
		
		// checks whether lhs < rhs.
		bool
		operator() (const chunk_pos lhs, const chunk_pos rhs) const
		{
			return distance (lhs, origin) < distance (rhs, origin);
		}
	};
	
	/* 
	 * Loads new close chunks to the player and unloads those that are too
	 * far away.
	 */
	void
	player::stream_chunks (int radius)
	{
		std::lock_guard<std::mutex> wguard {this->world_lock};
		std::multiset<chunk_pos, chunk_pos_less> to_load {chunk_pos_less (this->pos)};
		auto prev_chunks = this->known_chunks;
		
		chunk_pos center = this->pos;
		int r_half = radius / 2;
		for (int cx = (center.x - r_half); cx <= (center.x + r_half); ++cx)
			for (int cz = (center.z - r_half); cz <= (center.z + r_half); ++cz)
				{
					chunk_pos cpos = chunk_pos (cx, cz);
					if (this->known_chunks.count (cpos) == 0)
						{
							to_load.insert (cpos);
						}
					prev_chunks.erase (cpos);
				}
		
		for (auto cpos : to_load)
			{
				this->known_chunks.insert (cpos);
				chunk *ch = this->get_world ()->load_chunk (cpos.x, cpos.z);
				this->send (packet::make_chunk (cpos.x, cpos.z, ch));
				
				// spawn self to other players and vice-versa.
				player *me = this;
				ch->all_entities (
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
		to_load.clear ();
		
		for (auto cpos : prev_chunks)
			{
				this->known_chunks.erase (cpos);
				this->send (packet::make_empty_chunk (cpos.x, cpos.z));
				
				// despawn self from other players and vice-versa.
				chunk *ch = this->get_world ()->load_chunk (cpos.x, cpos.z);
				
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
		prev_chunks.clear ();
		
		chunk *prev_chunk = this->get_world ()->get_chunk (this->curr_chunk.x, this->curr_chunk.z);
		if (prev_chunk)
			prev_chunk->remove_entity (this);
		
		chunk *new_chunk = this->get_world ()->load_chunk (center.x, center.z);
		new_chunk->add_entity (this);
		this->curr_chunk.set (center.x, center.z);
	}
	
	/* 
	 * Used when transitioning players between worlds or teleporting.
	 * This sends common chunks (shared by two worlds in their position) without
	 * unloading them first.
	 */
	void
	player::stream_common_chunks (world *wr, entity_pos dest_pos, int radius)
	{
		std::lock_guard<std::mutex> wguard {this->world_lock};
		
		auto spawn_pos = dest_pos;
		chunk_pos center = spawn_pos;
		
		chunk *prev_chunk = this->get_world ()->get_chunk (this->curr_chunk.x, this->curr_chunk.z);
		if (prev_chunk)
			prev_chunk->remove_entity (this);
		
		std::multiset<chunk_pos, chunk_pos_less> to_load {chunk_pos_less (spawn_pos)};
		int r_half = radius / 2;
		for (int cx = (center.x - r_half); cx <= (center.x + r_half); ++cx)
			for (int cz = (center.z - r_half); cz <= (center.z + r_half); ++cz)
				{
					chunk_pos cpos = chunk_pos (cx, cz);
					to_load.insert (cpos);
				}
		
		// keep chunks that are shared between both worlds, remove others.
		for (auto itr = to_load.begin (); itr != to_load.end (); )
			{
				chunk_pos cpos = *itr;
				if (std::find (this->known_chunks.begin (), this->known_chunks.end (),
					cpos) == this->known_chunks.end ())
					{
						// not contained by both worlds, remove.
						itr = to_load.erase (itr);
					}
				else
					++ itr;
			}
		
		for (auto cpos : to_load)
			{
				chunk *ch = wr->load_chunk (cpos.x, cpos.z);
				this->send (packet::make_chunk (cpos.x, cpos.z, ch));
			}
		
		this->send (packet::make_player_pos_and_look (
			dest_pos.x, dest_pos.y, dest_pos.z, dest_pos.y + 1.65, dest_pos.r,
				dest_pos.l, true));
		this->pos = dest_pos;
		
		// spawn self to other players and vice-versa.
		for (auto cpos : to_load)
			{
				player *me = this;
				chunk *ch = wr->get_chunk (cpos.x, cpos.z);
				if (ch)
					{
						ch->all_entities (
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
		
		// unload all other chunks
		for (auto itr = this->known_chunks.begin (); itr != this->known_chunks.end (); )
			{
				chunk_pos cpos = *itr;
				if (std::find (to_load.begin (), to_load.end (), cpos) == to_load.end ())
					{
						this->send (packet::make_empty_chunk (cpos.x, cpos.z));
						itr = this->known_chunks.erase (itr);
					}
				else
					++ itr;
			}
		
		to_load.clear ();
	}
	
	/* 
	 * Checks whether the specified chunk is within the visible chunk range
	 * of the player.
	 */
	bool
	player::can_see_chunk (int x, int z)
	{
		std::unique_lock<std::mutex> guard {this->world_lock};
		auto itr = this->known_chunks.find (chunk_pos (x, z));
		return (itr != this->known_chunks.end ());
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
				
				if (changed)
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
				this->stream_chunks ();
			}
		
		if (prev_pos == dest)
			return;
		
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
	
	static void
	get_ping_name (char col, const char *username, char *out)
	{
		std::string str;
		str.append ("§");
		str.push_back (col);
		str.append (username);
		if (str.size () > 16)
			{
				str.resize (13);
				str.append ("...");
			}
		std::strcpy (out, str.c_str ());
	}
	
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
		if (pl == this)
			return;
		
		std::string col_name;
		col_name.append ("§");
		col_name.push_back (this->get_rank ().main_group->get_color ());
		col_name.append (this->get_username ());
		
		char ping_name[24];
		get_ping_name (this->get_rank ().main_group->get_color (), this->get_username (),
			ping_name);
		
		entity_pos me_pos = this->pos;
		entity_metadata me_meta;
		this->build_metadata (me_meta);
		pl->send (packet::make_spawn_named_entity (
			this->get_eid (), col_name.c_str (),
			me_pos.x, me_pos.y, me_pos.z, me_pos.r, me_pos.l, 0, me_meta));
		pl->send (packet::make_entity_head_look (this->get_eid (), me_pos.r));
		pl->send (packet::make_entity_equipment (this->eid, 0, this->inv.get (this->held_slot)));
		//pl->send (packet::make_player_list_item (ping_name, true, this->ping_time_ms));
		
		{
			std::lock_guard<std::mutex> guard {pl->visible_player_lock};
			pl->visible_players.insert (this);
		}
	}
	
	/* 
	 * Despawns self from the specified player.
	 */
	bool
	player::despawn_from (player *pl)
	{
		if (!entity::despawn_from (pl))
			return false;
		
		//char ping_name[24];
		//get_ping_name (this->get_rank ().main_group->get_color (), this->get_username (),
		//	ping_name);
		//pl->send (packet::make_player_list_item (ping_name, false, 0));
		
		std::lock_guard<std::mutex> guard {pl->visible_player_lock};
		pl->visible_players.erase (this);
		return true;
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
	 */
	void
	player::load_data ()
	{
		{
			auto& conn = this->get_server ().sql ().pop ();
			auto stmt = conn.query ("SELECT * FROM `players` WHERE `name`=?");
			stmt.bind (1, this->get_username ());
			
			sql::row row;
			if (stmt.step (row))
				{
					try
						{
							this->rnk.set (row.at (2).as_cstr (), this->get_server ().get_groups ());
						}
					catch (const std::exception& str)
						{
							this->rnk.set (this->get_server ().get_groups ().default_rank);
							this->log (LT_WARNING) << "Player \"" << this->get_username () << "\" has an invalid rank." << std::endl;
						}
					std::strcpy (this->nick, row.at (3).as_cstr ());
				}
			else
				{
					this->rnk.set (this->get_server ().get_groups ().default_rank);
					std::string grp_str;
					this->rnk.get_string (grp_str);
					
					std::strcpy (this->nick, this->username);
					
					auto stmt = conn.query (
						"INSERT INTO `players` (`name`, `groups`, `nick`) VALUES (?, ?, ?)");
					stmt.bind (1, this->get_username ());
					stmt.bind (2, grp_str.c_str (), sql::pass_transient);
					stmt.bind (3, this->get_nickname ());
					stmt.execute ();
				}
			
			this->get_server ().sql ().push (conn);
		}
		
		std::string str;
		str.append ("§");
		str.push_back (this->rnk.main ()->get_color ());
		str.append (this->nick);
		std::strcpy (this->colored_nick, str.c_str ());
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
		str.push_back (this->rnk.main ()->get_color ());
		str.append (this->nick);
		std::strcpy (this->colored_nick, str.c_str ());
		
		if (modify_sql)
			{
				std::ostringstream ss;
				ss << "UPDATE `players` SET `nick`='"
				 << nick << "' WHERE `name`='"
				 << this->get_username () << "'";
				this->get_server ().execute_sql (ss.str ());
			}
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
	player::sb_add (int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		auto itr = this->sel_blocks.find (selection_block (x, y, z));
		if (itr != this->sel_blocks.end ())
			{ ++ itr->counter; return; }
		
		this->sb_updates.set (x, y, z, this->sb_block.id, this->sb_block.meta);
		this->sel_blocks.insert (selection_block (x, y, z, 1));
	}
	
	void
	player::sb_remove (int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		auto itr = this->sel_blocks.find (selection_block (x, y, z));
		if (itr == this->sel_blocks.end ())
			return;
		
		-- itr->counter;
		if (itr->counter == 0)
			{
				this->sel_blocks.erase (itr);
				block_data bd = this->curr_world->get_block (x, y, z);
				this->sb_updates.set (x, y, z, bd.id, bd.meta);
			}
	}
	
	bool
	player::sb_exists (int x, int y, int z)
	{
		if (y < 0 || y > 255) return false;
		return (this->sel_blocks.find (selection_block (x, y, z))
			!= this->sel_blocks.end ());
	}
	
	void
	player::sb_commit ()
	{
		this->sb_updates.preview_to (this);
		this->sb_updates.reset ();
	}
	
	void
	player::sb_send (int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		this->send (packet::make_block_change (x, y, z, this->sb_block.id,
			this->sb_block.meta));
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
	 * Health modification:
	 */
	
	void
	player::set_hearts (int hearts)
	{
		if (hearts < this->hearts)
			{
				// exhaustion from damage
				this->increment_exhaustion (0.3);
			}
		this->set_health (hearts, this->hunger, this->hunger_saturation);
	}
	
	void
	player::set_hunger (int hunger)
	{
		if (this->hunger_saturation > hunger)
			this->hunger_saturation = hunger;
		this->set_health (this->hearts, hunger, this->hunger_saturation);
	}
	
	void
	player::set_hunger_saturation (float hunger_saturation)
	{
		this->set_health (this->hearts, this->hunger, hunger_saturation);
	}
	
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
		
		this->hearts = hearts;
		this->hunger = hunger;
		this->hunger_saturation = hunger_saturation;
	}
	
	void
	player::increment_exhaustion (float val)
	{
		this->exhaustion += val;
		if (this->exhaustion >= 4.0)
			{
				this->exhaustion -= 4.0;
				if (this->hunger_saturation <= 0.0)
					this->set_hunger (this->hunger - 1);
				else
					{
						double new_sat = this->hunger_saturation - 1.0;
						if (new_sat < 0.0) new_sat = 0.0;
						this->set_hunger_saturation (new_sat);
					}
			}
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
		// drop everything
		if (this->curr_gamemode != GT_CREATIVE)
			{
				std::minstd_rand rnd (utils::ns_since_epoch ());
				std::uniform_real_distribution<> dis (0, 2);
				
				entity_pos pos = this->pos;
				for (int j = 0; j < 5; ++j)
					{
						for (int i = 0; i <= 44; ++i)
							{
								slot_item s = this->inv.get (i);
								if (!s.empty ())
									{
										pickup_item *pick = new pickup_item (this->srv.next_entity_id (), s);
										pick->pos.set_pos (pos.x + dis (rnd), pos.y + dis (rnd), pos.z + dis (rnd));
										this->get_world ()->spawn_entity (pick);
									}
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
		
		this->last_tick = now;
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
	static bool
	ms_passed (std::chrono::time_point<std::chrono::system_clock> pt, int ms)
	{
		auto now = std::chrono::system_clock::now ();
		return ((pt + std::chrono::milliseconds (ms)) <= now);
	}*/
	
	
	
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
		
		// update self
		char ping_name[24];
		get_ping_name (pl->get_rank ().main_group->get_color (), pl->get_username (),
			ping_name);
		//pl->send (packet::make_player_list_item (ping_name, true, pl->ping_time_ms));
		
		// update other players
		player *me = pl;
		pl->get_world ()->get_players ().all (
			[me] (player *pl)
				{
					char ping_name[24];
					get_ping_name (me->get_rank ().main_group->get_color (), me->get_username (),
						ping_name);
					//pl->send (packet::make_player_list_item (ping_name, true, me->ping_time_ms));
				});
		
		return 0;
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
		
		char username[17];
		///*
		int username_len = reader.read_string (username, 16);
		if (username_len < 2)
			{
				pl->log (LT_WARNING) << "@" << pl->get_ip () << " connected with an invalid username." << std::endl;
				return -1;
			}
		//*/
		/*
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
		
		pl->load_data ();
		pl->handshake = true;
		
		{
			std::string str;
			str.append ("§");
			str.push_back (pl->rnk.main ()->get_color ());
			str.append (pl->username);
			std::strcpy (pl->colored_username, str.c_str ());
		}
		
		pl->curr_gamemode = GT_CREATIVE;
		pl->send (packet::make_login (pl->get_eid (), "hCraft", pl->curr_gamemode,
			0, 0, (pl->get_server ().get_config ().max_players > 64)
				? 64 : (pl->get_server ().get_config ().max_players)));
		pl->logged_in = true;
		if (!pl->get_server ().done_connecting (pl))
			return 0;
		
		// insert self into player list ping
		{
			char ping_name[128];
			get_ping_name (pl->get_rank ().main_group->get_color (), pl->get_username (),
				ping_name);
			//pl->send (packet::make_player_list_item (ping_name, true, pl->ping_time_ms));
		}
		
		{
			std::ostringstream ss;
			ss << "§e[§a+§e] " << pl->get_colored_nickname ()
				 << " §ehas joined the server§f!";
			pl->get_server ().get_players ().message (ss.str ());
		}
		pl->join_world (pl->get_server ().get_main_world ());
		
		pl->inv.subscribe (pl);
		
		pl->inv.add (slot_item (IT_FEATHER, 0, 1));
		pl->inv.add (slot_item (BT_STONE, 0, 1));
		
		slot_item sword (IT_IRON_SWORD, 0, 1);
		sword.lore.emplace_back ("§7Poison I");
		sword.enchants.push_back ({ENC_KNOCKBACK, 1});
		pl->inv.add (sword);
		
		return 0;
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
		std::ostringstream ss;
		
		if (is_global_message)
			ss << "§c# ";
		
		group *mgrp = pl->get_rank ().main ();
		ss << mgrp->get_mprefix ();
		for (group *grp : pl->get_rank ().get_groups ())
			ss << grp->get_prefix ();
		ss << pl->get_colored_nickname ();
		ss << mgrp->get_msuffix ();
		for (group *grp : pl->get_rank ().get_groups ())
			ss << grp->get_suffix ();
		ss << "§f: " << "§" << mgrp->get_text_color () << msg;
		
		std::string out = ss.str ();
		
		playerlist *target;
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
					pl->message_wrapped (out.c_str ());
				});
		
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
		
		int w_width = pl->get_world ()->get_width ();
		int w_depth = pl->get_world ()->get_depth ();
		if (((w_width > 0) && ((x >= w_width) || (x < 0))) ||
				((w_depth > 0) && ((z >= w_depth) || (z < 0))))
			{
				pl->send (packet::make_block_change (
					x, y, z,
					pl->get_world ()->get_id (x, y, z),
					pl->get_world ()->get_meta (x, y, z)));
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
		if (pl->mark_block (x, y, z))
			return 0;
		
		block_data bd = pl->get_world ()->get_block (x, y, z);
		if (status == 2 || (status == 0 && pl->curr_gamemode == GT_CREATIVE))
			{
				/* 
				 * Digging
				 */
				
				// degrade tool
				if (pl->curr_gamemode != GT_CREATIVE)
					{
						slot_item& held = pl->held_item ();
						if (item_info::is_tool (held.id ()))
							{
								held.set_damage (held.damage () + 1);
							}
					}
				
				pl->get_world ()->queue_update (x, y, z, 0, 0, 0, nullptr, pl);
				if (pl->gamemode () == GT_SURVIVAL)
					{
						pl->increment_exhaustion (0.025);
						
						blocki drop = block_info::get_drop ({bd.id, bd.meta});
						if (drop.id != BT_AIR && drop.valid ())
							{
								pickup_item *pick = new pickup_item (pl->srv.next_entity_id (),
								slot_item (drop.id, drop.meta, 1));
								pick->pos.set_pos (x + 0.5, y + 0.5, z + 0.5);
								pl->get_world ()->spawn_entity (pick);
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
		switch (direction)
			{
				case 0: -- ny; break;
				case 1: ++ ny; break;
				case 2: -- nz; break;
				case 3: ++ nz; break;
				case 4: -- nx; break;
				case 5: ++ nx; break;
			}
		
		int w_width = pl->get_world ()->get_width ();
		int w_depth = pl->get_world ()->get_depth ();
		if (((w_width > 0) && ((nx >= w_width) || (nx < 0))) ||
				((w_depth > 0) && ((nz >= w_depth) || (nz < 0))))
			{
				pl->send (packet::make_block_change (
					nx, ny, nz,
					pl->get_world ()->get_id (nx, ny, nz),
					pl->get_world ()->get_meta (nx, ny, nz)));
				return 0;
			}
			
		if (pl->sb_exists (nx, ny, nz))
			{
				// modifying a selection block
				pl->sb_send (nx, ny, nz);
				return 0;
			}
		
		pl->get_world ()->queue_update (nx, ny, nz,
			item.id (), item.damage ());
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
		
		pl->inv.update ();
		
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
		if (payload == 1)
			{
				pl->send (packet::make_respawn (0, 0, pl->curr_gamemode, "hCraft"));
				
				// revert health
				pl->hearts = pl->hunger = 20;
				pl->hunger_saturation = 5.0;
				
				entity_pos spawn_pos = pl->curr_world->get_spawn ();
				pl->last_ground_height = -128.0;
				pl->teleport_to (spawn_pos);
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
		char mbtn = reader.read_byte ();
		short actnum = reader.read_short ();
		char mode = reader.read_byte ();
		
		//slot_item item = reader.read_slot ();
		// use our own item:
		slot_item item = pl->inv.get (slot);
		
		pl->log (LT_DEBUG) << "[" << slot << "]: btn(" << (int)mbtn << ") mode(" << (int)mode <<
			") item(" << item.id () << ")" << std::endl;
		
		if (mode == 3 || mbtn == 3)
			return 0;
		
		// TODO: This assumes that we're only handling the inventory.
		if (slot < 0 || slot >= pl->inv.slot_count ())
			{
				if (mode == 5 && slot == -999)
					{
						// inv painting
						switch (mbtn)
							{
							// begin painting
							case 0:
								if (pl->inv_painting)
									return -1;
								pl->inv_mb = 0;
								pl->inv_paint_slots.clear ();
								pl->inv_painting = true;
								return 0;
								
							case 4:
								if (pl->inv_painting)
									return -1;
								pl->inv_mb = 1;
								pl->inv_paint_slots.clear ();
								pl->inv_painting = true;
								return 0;
							
							// end painting
							case 2: case 6:
								if (!pl->inv_painting)
									return -1;
								pl->inv_painting = false;
								
								// now that we have collected all affected slots, reorganize...
								if (!pl->inv_paint_slots.empty ())
									{
										int give = (pl->inv_mb == 1) ? 1
											: (pl->cursor_slot.amount () / pl->inv_paint_slots.size ());
										
										for (short s : pl->inv_paint_slots)
											{
												if (pl->cursor_slot.amount () < give)
													return 0;
												
												slot_item& prev = pl->inv.get (s);
												
												// make sure both items are compatible
												if (item.compatible_with (pl->cursor_slot))
													{
														if (pl->cursor_slot.amount () < give)
															return 0;
														
														short this_give = give;
														if ((prev.id () != BT_AIR) && ((prev.amount () + this_give) > prev.max_stack ()))
															this_give = prev.max_stack () - prev.amount ();
														
														pl->cursor_slot.set_amount (pl->cursor_slot.amount () - this_give);
														
														int p_amount = prev.amount ();
														if (prev.id () == BT_AIR)
															prev = pl->cursor_slot;
														prev.set_amount (p_amount + this_give);
														//pl->inv.set (s, prev, false);
													}
											}
									}
								
								return 0;
							}
					}
				else
					return -1;
			}
		
		if (mode == 5)
			{
				if (pl->inv_painting)
					{
						// remember slot
						pl->inv_paint_slots.push_back (slot);
					}
				return 0;
			}
		
		if (item.id () == BT_AIR)
			{
				// putting item back
				if (pl->cursor_slot.id () == BT_AIR)
					return 0;
				pl->log (LT_DEBUG) << "Putting item back" << std::endl;
				
				pl->inv.get (slot) = pl->cursor_slot;
				pl->cursor_slot.set_id (BT_AIR);
			}
		else
			{
				if (mode == 1 && !item.empty ())
					{
						// shift clicking
						if (slot >= 9 && slot <= 35)
							{
								// first slots with same id&damage
								for (int i = 36; (i <= 44) && !item.empty (); ++i)
									{
										slot_item& s = pl->inv.get (i);
										if (s.id () != BT_AIR && s.compatible_with (item))
											{
												int take = s.max_stack () - s.amount ();
												if (take > item.amount ())
													take = item.amount ();
												
												int am = s.amount () + take;
												s = item;
												s.set_amount (am);
												item.take (take);
											}
									}
								
								// now do empty slots
								for (int i = 36; (i <= 44) && !item.empty (); ++i)
									{
										slot_item& s = pl->inv.get (i);
										if (s.id () == BT_AIR)
											{
												int take = 64;
												if (take > item.amount ())
													take = item.amount ();
												
												s = item;
												s.set_amount (take);
												item.take (take);
												//pl->inv.update_slot (i, s);
											}
									}
								
								// update item
								pl->inv.set (slot, item);
							}
						else if ((slot >= 36 && slot <= 44) || (slot >= 0 && slot <= 8))
							{
								// first slots with same id&damage
								for (int i = 9; (i <= 35) && !item.empty (); ++i)
									{
										slot_item& s = pl->inv.get (i);
										if (s.id () != BT_AIR && s.compatible_with (item))
											{
												int take = s.max_stack () - s.amount ();
												if (take > item.amount ())
													take = item.amount ();
												
												int am = s.amount () + take;
												s = item;
												s.set_amount (am);
												item.take (take);
											}
									}
								
								// now do empty slots
								for (int i = 9; (i <= 35) && !item.empty (); ++i)
									{
										slot_item& s = pl->inv.get (i);
										if (s.id () == BT_AIR)
											{
												int take = 64;
												if (take > item.amount ())
													take = item.amount ();
												
												s = item;
												s.set_amount (take);
												item.take (take);
											}
									}
								
								// update item
								pl->inv.set (slot, item);
							}
					}
				else if (mode == 2)
					{
						// pressing 1-9 while hovering over an item
						
						int dest = 36 + mbtn;
						if (dest > 44)
							return -1;
						
						pl->log (LT_DEBUG) << "Swapping between [" << slot << "] and [" << dest << "]" << std::endl;
						pl->inv.set (slot, pl->inv.get (dest));
						pl->inv.set (dest, item);
					}
				else
					{
						if (pl->cursor_slot.id () != BT_AIR)
							{
								if (mbtn == 1)
									{
										if (item.compatible_with (pl->cursor_slot) &&
											(item.amount () < item.max_stack ()))
											{
												pl->log (LT_DEBUG) << "Putting one" << std::endl;
												if (item.id () == BT_AIR)
													{
														slot_item &s = pl->inv.get (slot);
														s = pl->cursor_slot;
														s.set_amount (1);
													}
												else
													pl->inv.get (slot).give (1);
												pl->cursor_slot.take (1);
											}
									}
								else
									{
										// attempt to merge stacks
										pl->log (LT_DEBUG) << "Merging stacks" << std::endl;
					
										bool item_tool = item_info::is_tool (item.id ());
										bool cursor_tool = item_info::is_tool (pl->cursor_slot.id ());
					
										if (item.compatible_with (pl->cursor_slot))
											{
												if (!item.full ())
													{
														int room = item.max_stack () - item.amount ();
														int take = room;
														if (take > pl->cursor_slot.amount ())
															take = pl->cursor_slot.amount ();
									
														pl->cursor_slot.set_amount (pl->cursor_slot.amount () - take);
														item.set_amount (item.amount () + take);
														pl->inv.get (slot).set_amount (item.amount ());
													}
											}
										else if (!item_tool && !cursor_tool)
											{
												// swap stacks
												pl->log (LT_DEBUG) << "Swapping stacks" << std::endl;
												slot_item temp = pl->cursor_slot;
												pl->cursor_slot = item;
												pl->inv.set (slot, temp, false);
											}
									}
							}
						else
							{
								if (mbtn == 1)
									{
										// split stack
										pl->log (LT_DEBUG) << "Splitting stack" << std::endl;
							
										int rem = item.amount () % 2;
										pl->cursor_slot = item;
										item.set_amount (item.amount () / 2);
										pl->cursor_slot.set_amount (pl->cursor_slot.amount () / 2 + rem);
										pl->inv.get (slot).set_amount (item.amount ());
									}
								else
									{
										// picking up item
										pl->log (LT_DEBUG) << "Picking item up" << std::endl;
										pl->cursor_slot = item;
										pl->inv.get (slot).set_id (BT_AIR);
										pl->log (LT_DEBUG) << "  [" << slot << "] = " << pl->inv.get (slot).id () << ", [cur] = " << pl->cursor_slot.id () << std::endl;
									}
							}
					}
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
		
		pl->inv.set (index, item);
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
				handle_packet_xx, handle_packet_xx, handle_packet_xx, handle_packet_xx, // 0xFB
				handle_packet_xx, handle_packet_xx, handle_packet_fe, handle_packet_ff, // 0xFF
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

