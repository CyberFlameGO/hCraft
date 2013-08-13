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

#include "block_history.hpp"
#include "server.hpp"
#include "world.hpp"
#include <algorithm>


namespace hCraft {
	
	/* 
	 * Constructs a new block history manager for the specified server.
	 */
	block_history_manager::block_history_manager (world &w, int cache_size)
		: w (w)
	{
		this->cache.reserve (cache_size);
		this->cache_limit = cache_size;
	}
	
	/* 
	 * Calls flush ()
	 */
	block_history_manager::~block_history_manager ()
	{
		this->flush ();
	}
	
	
	
	/* 
	 * Records block modification.
	 */
	void
	block_history_manager::insert (int x, int y, int z, blocki oldt,
		blocki newt, player *pl)
	{
		std::lock_guard<std::mutex> guard {this->update_lock};
		this->cache.push_back ({x, (unsigned char)y, z, oldt, newt, pl->pid (), std::time (nullptr)});
		if (this->cache.size () >= (size_t)this->cache_limit)
			{
				this->flush_nolock ();
			}
	}
	
	
	
	void
	block_history_manager::flush_nolock ()
	{
		if (this->cache.empty ())
			return;
		
		soci::session sql (this->w.get_server ().sql_pool ());
		soci::transaction tr (sql);
		
		for (block_history_record& rec : this->cache)
			{
				sql << "INSERT INTO `block_history_" << this->w.get_name () << "` "
					"VALUES (:x, :y, :z, :oid, :ometa, :oex, :nid, :nmeta, :nex, :pid, :tm)",
					soci::use (rec.x),
					soci::use ((int)rec.y),
					soci::use (rec.z),
					
					soci::use (rec.oldt.id),
					soci::use (rec.oldt.meta),
					soci::use (rec.oldt.ex),
					soci::use (rec.newt.id),
					soci::use (rec.newt.meta),
					soci::use (rec.newt.ex),
					
					soci::use (rec.pid),
					soci::use ((unsigned long long)rec.tm);
			}
		
		tr.commit ();
		this->cache.clear ();
	}
	
	/* 
	 * Pushes all changes stored in cache to the SQL database.
	 */
	void
	block_history_manager::flush ()
	{
		std::lock_guard<std::mutex> guard {this->update_lock};
		this->flush_nolock ();
	}
	
	
	
	/* 
	 * Gets all block modification records that have the given coordinates.
	 * The results are stored in @{out}.
	 */
	void
	block_history_manager::get (int x, int y, int z, block_history& out)
	{
		std::lock_guard<std::mutex> guard {this->update_lock};
		
		// try cache first
		for (block_history_record& rec : this->cache)
			{
				if (rec.x == x && rec.y == y && rec.z == z)
					out.push_back (rec);
			}
		
		// and from the database
		soci::session sql {this->w.get_server ().sql_pool ()};
		soci::rowset<soci::row> rs =
			(sql.prepare << "SELECT * FROM `block_history_"
				<< this->w.get_name () << "` WHERE `x`=" << x << " AND `y`=" << y
				<< " AND `z`=" << z);
		for (soci::rowset<soci::row>::const_iterator itr = rs.begin (); itr != rs.end (); ++itr)
			{
				const soci::row& r = *itr;
				
				block_history_record rec;
				rec.x = x;
				rec.y = y;
				rec.z = z;
				rec.oldt.set (r.get<int> (3), r.get<int> (4), r.get<int> (5));
				rec.newt.set (r.get<int> (6), r.get<int> (7), r.get<int> (8));
				rec.pid = r.get<unsigned int> (9);
				rec.tm = (std::time_t)r.get<unsigned long long> (10);
				out.push_back (rec);
			}
		
		// sort
		std::sort (out.begin (), out.end (),
			[] (const block_history_record& a, const block_history_record& b) -> bool
				{
					return a.tm < b.tm;
				});
	}
}

