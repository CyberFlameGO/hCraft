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
		this->sl_overloaded = false;
		this->bl_overloaded = false;
		this->limit = limit;
	}
	
	
	
	void
	lighting_manager::enqueue_nolock (int x, int y, int z)
	{
		this->enqueue_sl_nolock (x, y, z);
		this->enqueue_bl_nolock (x, y, z);
	}
	
	void
	lighting_manager::enqueue_sl_nolock (int x, int y, int z)
	{
		if (this->sl_overloaded)
			return;
		
		this->sl_updates.emplace (x, y, z);
		if ((int)this->sl_updates.size () >= this->limit)
			{
				this->sl_overloaded = true;
				this->log (LT_WARNING) << "World \"" << this->wr->get_name () <<
					"\": Too many (S)lighting updates! (>= " << this->limit << ")" << std::endl;
			}
	}
	
	void
	lighting_manager::enqueue_bl_nolock (int x, int y, int z)
	{
		if (this->bl_overloaded)
			return;
		
		this->bl_updates.emplace (x, y, z);
		if ((int)this->bl_updates.size () >= this->limit)
			{
				this->bl_overloaded = true;
				this->log (LT_WARNING) << "World \"" << this->wr->get_name () <<
					"\": Too many (B)lighting updates! (>= " << this->limit << ")" << std::endl;
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
				
				lm.enqueue_sl_nolock (x + 1, y, z);
				lm.enqueue_sl_nolock (x - 1, y, z);
				if (y < 255) lm.enqueue_sl_nolock (x, y + 1, z);
				if (y >   0) lm.enqueue_sl_nolock (x, y - 1, z);
				lm.enqueue_sl_nolock (x, y, z + 1);
				lm.enqueue_sl_nolock (x, y, z - 1);
			}
		
		return nl;
	}
	
	
	static char
	get_neighbour_bl (chunk *ch, int bx, int by, int bz)
	{
		if (by > 255) return 0;
		if (by <   0) return 0;
		
		if (bx > 15)
			{ bx = 0; ch = ch->east; if (!ch) return 0; }
		if (bx <  0)
			{ bx = 15; ch = ch->west; if (!ch) return 0; }
		if (bz > 15)
			{ bz = 0; ch = ch->south; if (!ch) return 0; }
		if (bz <  0)
			{ bz = 15; ch = ch->north; if (!ch) return 0; }
		
		block_data bd = ch->get_block (bx, by, bz);
		block_info *binf = block_info::from_id (bd.id);
		if (binf->opaque && (binf->luminance == 0))
			return 0;
		return bd.bl;
	}
	
	static char
	calc_block_light (lighting_manager& lm, int x, int y, int z)
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
		
		if (this_info->opaque)
			{
				nl = this_info->luminance;
			}
		else
			{
				char ble = get_neighbour_bl (ch, bx + 1, y, bz);
				char blw = get_neighbour_bl (ch, bx - 1, y, bz);
				char blu = get_neighbour_bl (ch, bx, y + 1, bz);
				char bld = get_neighbour_bl (ch, bx, y - 1, bz);
				char bls = get_neighbour_bl (ch, bx, y, bz + 1);
				char bln = get_neighbour_bl (ch, bx, y, bz - 1);
		
				char brightest = _max (ble, _max (blw, _max (blu, _max (bld, _max (bls, _max (bln, 0))))));
				nl = brightest - 1 + this_info->luminance;
				if (nl <  0) nl = 0;
				else if (nl > 15) nl = 15;
			}
		
		if (this_block.bl != nl)
			{
				ch->set_block_light (bx, y, bz, nl);
				
				lm.enqueue_bl_nolock (x + 1, y, z);
				lm.enqueue_bl_nolock (x - 1, y, z);
				if (y < 255) lm.enqueue_bl_nolock (x, y + 1, z);
				if (y >   0) lm.enqueue_bl_nolock (x, y - 1, z);
				lm.enqueue_bl_nolock (x, y, z + 1);
				lm.enqueue_bl_nolock (x, y, z - 1);
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
		std::lock_guard<std::mutex> guard {this->lock};
		
		int updated = 0;
		
		// sky light updates
		while (!this->sl_updates.empty () && (updated++ < max_updates))
			{
				light_update u = this->sl_updates.front ();
				this->sl_updates.pop ();
				
				calc_sky_light (*this, u.x, u.y, u.z);
			}
		if (this->sl_updates.empty ())
			this->sl_overloaded = false;
		
		// block light updates
		updated = 0;
		while (!this->bl_updates.empty () && (updated++ < max_updates))
			{
				light_update u = this->bl_updates.front ();
				this->bl_updates.pop ();
				
				calc_block_light (*this, u.x, u.y, u.z);
			}
		if (this->bl_updates.empty ())
			this->bl_overloaded = false;
		
		return updated;
	}
}

