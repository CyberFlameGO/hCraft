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

#ifndef _hCraft__AUTHENTICATION_H_
#define _hCraft__AUTHENTICATION_H_

#include "tbb/concurrent_queue.h"
#include <thread>


namespace hCraft {

	class player;
	
	
	/* 
	 * A player authenticator that runs in its own thread.
	 */
	class authenticator
	{
		tbb::concurrent_queue<player *> q;
		std::thread *th;
		bool running;
		
	private:
		/* 
		 * Where everything happens.
		 */
		void worker ();
		
		bool auth_player (player *pl);
		
	public:
		authenticator ();
		~authenticator ();
		
		
		/* 
		 * Starts/stops the authenticator thread.
		 */
		void start ();
		void stop ();
		
		
		/* 
		 * Queues the specified player for authentication.
		 */
		void enqueue (player *pl);
	};
}

#endif

