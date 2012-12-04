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

#include <iostream> // DEBUG

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
	
	
	
	static short
	isqrt (short num)
	{
		short res = 0;
		short bit = 1 << 14;
		while (bit > num)
			bit >>= 2;
		
		while (bit != 0)
			{
				if (num >= res + bit)
					{
						num -= res + bit;
						res = (res >> 1) + bit;
					}
				else
					res >>= 1;
				bit >>= 2;
			}
		return res;
	}

	static inline unsigned int
	relative_index (int rx, int ry, int rz)
		{ return ((rz + 16) << 10) | ((ry + 16) << 5) | (rx + 16); }
	
	static void
	brightest_block_in_vicinity_do (world *wr, int rx, int ry, int rz,
		int ox, int oy, int oz, std::bitset<32768>& visited,
		std::pair<char, int>& brightest, bool& connected_to_fully_lit_block)
	{
		int nx = ox + rx;
		int ny = oy + ry;
		int nz = oz + rz;
		
		int cx = nx >> 4;
		int cz = nz >> 4;
		int bx = nx & 15;
		int bz = nz & 15;
		chunk *ch = wr->get_chunk (cx, cz);
		if (!ch) return;
		//std::cout << "<<< entering (" << nx << " " << ny << " " << nz << ") = " << relative_index (rx, ry, rz) << std::endl;
		
		int distance = 0;
		{
			int sx = rx * rx;
			int sy = ry * ry;
			int sz = rz * rz;
			distance = isqrt(sx + sy + sz);
		}
		
		//if (brightest.first == 15 && (distance > 0))
		//	return;
		if (((oy + ry) > 255) || ((oy + ry) < 0))
			return;
		
		//std::cout << "  around [" << ox << " " << oy << " " << oz << "]: [" << (ox + rx) << " " <<
		//	(oy + ry) << " " << (oz + rz) << "] ->" << std::endl;
		
		//std::cout << "  <<<<<< A" << std::endl;
		unsigned int index = relative_index (rx, ry, rz);
		if (visited.test (index)) return;
		visited.set (index, true);
		//std::cout << "  <<<<<< B" << std::endl;
		
		block_data bd_this = ch->get_block (bx, ny, bz);
		if (distance == 0) bd_this.sl = 0;
		
		//std::cout << "  this brightness: " << (bd_this.sl - distance) << " (sl: " << (int)bd_this.sl <<
		//	", distance: " << distance << ")" << std::endl;
		//std::cout << "  prev brightness: " << (brightest.first - brightest.second) << " (sl: " << (int)brightest.first <<
		//	", distance: " << brightest.second << ")" << std::endl;
			
		int prev_bright = brightest.first - brightest.second;
		int curr_bright = bd_this.sl - distance;
		if (distance > 0)
			{
				if ((curr_bright > prev_bright) ||
					((curr_bright == prev_bright) && (bd_this.sl > brightest.first)))
				{
					//std::cout << "   - new brightest!" << std::endl;
					brightest.first = bd_this.sl;
					brightest.second = distance;
				}
				
				if (bd_this.sl == 15)
					connected_to_fully_lit_block = true;
				
				if ((oy + ry + 1) >= ch->get_height (nx & 15, nz & 15))
					{
						if ((15 - distance) > (brightest.first - brightest.second))
							{
								brightest.first = 15;
								brightest.second = distance;
							}
						return;
					}
			}
		
		unsigned short id_left = (bx < 15)
			? ch->get_id (bx + 1, ny, bz)
			: (ch->east
				? ch->east->get_id (0, ny, bz)
				: 0);
		unsigned short id_right = (bx > 0)
			? ch->get_id (bx - 1, ny, bz)
			: (ch->west
				? ch->west->get_id (15, ny, bz)
				: 0);
		unsigned short id_up = (ny < 255) ? ch->get_id (bx, ny + 1, bz) : 15;
		unsigned short id_down = (ny > 0) ? ch->get_id (bx, ny - 1, bz) : 0;
		unsigned short id_forward = (bz < 15)
			? ch->get_id (bx, ny, bz + 1)
			: (ch->south
				? ch->south->get_id (bx, ny, 0)
				: 0);
		unsigned short id_back = (bx > 0)
			? ch->get_id (bx - 1, ny, bz)
			: (ch->north
				? ch->north->get_id (bx, ny, 15)
				: 0);
		
		/*
		std::cout << "   - left    +x (" << nx << " " << ny << " " << nz << "): " << bd_left.id << ", sl: " << (int)bd_left.sl << " (distance: " << (distance + 1) << ")" << std::endl;
		std::cout << "   - right   -x (" << nx << " " << ny << " " << nz << "): " << bd_right.id << ", sl: " << (int)bd_right.sl << " (distance: " << (distance + 1) << ")" << std::endl;
		std::cout << "   - up      +y (" << nx << " " << ny << " " << nz << "): " << bd_up.id << ", sl: " << (int)bd_up.sl << " (distance: " << (distance + 1) << ")" << std::endl;
		std::cout << "   - down    -y (" << nx << " " << ny << " " << nz << "): " << bd_down.id << ", sl: " << (int)bd_down.sl << " (distance: " << (distance + 1) << ")" << std::endl;
		std::cout << "   - forward +z (" << nx << " " << ny << " " << nz << "): " << bd_forward.id << ", sl: " << (int)bd_forward.sl << " (distance: " << (distance + 1) << ")" << std::endl;
		std::cout << "   - back    -z (" << nx << " " << ny << " " << nz << "): " << bd_back.id << ", sl: " << (int)bd_back.sl << " (distance: " << (distance + 1) << ")" << std::endl;
		*/
		
		// 
		// Left (+x)
		// 
		if (!block_info::from_id (id_left)->opaque)
			{
				if (distance < 15/* && (brightest.first != 15)*/)
					brightest_block_in_vicinity_do (wr, rx + 1, ry, rz, ox, oy, oz,
						visited, brightest, connected_to_fully_lit_block);
			}
		
		// 
		// Right (-x)
		// 
		if (!block_info::from_id (id_right)->opaque)
			{
				if (distance < 15/* && (brightest.first != 15)*/)
					brightest_block_in_vicinity_do (wr, rx - 1, ry, rz, ox, oy, oz,
						visited, brightest, connected_to_fully_lit_block);
			}
		
		// 
		// Up (+y)
		// 
		if ((ny < 255) && !block_info::from_id (id_up)->opaque)
			{
				if (distance < 15/* && (brightest.first != 15)*/)
					brightest_block_in_vicinity_do (wr, rx, ry + 1, rz, ox, oy, oz,
						visited, brightest, connected_to_fully_lit_block);
			}
		
		// 
		// Down (-y)
		// 
		if ((ny > 0) && !block_info::from_id (id_down)->opaque)
			{
				if (distance < 15/* && (brightest.first != 15)*/)
					brightest_block_in_vicinity_do (wr, rx, ry - 1, rz, ox, oy, oz,
						visited, brightest, connected_to_fully_lit_block);
			}
		
		// 
		// Forward (+z)
		// 
		if (!block_info::from_id (id_forward)->opaque)
			{
				if (distance < 15/* && (brightest.first != 15)*/)
					brightest_block_in_vicinity_do (wr, rx, ry, rz + 1, ox, oy, oz,
						visited, brightest, connected_to_fully_lit_block);
			}
		
		// 
		// Back (-z)
		// 
		if (!block_info::from_id (id_back)->opaque)
			{
				if (distance < 15/* && (brightest.first != 15)*/)
					{
						brightest_block_in_vicinity_do (wr, rx, ry, rz - 1, ox, oy, oz,
							visited, brightest, connected_to_fully_lit_block);
					}
			}
	}
	
	//               light distance
	static std::pair<char, int>
	brightest_block_in_vicinity (lighting_manager &lm, world *wr, int x, int y,
		int z)
	{
		std::bitset<32768> &visited = lm.visited;
		visited.reset ();
		lm.connected_to_fully_lit_block = false;
		
		//block_data this_block = wr->get_block (x, y, z);
		std::pair<char, int> brightest (0, 0);
		
		brightest_block_in_vicinity_do (wr, 0, 0, 0, x, y, z, visited, brightest,
			lm.connected_to_fully_lit_block);
		return brightest;
	}
	
	static char
	calc_sky_light (lighting_manager &lm, int x, int y, int z)
	{
		world *wr = lm.get_world ();
		//std::cout << "Checking [" << x << " " << y << " " << z << "] ->" << std::endl;
		
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
				auto brightest = brightest_block_in_vicinity (lm, wr, x, y, z);
				//std::cout << " Brightest block is " << (int)brightest.first << " (" << brightest.second << " blocks away) (F: " << (brightest.first - brightest.second) << ")" << std::endl;
				
				nl = brightest.first - brightest.second;
				if (nl < 0 || !lm.connected_to_fully_lit_block)
					nl = 0;
			}
		
		if (this_block.sl != nl)
			{
				ch->set_sky_light (bx, y, bz, nl);
				//std::cout << "  [" << x << " " << y << " " << z << "] id: " << this_block.id << ", sl: " << (int)this_block.sl << " -> " << (int)nl << "." << std::endl;
				
				//if (block_info::from_id (wr->get_id (x + 1, y, z))->opacity < 15)
					lm.enqueue_nolock (x + 1, y, z);
				//if (block_info::from_id (wr->get_id (x - 1, y, z))->opacity < 15)
					lm.enqueue_nolock (x - 1, y, z);
				//if ((y < 255) && (block_info::from_id (wr->get_id (x, y + 1, z))->opacity < 15))
					lm.enqueue_nolock (x, y + 1, z);
				//if ((y >   0) && (block_info::from_id (wr->get_id (x, y - 1, z))->opacity < 15))
					lm.enqueue_nolock (x, y - 1, z);
				//if (block_info::from_id (wr->get_id (x, y, z + 1))->opacity < 15)
					lm.enqueue_nolock (x, y, z + 1);
				//if (block_info::from_id (wr->get_id (x, y, z - 1))->opacity < 15)
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

