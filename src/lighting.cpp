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
		if (this->updates.size () >= this->limit)
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
	skylight_at (world *wr, int x, int y, int z)
	{
		if (y < 0) return 0;
		if (y >= 255) return 0xF;
		
		return wr->get_sky_light (x, y, z);
	}
	
	
	
	static bool
	is_light_inconsistent (chunk *ch, int x, int y, int z)
	{
		if (x > 15) { x =  0; ch = ch->east; }
		if (x <  0) { x = 15; ch = ch->west; }
		if (z > 15) { z =  0; ch = ch->south; }
		if (z <  0) { z = 15; ch = ch->north; }
		
		block_data bd = ch->get_block (x, y, z);
		short id = bd.id;
		block_info *binf = block_info::from_id (id);
		if (binf->obscures_light)
			return false;
		
		short sl = bd.sl;
		
		// neighbouring blocks
		char sll = (x < 15) ? ch->get_sky_light (x + 1, y, z)
												: ((ch->east) ? ch->east->get_sky_light (0, y, z) : 15);
		char slr = (x >  0) ? ch->get_sky_light (x - 1, y, z)
												: ((ch->west) ? ch->west->get_sky_light (15, y, z) : 15);
		char slf = (z < 15) ? ch->get_sky_light (x, y, z + 1)
												: ((ch->south) ? ch->south->get_sky_light (x, y, 0) : 15);
		char slb = (z >  0) ? ch->get_sky_light (x, y, z - 1)
												: ((ch->north) ? ch->north->get_sky_light (x, y, 15) : 15);
		char slu = (y < 255) ? ch->get_sky_light (x, y + 1, z) : 15;
		char sld = (y >   0) ? ch->get_sky_light (x, y - 1, z) : 0;
		
		short need = sl + 1;
		if (need > 15) need = 15;
		
		return ((sll < need) || 
			 	 	  (slr < need) || 
				 	  (slf < need) || 
				 	  (slb < need) || 
					  (slu < need) || 
					  (sld < need));
	}
	
	
	
	static char
	calc_sky_light (lighting_manager &lm, int x, int y, int z)
	{
		world *wr = lm.get_world ();
		
		chunk *ch = wr->get_chunk_at (x, z);
		if (!ch) return 15;
		
		int bx = x & 0xF;
		int bz = z & 0xF;
		
		block_data u_data = wr->get_block (x, y, z);
		unsigned short u_id = u_data.id;
		char u_sl = u_data.sl;
		block_info *u_inf = block_info::from_id (u_id);
		
		// get the sky light values of neighbouring blocks
		char sll = (bx < 15) ? ch->get_sky_light (bx + 1, y, bz)
												: ((ch->east) ? ch->east->get_sky_light (0, y, bz) : 15);
		char slr = (bx >  0) ? ch->get_sky_light (bx - 1, y, bz)
												: ((ch->west) ? ch->west->get_sky_light (15, y, bz) : 15);
		char slf = (bz < 15) ? ch->get_sky_light (bx, y, bz + 1)
												: ((ch->south) ? ch->south->get_sky_light (bx, y, 0) : 15);
		char slb = (bz >  0) ? ch->get_sky_light (bx, y, bz - 1)
												: ((ch->north) ? ch->north->get_sky_light (bx, y, 15) : 15);
		char slu = (y < 255) ? ch->get_sky_light (bx, y + 1, bz) : 15;
		char sld = (y >   0) ? ch->get_sky_light (bx, y - 1, bz) : 0;
		
		// pick the highest value
		char max_sl =
			_max (sll, _max (slr, _max (slf,
			_max (slb, _max (sld, _max (slu, 0))))));
		char new_sl = max_sl - u_inf->opacity - 1;
		if (new_sl < 0) new_sl = 0;
		bool direct_sunlight = (y >= ch->get_height (bx, bz));
		if (direct_sunlight)
			new_sl = 15 - u_inf->opacity;
		
		if (new_sl != u_sl)
			{
				wr->set_sky_light (x, y, z, new_sl);
				//lm.get_logger () (LT_DEBUG) << "Setting [" << x << " " << y << " " << z << "] to " <<
				//	(int)new_sl << "." << std::endl;
				
				// update any inconsistent neighbouring blocks
				if (is_light_inconsistent (ch, bx + 1, y, bz))
					lm.enqueue_nolock (x + 1, y, z);
				if (is_light_inconsistent (ch, bx - 1, y, bz))
					lm.enqueue_nolock (x - 1, y, z);
				if (is_light_inconsistent (ch, bx, y, bz + 1))
					lm.enqueue_nolock (x, y, z + 1);
				if (is_light_inconsistent (ch, bx, y, bz - 1))
					lm.enqueue_nolock (x, y, z - 1);
				if ((y < 255) && is_light_inconsistent (ch, bx, y + 1, bz))
					lm.enqueue_nolock (x, y + 1, z);
				if ((y >   0) && is_light_inconsistent (ch, bx, y - 1, bz))
					lm.enqueue_nolock (x, y - 1, z);
				
				// go through the entire column again and check for more
				// inconsistencies.
				char sl, cur_sl = 0xF;
				unsigned short id;
				
				int cx = x >> 4;
				int cz = z >> 4;
				int bx = x & 0xF;
				int bz = z & 0xF;
				
				for (int yy = 254; yy >= 0; --yy)
					{
						id = ch->get_id (bx, yy, bz);
						sl = block_info::from_id (id)->opacity;
						cur_sl -= sl;
						ch->set_sky_light (bx, yy, bz, (cur_sl > 0) ? cur_sl : 0);
						
						// check neighbouring blocks
						if (cur_sl > 0)
							{
								if (is_light_inconsistent (ch, bx + 1, yy, bz))
									lm.enqueue_nolock (x + 1, yy, z);
								if (is_light_inconsistent (ch, bx - 1, yy, bz))
									lm.enqueue_nolock (x - 1, yy, z);
								if (is_light_inconsistent (ch, bx, yy, bz + 1))
									lm.enqueue_nolock (x, yy, z + 1);
								if (is_light_inconsistent (ch, bx, yy, bz - 1))
									lm.enqueue_nolock (x, yy, z - 1);
							}
						else break;
					}
			}
			
			return new_sl;
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
				
				unsigned short u_id = this->wr->get_id (u.x, u.y, u.z);
				char u_sl = this->wr->get_sky_light (u.x, u.y, u.z);
				block_info *u_inf = block_info::from_id (u_id);
				
				calc_sky_light (*this, u.x, u.y, u.z);
				++ this->handled_since_empty;
			}
		
		if (this->updates.empty () && (this->handled_since_empty > 0))
			{
				this->overloaded = false;
				this->log (LT_DEBUG) << "Handled " << this->handled_since_empty << " updates." << std::endl;
				this->handled_since_empty = 0;
			}
		
		return updated;
	}
}

