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

#include "lighting.hpp"
#include "logger.hpp"
#include "blocks.hpp"
#include "world.hpp"

#include <utility>
#include <bitset>
#include <vector>

namespace hCraft {
	
	/* 
	 * Constructs a new lighting manager on top of the given world.
	 */
	lighting_manager::lighting_manager (logger &log, world *wr, int limit)
		: log (log)
	{
		this->wr = wr;
		this->overloaded = false;
		this->limit = limit;
		this->handled_since_empty = 0;
	}
	
	
	
	void
	lighting_manager::enqueue_nolock (int x, int y, int z)
	{
		if (this->overloaded)
			return;
	
		this->updates.emplace (x, y, z);
		if ((int)this->updates.size () >= this->limit)
			{
				this->overloaded = true;
				this->log (LT_WARNING) << "World \"" << this->wr->get_name () <<
					"\": Too many lighting updates! (>= " << this->limit << ")" << std::endl;
			}
	}
	
	/* 
	 * Pushes a lighting update to the update queue.
	 */
	void
	lighting_manager::enqueue (int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		this->enqueue_nolock (x, y, z);
	}
	
	
	
	static inline int
	_max (int a, int b)
		{ return (a < b) ? b : a; }
	
	static inline int
	_min (int a, int b)
		{ return (a > b) ? b : a; }
	
	static inline int
	_abs (int x)
		{ return (x < 0) ? (-x) : x; }
	
	
	
	static char
	calc_sky_light (lighting_manager &lm, int x, int y, int z)
	{
		world *wr = lm.get_world ();
		
		int cx = x >> 4;
		int cz = z >> 4;
		int bx = x & 15;
		int bz = z & 15;
		chunk *ch = wr->get_chunk (cx, cz);
		if (!ch) return 0;
		
		block_data this_block = ch->get_block (bx, y, bz);
		block_info *this_info = block_info::from_id (this_block.id);
		char nl;
		
		if (this_info->opacity == 15)
			{
				nl = 0;
			}
		else if ((y + 1) >= ch->get_height (bx, bz))
			{
				nl = 15 - this_info->opacity;
			}
		else
			{
				char sle = (bx < 15) ? ch->get_sky_light (bx + 1, y, bz)
														 : (ch->east ? ch->east->get_sky_light (0, y, bz) : 0);
				char slw = (bx >  0) ? ch->get_sky_light (bx - 1, y, bz)
														 : (ch->west ? ch->west->get_sky_light (15, y, bz) : 0);
				char slu = (y < 255) ? ch->get_sky_light (bx, y + 1, bz) : 15;
				char sld = (y >   0) ? ch->get_sky_light (bx, y - 1, bz) : 0;
				char sls = (bz < 15) ? ch->get_sky_light (bx, y, bz + 1)
														 : (ch->south ? ch->south->get_sky_light (bx, y, 0) : 0);
				char sln = (bz >  0) ? ch->get_sky_light (bx, y, bz - 1)
														 : (ch->north ? ch->north->get_sky_light (bx, y, 15) : 0);
				
				char brightest = _max (sle, _max (slw, _max (slu, _max (sld, _max (sls, _max (sln, 0))))));
				nl = brightest - this_info->opacity - 1;
				if (nl < 0) nl = 0;
			}
		
		if (this_block.sl != nl)
			{
				ch->set_sky_light (bx, y, bz, nl);
				
				lm.enqueue_nolock (x + 1, y, z);
				lm.enqueue_nolock (x - 1, y, z);
				lm.enqueue_nolock (x, y + 1, z);
				lm.enqueue_nolock (x, y - 1, z);
				lm.enqueue_nolock (x, y, z + 1);
				lm.enqueue_nolock (x, y, z - 1);
			}
		
		return nl;
	}
	
	
	
	/* 
	 * Goes through all queued updates and handles them (No more than
	 * @{max_updates} updates are handled).
	 * 
	 * Returns the total amount of updates handled.
	 */
	int
	lighting_manager::update (int max_updates)
	{
		int updated = 0;
		
		std::lock_guard<std::mutex> guard {this->lock};
		while (!this->updates.empty () && (updated++ < max_updates))
			{
				light_update u = this->updates.front ();
				this->updates.pop ();
				
				calc_sky_light (*this, u.x, u.y, u.z);
				++ this->handled_since_empty;
			}
		
		if (this->updates.empty () && (this->handled_since_empty > 0))
			{
				this->overloaded = false;
				this->handled_since_empty = 0;
			}
		
		return updated;
	}
}

