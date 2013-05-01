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

#include "sqlops.hpp"
#include "sql.hpp"
#include "server.hpp"

#include <iostream> // DEBUG


namespace hCraft {
	
	/* 
	 * Total number of players registered in the database.
	 */
	int
	sqlops::player_count (sql::connection& conn)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT Count(*) FROM `players`");
		if (stmt.step (row))
			return row.at (0).as_int ();
		return 0;
	}
	
	/* 
	 * Checks whether the database has a player with the given name registered.
	 */
	bool
	sqlops::player_exists (sql::connection& conn, const char *name)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT Count(*) FROM `players` WHERE `name`=?");
		stmt.bind (1, name, sql::pass_transient);
		if (stmt.step (row))
			return (row.at (0).as_int () == 1);
		return false;
	}
	
	
	
	/* 
	 * Saves all information about the given player (named @{name}) stored in @{in}
	 * to the database.
	 * 
	 * NOTE: The 'id' (always) and 'name' (if overwriting) fields are ignored.
	 * 
	 * Returns true if the player already existed before the function was called.
	 */
	bool
	sqlops::save_player_data (sql::connection& conn, const char *name,
		server &srv, const player_info& in)
	{
		bool exists;
		sql::statement stmt {conn};
		if ((exists = player_exists (conn, name)))
			{
				stmt.prepare ("UPDATE `players`  SET `nick`=?, `ip`=?, `op`=?, "
					"`rank`=?, `blocks_destroyed`=?, `blocks_created`=?, "
					"`messages_sent`=?, `first_login`=?, `last_login`=?, "
					"`login_count`=?, `balance`=?   WHERE `name`=?");
			}
		else
			{
				stmt.prepare ("INSERT INTO `players` (`name`, `nick`, "
					"`ip`, `op`, `rank`, `blocks_destroyed`, `blocks_created`, "
					"`messages_sent`, `first_login`, `last_login`, "
					"`login_count`, `balance`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
			}
		
		int i = 1;
		if (!exists)
			stmt.bind (i++, name);
		stmt.bind (i++, in.nick.c_str ());
		stmt.bind (i++, in.ip.c_str ());
		stmt.bind (i++, in.op ? 1 : 0);
		
		std::string rank_str;
		in.rnk.get_string (rank_str);
		stmt.bind (i++, rank_str.c_str ());
		
		stmt.bind (i++, in.blocks_destroyed);
		stmt.bind (i++, in.blocks_created);
		stmt.bind (i++, in.messages_sent);
		stmt.bind (i++, (long long)in.first_login);
		stmt.bind (i++, (long long)in.last_login);
		stmt.bind (i++, in.login_count);
		stmt.bind (i++, in.balance);
		
		if (exists)
			stmt.bind (i++, name);
		
		sql::row row;
		while (stmt.step (row))
			;
		
		return exists;
	}
		
	/* 
	 * Fills the specified player_info structure with information about
	 * the given player. Returns true if the player found, false otherwise.
	 */
	bool
	sqlops::player_data (sql::connection& conn, const char *name,
		server &srv, sqlops::player_info& out)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT * FROM `players` WHERE `name`=?");
		stmt.bind (1, name, sql::pass_transient);
		if (stmt.step (row))
			{
				out.id = row.at (0).as_int ();
				out.name.assign (row.at (1).as_cstr ());
				out.nick.assign (row.at (2).as_cstr ());
				out.ip.assign (row.at (3).as_cstr ());
				
				out.op = (row.at (4).as_int () == 1);
				
				try
					{
						out.rnk.set (row.at (5).as_cstr (), srv.get_groups ());
					}
				catch (const std::exception& str)
					{
						// invalid rank
						out.rnk.set (srv.get_groups ().default_rank);
						srv.get_logger () (LT_ERROR) << "Player \"" << name << "\" has an invalid rank." << std::endl;
					}
				
				out.blocks_destroyed = row.at (6).as_int ();
				out.blocks_created = row.at (7).as_int ();
				out.messages_sent = row.at (8).as_int ();
				
				out.first_login = (std::time_t)row.at (9).as_int ();
				out.last_login = (std::time_t)row.at (10).as_int ();
				out.login_count = row.at (11).as_int ();
				
				out.balance = row.at (12).as_double ();
				
				return true;
			}
		return false;
	}
	
	
		
	/* 
	 * Changes the rank of the player that has the specified name.
	 */
	void
	sqlops::modify_player_rank (sql::connection& conn, const char *name,
		const char *rankstr)
	{
		auto stmt = conn.query ("UPDATE `players` SET `rank`=? WHERE `name`=?");
		stmt.bind (1, rankstr, sql::pass_transient);
		stmt.bind (2, name, sql::pass_transient);
		
		sql::row row;
		while (stmt.step (row))
			;
	}
}

