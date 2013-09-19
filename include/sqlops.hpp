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

#ifndef _hCraft__SQLOPS_H_
#define _hCraft__SQLOPS_H_

#include "rank.hpp"
#include "position.hpp"
#include <soci/soci.h>
#include <ctime>
#include <vector>


namespace hCraft {
	
	// forward decs
	class server;
	
	
	/* 
	 * Common SQL queries.
	 */
	struct sqlops
	{
		struct player_info
		{
			int id;
			
			std::string name;
			std::string nick;
			std::string ip;
			
			bool op;
			rank rnk;
			
			int blocks_destroyed;
			int blocks_created;
			int messages_sent;
			
			std::time_t first_login;
			std::time_t last_login;
			int login_count;
			
			double balance;
			
			bool banned;
	 	};
	 	
	 	
	 //----
	 	
		/* 
		 * Total number of players registered in the database.
		 */
		static int player_count (soci::session& sql);
		
		/* 
		 * Checks whether the database has a player with the given name registered.
		 */
		static bool player_exists (soci::session& sql, const char *name);
		
		/* 
		 * Fills the specified player_info structure with information about
		 * the given player. Returns true if the player found, false otherwise.
		 */
		static bool player_data (soci::session& sql, const char *name,
			server &srv, player_info& out);
		
		// uses PID instead
		static bool player_data (soci::session& sql, int pid,
			server &srv, player_info& out);
		
		/* 
		 * Returns the rank of the specified player.
		 */
		static rank player_rank (soci::session& sql, const char *name, server& srv);
		
		/* 
		 * Returns the database PID of the specified player, or -1 if not found.
		 */
		static int player_id (soci::session& sql, const char *name);
		
		
		
		/* 
		 * Fills the given player_info structure with default values for a player
		 * with the specified name.
		 */
		static void default_player_data (const char *name, server &srv, player_info& pd);
		
		
		
		/* 
		 * Recording bans\kicks:
		 */
		static void record_kick (soci::session& sql, int target_pid,
			int kicker_pid, const char *reason);
		
		static void record_ban (soci::session& sql, int target_pid,
			int banner_pid, const char *reason);
		static void record_unban (soci::session& sql, int target_pid,
			int unbanner_pid, const char *reason);
		static void record_ipban (soci::session& sql, const char *ip,
			int banner_pid, const char *reason);
		
		static void modify_ban_status (soci::session& sql, const char *username, bool ban);
		static bool is_banned (soci::session& sql, const char *username);
		static bool is_ip_banned (soci::session& sql, const char *ip);
		
		static void unban_ip (soci::session& sql, const char *ip);
		
		
		/* 
		 * Populates the specified PID vector with all PIDs associated with the
		 * given IP address.
		 */
		static void get_pids_from_ip (soci::session& sql, const char *ip,
			std::vector<int>& out);
		
		
		/* 
		 * Player name-related:
		 */
		static std::string player_name (soci::session& sql, const char *name);
		static std::string player_name (soci::session& sql, int pid);
		
		static std::string player_colored_name (soci::session& sql, const char *name, server &srv);
		static std::string player_nick (soci::session& sql, const char *name);
		static std::string player_colored_nick (soci::session& sql, const char *name, server &srv);
		
		
		
		/* 
		 * Saves all information about the given player (named @{name}) stored in @{in}
		 * to the database.
		 * 
		 * NOTE: The 'id' (always) and 'name' (if overwriting) fields are ignored.
		 * 
		 * Returns true if the player already existed before the function was called.
		 */
		static bool save_player_data (soci::session& sql, const char *name,
			server &srv, const player_info& in);
		
		/* 
		 * Changes the rank of the player that has the specified name.
		 */
		static void modify_player_rank (soci::session& sql, const char *name,
			const char *rankstr);
		
		
		/* 
		 * Money-related:
		 */
		static void set_money (soci::session& sql, const char *name, double amount);
		static void add_money (soci::session& sql, const char *name, double amount);
		static double get_money (soci::session& sql, const char *name);
	};
}

#endif
