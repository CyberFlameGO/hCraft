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

#include "system/sqlops.hpp"
#include "system/server.hpp"
#include <ctime>


namespace hCraft {
	
	/* 
	 * Total number of players registered in the database.
	 */
	int
	sqlops::player_count (soci::session& sql)
	{
		int count;
		sql << "SELECT Count(*) FROM `players`", soci::into (count);
		return count;
	}
	
	/* 
	 * Checks whether the database has a player with the given name registered.
	 */
	bool
	sqlops::player_exists (soci::session& sql, const char *name)
	{
		int count;
		sql << "SELECT Count(*) FROM `players` WHERE `name`=:n",
			soci::into (count), soci::use (std::string (name));
		return (count == 1);
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
	sqlops::save_player_data (soci::session& sql, const char *name,
		server &srv, const player_info& in)
	{
		std::string rank_str;
		in.rnk.get_string (rank_str);
		
		bool exists;
		if ((exists = player_exists (sql, name)))
			{
				sql.once <<
					"UPDATE `players` SET `nick`=:nick, "
					"`ip`=:ip, `op`=:op, `rank`=:rank, `blocks_destroyed`=:blocks_destroyed, "
					"`blocks_created`=:blocks_created, `messages_sent`=:messages_sent, "
					"`first_login`=:first_login, `last_login`=:last_login, "
					"`login_count`=:login_count, `balance`=:balance, `banned`=:banned WHERE "
					"`name`=:name",
					soci::use (in.nick),
					soci::use (in.ip),
					soci::use (in.op ? 1 : 0),
					soci::use (rank_str),
					soci::use (in.blocks_destroyed),
					soci::use (in.blocks_created),
					soci::use (in.messages_sent),
					soci::use ((long long)in.first_login),
					soci::use ((long long)in.last_login),
					soci::use (in.login_count),
					soci::use (in.balance),
					soci::use (in.banned ? 1 : 0),
					soci::use (std::string (name));
			}
		else
			{
				sql.once <<
					"INSERT INTO `players` (`name`, `nick`, "
					"`ip`, `op`, `rank`, `blocks_destroyed`, `blocks_created`, "
					"`messages_sent`, `first_login`, `last_login`, "
					"`login_count`, `balance`, `banned`) VALUES (:name, :nick, "
					":ip, :op, :rank, :blocks_destroyed, :blocks_created, :messages_sent, "
					":first_login, :last_login, :login_count, :balance, :banned)",
					soci::use (std::string (name)),
					soci::use (in.nick),
					soci::use (in.ip),
					soci::use (in.op ? 1 : 0),
					soci::use (rank_str),
					soci::use (in.blocks_destroyed),
					soci::use (in.blocks_created),
					soci::use (in.messages_sent),
					soci::use ((unsigned long long)in.first_login),
					soci::use ((unsigned long long)in.last_login),
					soci::use (in.login_count),
					soci::use (in.balance),
					soci::use (in.banned ? 1 : 0);
			}
		
		return exists;
	}
		
		
	
	static bool
	_fill_player_info (soci::row& r, server &srv, sqlops::player_info& out)
	{
		out.id = r.get<unsigned int> (0);
		out.name = r.get<std::string> (1);
		out.nick = r.get<std::string> (2);
		out.ip = r.get<std::string> (3);
		out.op = (r.get<int> (4) == 1);
		
		try
			{
				out.rnk.set (r.get<std::string> (5).c_str (), srv.get_groups ());
			}
		catch (const std::exception& str)
			{
				// invalid rank
				out.rnk.set (srv.get_groups ().default_rank);
				srv.get_logger () (LT_ERROR) << "Player \"" << out.name << "\" has an invalid rank." << std::endl;
			}
		
		out.blocks_destroyed = r.get<int> (6);
		out.blocks_created = r.get<int> (7);
		out.messages_sent = r.get<int> (8);
		
		out.first_login = (std::time_t)r.get<unsigned long long> (9);
		out.first_login = (std::time_t)r.get<unsigned long long> (10);
		out.login_count = r.get<int> (11);
		
		out.balance = r.get<double> (12);
		out.banned = (r.get<int> (13) == 1);
		return true;
	}
	
	/* 
	 * Fills the specified player_info structure with information about
	 * the given player. Returns true if the player found, false otherwise.
	 */
	bool
	sqlops::player_data (soci::session& sql, const char *name,
		server &srv, sqlops::player_info& out)
	{
		soci::row r;
		sql << "SELECT * FROM `players` WHERE `name`=:name", soci::into (r), soci::use (std::string (name));
		if (!sql.got_data ())
			return false;
			
		return _fill_player_info (r, srv, out);
	}
	
	// uses PID instead
  bool
  sqlops::player_data (soci::session& sql, int pid, server &srv,
  	sqlops::player_info& out)
  {
  	soci::row r;
		sql << "SELECT * FROM `players` WHERE `id`=" << pid, soci::into (r);
		if (!sql.got_data ())
			return false;
		
		return _fill_player_info (r, srv, out);
  }
	
	
	
	/* 
	 * Returns the rank of the specified player.
	 */
	rank
	sqlops::player_rank (soci::session& sql, const char *name, server& srv)
	{
		std::string rank_str;
		
		sql << "SELECT `rank` FROM `players` WHERE `name`=:name",
			soci::into (rank_str), soci::use (std::string (name));
		if (!sql.got_data ())
			return srv.get_groups ().default_rank;
		
		rank rnk;
		try
			{
				rnk.set (rank_str.c_str (), srv.get_groups ());
			}
		catch (const std::exception& str)
			{
				// invalid rank
				srv.get_logger () (LT_ERROR) << "Player \"" << name << "\" has an invalid rank." << std::endl;
				return srv.get_groups ().default_rank;
			}
		
		return rnk;
	}
	
	/* 
	 * Returns the database PID of the specified player, or -1 if not found.
	 */
	int
	sqlops::player_id (soci::session& sql, const char *name)
	{
		int id;
		sql << "SELECT id FROM `players` WHERE `name`='" << name << "'",
			soci::into (id);
		if (!sql.got_data ())
			return -1;
		return id;
	}
	
	
	
	/* 
	 * Fills the given player_info structure with default values for a player
	 * with the specified name.
	 */
	void
	sqlops::default_player_data (const char *name, server &srv, player_info& pd)
	{
		pd.id = -1;
		pd.name.assign (name);
		pd.nick.assign (name);
		pd.ip.assign ("0.0.0.0");
		pd.op = false;
		pd.rnk = srv.get_groups ().default_rank;
		pd.blocks_created = 0;
		pd.blocks_destroyed = 0;
		pd.messages_sent = 0;
		pd.first_login = (std::time_t)0;
		pd.last_login = (std::time_t)0;
		pd.login_count = 0;
		pd.balance = 0.0;
		pd.banned = false;
	}
	
	
	
	/* 
	 * Player name-related:
	 */
	
	std::string
	sqlops::player_name (soci::session& sql, const char *name)
	{
		std::string n;
		sql << "SELECT `name` FROM `players` WHERE `name`=:name",
			soci::into (n), soci::use (std::string (name));
		if (!sql.got_data ())
			return "";
		return n;
	}
	
	std::string
	sqlops::player_name (soci::session& sql, int pid)
	{
		std::string n;
		sql << "SELECT `name` from `players` WHERE `id`=" << pid,
			soci::into (n);
		if (!sql.got_data ())
			return "";
		return n;
	}
	
	std::string
	sqlops::player_colored_name (soci::session& sql, const char *name, server &srv)
	{
		rank rnk = player_rank (sql, name, srv);
		std::string str;
		str.append ("§");
		str.push_back (rnk.main ()->color);
		str.append (player_name (sql, name));
		return str;
	}
	
	std::string
	sqlops::player_nick (soci::session& sql, const char *name)
	{
		std::string n;
		sql << "SELECT `nick` FROM `players` WHERE `name`=:name",
			soci::into (n), soci::use (std::string (name));
		if (!sql.got_data ())
			return "";
		return n;
	}
	
	std::string
	sqlops::player_colored_nick (soci::session& sql, const char *name, server &srv)
	{
		rank rnk = player_rank (sql, name, srv);
		std::string str;
		str.append ("§");
		str.push_back (rnk.main ()->color);
		str.append (player_nick (sql, name));
		return str;
	}
	
	
		
	/* 
	 * Changes the rank of the player that has the specified name.
	 */
	void
	sqlops::modify_player_rank (soci::session& sql, const char *name,
		const char *rankstr)
	{
		sql.once << "UPDATE `players` SET `rank`='" << rankstr << "' WHERE `name`='" << name << "'";
	}
	
	
	
	/* 
	 * Money-related:
	 */
	
	void
	sqlops::set_money (soci::session& sql, const char *name, double amount)
	{
		sql.once << "UPDATE `players` SET `balance`=" << amount << " WHERE `name`='" << name << "'";
	}
	
	void
	sqlops::add_money (soci::session& sql, const char *name, double amount)
	{
		sql.once << "UPDATE `players` SET `balance`=`balance`+" << amount << " WHERE `name`='" << name << "'";
	}
	
	double
	sqlops::get_money (soci::session& sql, const char *name)
	{
		double amount;
		sql <<  "SELECT `balance` FROM `players` WHERE `name`=:n",
			soci::into (amount), soci::use (std::string (name));
		if (!sql.got_data ())
			return 0.0;
		return amount;
	}
	
	
	
	/* 
	 * Recording bans\kicks:
	 */
	
	void
	sqlops::record_kick (soci::session& sql, int target_pid, int kicker_pid,
		const char *reason)
	{
		std::time_t t = std::time (nullptr);
		sql << "INSERT INTO `kicks` (`target`, `kicker`, `reason`, "
			"`kick_time`) VALUES (:tar, :kic, :rea, :tim)",
			soci::use (target_pid), soci::use (kicker_pid), soci::use (std::string (reason)), soci::use ((long long)t);
	}
	
	void
	sqlops::record_ban (soci::session& sql, int target_pid, int banner_pid,
		const char *reason)
	{
		std::time_t t = std::time (nullptr);
		sql << "INSERT INTO `bans` (`target`, `banner`, `reason`, "
			"`ban_time`) VALUES (:tar, :ban, :rea, :tim)",
			soci::use (target_pid), soci::use (banner_pid), soci::use (std::string (reason)), soci::use ((long long)t);
	}
	
	void
	sqlops::record_unban (soci::session& sql, int target_pid, int unbanner_pid,
		const char *reason)
	{
		std::time_t t = std::time (nullptr);
		sql << "INSERT INTO `unbans` (`target`, `unbanner`, `reason`, "
			"`unban_time`) VALUES (:tar, :unb, :rea, :tim)",
			soci::use (target_pid), soci::use (unbanner_pid), soci::use (std::string (reason)), soci::use ((long long)t);
	}
	
	void
	sqlops::record_ipban (soci::session& sql, const char *ip,
		int banner_pid, const char *reason)
	{
		std::time_t t = std::time (nullptr);
		sql << "INSERT INTO `ip-bans` (`ip`, `banner`, `reason`, "
			"`ban_time`) VALUES (:ip, :ban, :rea, :tim)",
			soci::use (std::string (ip)), soci::use (banner_pid), soci::use (std::string (reason)), soci::use ((long long)t);
	}
	
	void
	sqlops::modify_ban_status (soci::session& sql, const char *username, bool ban)
	{
		sql.once << "UPDATE `players` SET `banned`=" << (ban ? 1 : 0) << " WHERE `name`='" << username << "'";
	}
	
	bool
	sqlops::is_banned (soci::session& sql, const char *username)
	{
		int banned;
		sql << "SELECT `banned` FROM `players` WHERE `name`=:n",
			soci::into (banned), soci::use (std::string (username));
		if (!sql.got_data ())
			return false;
		return (banned == 1);
	}
	
	bool
	sqlops::is_ip_banned (soci::session& sql, const char *ip)
	{
		int c;
		sql << "SELECT Count(*) FROM `ip-bans` WHERE `ip`=:ip",
			soci::into (c), soci::use (std::string (ip));
		if (!sql.got_data ())
			return false;
		return (c > 0);
	}
	
	void
	sqlops::unban_ip (soci::session& sql, const char *ip)
	{
		sql.once << "DELETE FROM `ip-bans` WHERE `ip`=:ip",
			soci::use (std::string (ip));
	}
	
	
	
	/* 
	 * Populates the specified PID vector with all PIDs associated with the
	 * given IP address.
	 */
	void
	sqlops::get_pids_from_ip (soci::session& sql, const char *ip,
		std::vector<int>& out)
	{
		soci::rowset<int> rs = (sql.prepare
			<< "SELECT `id` FROM `players` WHERE `ip`=:ip",
			soci::use (std::string (ip)));
		for (auto itr = rs.begin (); itr != rs.end (); ++itr)
			out.push_back (*itr);
	}
}

