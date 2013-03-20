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

#ifndef _hCraft__BLOCK_PHYSICS_H_
#define _hCraft__BLOCK_PHYSICS_H_

#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include <bitset>
#include <chrono>
#include <unordered_map>
#include <random>
#include "position.hpp"
#include "tbb/concurrent_queue.h"


namespace hCraft {
	
	// forward decs:
	class world;
	
	
	enum physics_action_type: unsigned char {
		PA_NONE = 0xFF,
		
		PA_DISSIPATE = 0,
		PA_DROP,
	};
	
	struct physics_action {
		physics_action_type type;
		unsigned short expire;
		short val;
	};
	
	struct physics_params {
		// up to eight actions
		physics_action actions[8];
		
	//---
		physics_params ();
	};
	
	struct physics_update	{
		world *w;
		int x, y, z;
		int extra;
		
		int tick;
		physics_params params;
		
		std::chrono::steady_clock::time_point nt; // next tick
		
	//---
		physics_update () { }
		physics_update (world *w, int x, int y, int z, int extra, int tick,
			std::chrono::steady_clock::time_point nt)
			: params (), nt (nt)
		{
			this->w = w;
			this->x = x;
			this->y = y;
			this->z = z;
			this->extra = extra;
			this->tick = tick;
		}
	};
	
	
//-----
	/* 
	 * These structures are used to store block memberships in chunks.
	 */
	
	struct ph_mem_subchunk {
		std::bitset<4096> bits;
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
	
	class block_physics_manager;
	
	/* 
	 * Every worker manages its own separate block queue.
	 */
	class block_physics_worker
	{
		friend class block_physics_manager;
		
	public:
		bool paused;
		unsigned long long ticks;
		
	private:
		block_physics_manager &man;
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
		block_physics_worker (block_physics_manager &man);
		
		/* 
		 * Destructor - stops the worker thread.
		 */
		~block_physics_worker ();
	};
	
	
	
	/* 
	 * Manages a collection of block_physics_worker instances. Individual 
	 */
	class block_physics_manager
	{
		friend class block_physics_worker;
		
		std::vector<std::shared_ptr<block_physics_worker>> workers;
		std::mutex lock;
		
		std::unordered_map<world *,
			std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>>
				block_mem;
				
	public:
		tbb::concurrent_queue<physics_update> updates;
				
	protected:
		bool block_exists (world *w, int x, int y, int z);
		void add_block (world *w, int x, int y, int z);
		void remove_block (world *w, int x, int y, int z);
		
	public:
		/* 
		 * Changes the number of worker threads to utilize.
		 */
		void set_thread_count (unsigned int count);
		inline int get_thread_count ()
			{ return workers.size (); }
		
		
		/* 
		 * Queue an update to be processed by one of the workers:
		 */
		
		void queue_physics (world *w, int x, int y, int z, int extra = 0,
			int tick_delay = 20, physics_params *params = nullptr);
		
		// Queues an update only if one with the same xyz coordinates does not
		// already exist.
		void queue_physics_once (world *w, int x, int y, int z, int extra = 0,
			int tick_delay = 20, physics_params *params = nullptr);
	};
}

#endif

