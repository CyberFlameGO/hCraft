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

#ifndef _hCraft__MESSAGES_H_
#define _hCraft__MESSAGES_H_

#include <string>


namespace hCraft {
	
	class group_manager;
	class player;
	class server;
	class world;
	
	
	struct server_messages
	{
		std::string server_join;
		std::string server_leave;
		
		std::string world_enter;     // displayed to players in the world a player is trying to enter
		std::string world_depart;    // displayed to players in the world a player is leaving
		std::string world_join;      // displayed to all other players not in either world
		std::string world_join_self; // displayed to the player who is performing the world switch
	};
	
	
	/* 
	 * A utility class that constructs messages that are used often.
	 */
	struct messages
	{
		struct environment
		{
			server &srv;
			
			player *target;
			
			world *curr_world;
			world *prev_world;
			
		//----
			
			/* 
			 * Constructs a default environment for the specified player.
			 */
			environment (player *pl);
		};
		
		/* 
		 * %target          = Player username
		 * %target-nick     = Player nickname
		 * %target-col      = Colored player username
		 * %target-col-nick = Colored player nickname
		 * 
		 * %curr-world      = Target player's current world name
		 * %curr-world-col  = Colored world name
		 * %prev-world      = Target player's previous world name
		 * %prev-world-col  = COlored previous world name
		 */
		static std::string compile (std::string input, const environment& env);
		
	//----
		static std::string insufficient_permissions (group_manager& groups,
			const char *perm);
		
		static std::string over_fill_limit (int f_limit);
		
		static std::string over_select_limit (int s_limit);
		
		static std::string not_allowed ();
	};
}

#endif

