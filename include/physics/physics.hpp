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

#ifndef _hCraft__PHYSICS_H_
#define _hCraft__PHYSICS_H_

#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include <bitset>
#include <chrono>
#include <unordered_map>
#include <random>
#include "util/position.hpp"
#include "tbb/concurrent_queue.h"


namespace hCraft {
	
	// forward decs:
	class world;
	class entity;
	class server;
	
	
	enum physics_update_type {
		PU_BLOCK,
		PU_ENTITY,
	};
	
	
	enum physics_action_type: unsigned char {
		PA_NONE = 0xFF,
		
		PA_DISSIPATE = 0,
		PA_DROP,
		PA_FINITE,
	};
	
	struct physics_action {
		physics_action_type type;
		int expire;
		short val;
	};
	
	struct physics_params {
		// up to eight actions
		physics_action actions[8];
		
	//---
		physics_params ();
		
		/* 
		 * Constructs a physics_params object from the given string.
		 * Returns true on success, and false in case of an error.
		 */
		static bool build (const std::string& str, physics_params& out, int def_expire = 32);
	};
	
	typedef void (*physics_block_callback)(world &w, int x, int y, int z,
		int data, std::minstd_rand& rnd);
	
	struct physics_update	{
		physics_update_type type;
		int wid;
		
		union
			{
				struct
					{
						int x, y, z;
						int data;
						physics_block_callback cb;
					} blk;
				
				struct
					{
						int eid;
						bool persistent;
					}	ent;
			} data;
			
		physics_params params;
		
		int elapsed;
		int tick;
		std::chrono::steady_clock::time_point nt; // next tick
		
	//---
		physics_update () { }
		physics_update (int wid, int x, int y, int z, int data, int tick,
			std::chrono::steady_clock::time_point nt, physics_block_callback cb = nullptr);
		physics_update (int wid, int eid, bool persistent, int tick,
			std::chrono::steady_clock::time_point nt);
	};
	
	
//-----
	/* 
	 * These structures are used to store block memberships in chunks.
	 */
	
	struct ph_mem_subchunk {
		unsigned short blocks[4096];
		
	//----
		ph_mem_subchunk ();
	};
	
	struct ph_mem_chunk {
		ph_mem_subchunk *subs[16];
		
		// constructor
		ph_mem_chunk () {
			for (int i = 0; i < 16; ++i)
				subs[i] = nullptr;
		}
		
		// destructor
		~ph_mem_chunk () {
			for (int i = 0; i < 16; ++i)
				delete subs[i];
		}
	};
//-----
	
	class physics_manager;
	
	/* 
	 * Every worker runs in its own separate thread.
	 */
	class physics_worker
	{
		friend class physics_manager;
		
	public:
		bool paused;
		unsigned long long ticks;
		
	private:
		physics_manager &man;
		std::minstd_rand rnd;
		
		bool _running;
		std::thread th;
		
	private:
		/* 
		 * Where everything happens.
		 */
		void main_loop ();
		
	public:
		/* 
		 * Constructs and starts the worker thread.
		 */
		physics_worker (physics_manager &man);
		
		/* 
		 * Destructor - stops the worker thread.
		 */
		~physics_worker ();
	};
	
	
	
	/* 
	 * Manages a collection of physics_worker instances. 
	 */
	class physics_manager
	{
		friend class physics_worker;
		
		std::vector<std::shared_ptr<physics_worker>> workers;
		std::mutex lock;
		
		std::unordered_map<world *,
			std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>>
				block_mem;
				
	public:
		tbb::concurrent_queue<physics_update> updates;
		server &srv;
				
	protected:
		bool block_exists_nolock (world *w, int x, int y, int z);
		void add_block_nolock (world *w, int x, int y, int z);
		
		bool block_exists (world *w, int x, int y, int z);
		void add_block (world *w, int x, int y, int z);
		void remove_block (world *w, int x, int y, int z);
		
	public:
		physics_manager (server &srv);
		~physics_manager ();
		
		void stop ();
		
		
		/* 
		 * Changes the number of worker threads to utilize.
		 */
		void set_thread_count (unsigned int count);
		inline int get_thread_count ()
			{ return workers.size (); }
		
		
		
		/* 
		 * Queues an update to be processed by one of the workers:
		 */
		void queue_physics (world *w, int x, int y, int z, int data = 0,
			int tick_delay = 20, physics_params *params = nullptr,
			physics_block_callback cb = nullptr);
		
		/* 
		 * Queues an update only if one with the same xyz coordinates does not
		 * already exist.
		 */
		void queue_physics_once (world *w, int x, int y, int z, int data = 0,
			int tick_delay = 20, physics_params *params = nullptr,
			physics_block_callback cb = nullptr);
		
		
		/* 
		 * Queues an entity update.
		 */
		void queue_physics (world *w, int eid, bool persistent = true,
			int tick_delay = 1, physics_params *params = nullptr);
	};
}

#endif

