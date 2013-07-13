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

#ifndef _hCraft__WORLD_H_
#define _hCraft__WORLD_H_

#include "position.hpp"
#include "chunk.hpp"
#include "generation/worldgenerator.hpp"
#include "providers/worldprovider.hpp"
#include "lighting.hpp"
#include "physics/blocks/physics_block.hpp"
#include "physics/physics.hpp"
#include "editstage.hpp"

#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <deque>
#include <memory>
#include <thread>
#include <chrono>
#include <functional>


namespace hCraft {
	
	class server;
	class entity;
	class logger;
	class player;
	class playerlist;
	class world_transaction;
	
	
	/* 
	 * The whereabouts of a block that must be modified and sent to close players.
	 */
	struct block_update
	{
		int x;
		int y;
		int z;
		int extra;
		void *ptr;
		unsigned short id;
		unsigned char  meta;
		player *pl; // the player that initated the update.
		bool physics;
		
		block_update (int x, int y, int z, unsigned short id, unsigned char meta,
			int extra, void *ptr, player *pl, bool physics)
		{
			this->x = x;
			this->y = y;
			this->z = z;
			this->id = id;
			this->meta = meta;
			this->pl = pl;
			this->extra = extra;
			this->ptr = ptr;
			this->physics = physics;
		}
	};
	
	enum world_physics_state
	{
		PHY_ON,
		PHY_OFF,
		PHY_PAUSED,
	};
	
	
	
	/* 
	 * The world provides methods to easily retreive or modify chunks, and
	 * get/set individual blocks within those chunks. In addition to that,
	 * the world also manages a list of players.
	 */
	class world
	{
		server &srv;
		logger &log;
		char name[33]; // 32 chars max
		playerlist *players;
		
		std::unique_ptr<std::thread> th;
		bool th_running;
		
		std::deque<block_update> updates;
		world_physics_state ph_state;
		unsigned long long ticks;
		
		std::unordered_map<unsigned long long, chunk *> chunks;
		std::mutex chunk_lock;
		
		struct { int x, z; chunk *ch; } last_chunk;
		
		std::unordered_set<entity *> entities;
		std::mutex entity_lock;
		
		int width;
		int depth;
		entity_pos spawn_pos;
		chunk *edge_chunk;
		
		world_generator *gen;
		world_provider *prov;
		
	public:
		bool auto_lighting;
		physics_manager physics;
		lighting_manager lm;
		
		dense_edit_stage estage;
		std::mutex estage_lock;
		std::mutex update_lock;
		
	public:
		inline const char* get_name () { return this->name; }
		inline playerlist& get_players () { return *this->players; }
		
		inline world_generator* get_generator () { return this->gen; }
		inline world_provider* get_provider () { return this->prov; }
		
		inline int get_width () const { return this->width; }
		inline int get_depth () const { return this->depth; }
		void set_width (int width);
		void set_depth (int depth);
		void set_size (int width, int depth)
			{ set_width (width); set_depth (depth); }
		chunk* get_edge_chunk () const { return this->edge_chunk; }
		
		inline entity_pos get_spawn () const { return this->spawn_pos; }
		inline void set_spawn (const entity_pos& pos) { this->spawn_pos = pos; }
		
		inline world_physics_state physics_state () const { return this->ph_state; }
		inline std::mutex& get_update_lock () { return this->update_lock; }
		
	private:
		/* 
		 * The function ran by the world's thread.
		 */
		void worker ();
		
		chunk* get_chunk_nolock (int x, int z);
		
		std::unordered_set<entity *>::iterator
		despawn_entity_nolock (std::unordered_set<entity *>::iterator itr);
		
		void get_information (world_information& inf);
		
	public:
		/* 
		 * Constructs a new empty world.
		 */
		world (server &srv, const char *name, logger &log, world_generator *gen,
			world_provider *provider);
		
		/* 
		 * Class destructor.
		 */
		~world ();
		
	//----
		
		/* 
		 * Checks whether the specified string can be used to name a world.
		 */
		static bool is_valid_name (const char *wname);
		
	//----
		
		/* 
		 * Starts the world's "physics"-handling thread.
		 */
		void start ();
		
		/* 
		 * Stops the world's thread.
		 */
		void stop ();
		
		
		
		/* 
		 * Saves all modified chunks to disk.
		 */
		void save_all ();
		
		/* 
		 * Saves metadata to disk (width, depth, spawn pos, etc...).
		 */
		void save_meta ();
		
		
		
		/* 
		 * Loads up a grid of radius x radius chunks around the given point
		 * (specified in chunk coordinates).
		 */
		void load_grid (chunk_pos cpos, int radius);
		
		/* 
		 * Calls load_grid around () {x: 0, z: 0}, and attempts to find a suitable
		 * spawn position. 
		 */
		void prepare_spawn (int radius, bool calc_spawn_point = false);
		
		
		
		/* 
		 * Inserts the specified chunk into this world at the given coordinates.
		 */
		void put_chunk (int x, int z, chunk *ch);
		
		/* 
		 * Searches the chunk world for a chunk located at the specified coordinates.
		 */
		chunk* get_chunk (int x, int z);
		
		/* 
		 * Returns the chunk located at the given block coordinates.
		 */
		chunk* get_chunk_at (int bx, int bz);
		
		/* 
		 * Same as get_chunk (), but if the chunk does not exist, it will be either
		 * loaded from a file (if such a file exists), or completely generated from
		 * scratch.
		 */
		chunk* load_chunk (int x, int z);
		chunk* load_chunk_at (int bx, int bz);
		
		/* 
		 * Checks whether a block exists at the given coordinates.
		 */
		bool in_bounds (int x, int y, int z);
		bool chunk_in_bounds (int cx, int cz);
		
		
		
		/* 
		 * Spawns the specified entity into the world.
		 */
		void spawn_entity (entity *e);
		
		/* 
		 * Removes the specified etnity from this world.
		 */
		void despawn_entity (entity *e);
		
		/* 
		 * Calls the given function on all entities in the world.
		 */
		void all_entities (std::function<void (entity *e)> f);
		
		
		
		/* 
		 * Block interaction: 
		 */
		
		void set_id (int x, int y, int z, unsigned short id);
		unsigned short get_id (int x, int y, int z);
		
		void set_meta (int x, int y, int z, unsigned char val);
		unsigned char get_meta (int x, int y, int z);
		
		void set_block_light (int x, int y, int z, unsigned char val);
		unsigned char get_block_light (int x, int y, int z);
		
		void set_sky_light (int x, int y, int z, unsigned char val);
		unsigned char get_sky_light (int x, int y, int z);
		
		void set_block (int x, int y, int z, unsigned short id, unsigned char meta, unsigned char ex = 0);
		
		void set_extra (int x, int y, int z, unsigned char ex);
		unsigned char get_extra (int x, int y, int z);
		
		block_data get_block (int x, int y, int z);
		
		bool has_physics_at (int x, int y, int z);
		physics_block* get_physics_at (int x, int y, int z);
		
		/* 
		 * Instead of fetching the block from the underlying chunk, and attempt
		 * to query the edit stage is made first.
		 */
		blocki get_final_block (int x, int y, int z);
		
	//----
		
		/* 
		 * Enqueues an update that should be made to a block in this world
		 * and sent to nearby players.
		 */
		void queue_update (int x, int y, int z, unsigned short id,
			unsigned char meta = 0, int extra = 0, void *ptr = nullptr,
			player *pl = nullptr, bool physics = true);
		
		void queue_update (world_transaction *tr);
		
		void queue_lighting (int x, int y, int z)
			{ this->lm.enqueue (x, y, z); }
		void queue_lighting_nolock (int x, int y, int z)
			{ this->lm.enqueue_nolock (x, y, z); }
		
		void queue_physics (int x, int y, int z, int extra = 0,
			void *ptr = nullptr, int tick_delay = 20, physics_params *params = nullptr,
			physics_block_callback cb = nullptr);
		
		// does nothing if the block is already queued to be handled.
		void queue_physics_once (int x, int y, int z, int extra = 0,
			void *ptr = nullptr, int tick_delay = 20, physics_params *params = nullptr,
			physics_block_callback cb = nullptr);
		
		
		void start_physics ();
		void stop_physics ();
		void pause_physics ();
	};
}

#endif

