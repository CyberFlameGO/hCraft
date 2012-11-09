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
#include "blocks.hpp"
#include "world.hpp"
#include "utils.hpp"


namespace hCraft {
	
	/* 
	 * Constructs a new lighting manager on top of the given world.
	 */
	lighting_manager::lighting_manager (world *wr)
	{
		this->wr = wr;
	}
	
	
	
	static inline int
	_max (int a, int b)
		{ return (a < b) ? b : a; }
	
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
	
	/* 
	 * Goes through all queued updates and handles them (No more than
	 * @{max_updates} updates are handled).
	 * 
	 * Returns the total amount of updates handled.
	 */
	void
	lighting_manager::update (int max_updates)
	{
		int updated = 0;
		
		std::lock_guard<std::recursive_mutex> guard {this->lock};
		while (!this->updates.empty () && (updated++ < max_updates))
			{
				light_update u = this->updates.front ();
				this->updates.pop ();
				
				unsigned short u_id = this->wr->get_id (u.x, u.y, u.z);
				char u_sl = this->wr->get_sky_light (u.x, u.y, u.z);
				block_info *u_inf = block_info::from_id (u_id);
				
				if (!u_inf->is_solid ())
					{
						// get the sky light values of neighbouring blocks
						char sll = skylight_at (wr, u.x + 1, u.y, u.z);
						char slr = skylight_at (wr, u.x - 1, u.y, u.z);
						char slf = skylight_at (wr, u.x, u.y, u.z + 1);
						char slb = skylight_at (wr, u.x, u.y, u.z - 1);
						char slu = skylight_at (wr, u.x, u.y + 1, u.z);
						char sld = skylight_at (wr, u.x, u.y - 1, u.z);
						
						// pick the highest value
						char max_sl =
							_max (sll, _max (slr, _max (slf,
							_max (slb, _max (sld, _max (slu, 0))))));
						char new_sl = (max_sl > 0) ? (max_sl - 1) : 0;
						
						// check whether this block has direct contact with sunlight
						bool direct_sunlight = true;
						if (u.y < 255)
							{
								for (int y = u.y; y < 256; ++y)
									{
										if (block_info::from_id (this->wr->get_id (u.x, y, u.z))->is_solid ())
											{ direct_sunlight = false; break; }
									}
							}
						if (direct_sunlight)
							new_sl = 15;
						
						if (new_sl != u_sl)
							{
								this->wr->set_sky_light (u.x, u.y, u.z, new_sl);
								
								// update any inconsistent neighbouring blocks
								if ((_abs (new_sl - sll) > 1) && !block_info::from_id (
									this->wr->get_id (u.x + 1, u.y, u.z))->is_solid ())
									this->enqueue (u.x + 1, u.y, u.z);
								if ((_abs (new_sl - slr) > 1) && !block_info::from_id (
									this->wr->get_id (u.x - 1, u.y, u.z))->is_solid ())
									this->enqueue (u.x - 1, u.y, u.z);
								if ((_abs (new_sl - slf) > 1) && !block_info::from_id (
									this->wr->get_id (u.x, u.y, u.z + 1))->is_solid ())
									this->enqueue (u.x, u.y, u.z + 1);
								if ((_abs (new_sl - slb) > 1) && !block_info::from_id (
									this->wr->get_id (u.x, u.y, u.z - 1))->is_solid ())
									this->enqueue (u.x, u.y, u.z - 1);
								if (u.y < 255 && (_abs (new_sl - slu) > 1) && !block_info::from_id (
									this->wr->get_id (u.x, u.y + 1, u.z))->is_solid ())
									this->enqueue (u.x, u.y + 1, u.z);
								if (u.y > 0 && (_abs (new_sl - sld) > 1) && !block_info::from_id (
									this->wr->get_id (u.x, u.y - 1, u.z))->is_solid ())
									this->enqueue (u.x, u.y - 1, u.z);
								
								// go through the entire column again and check for more
								// inconsistencies.
								chunk *ch = this->wr->get_chunk_at (u.x, u.z);
								if (ch)
									{
										char sl, cur_sl = 0xF;
										unsigned short id;
										
										int cx = utils::div (u.x, 16);
										int cz = utils::div (u.z, 16);
										int bx = utils::mod (u.x, 16);
										int bz = utils::mod (u.z, 16);
										
										for (int y = 254; y >= 0; --y)
											{
												id = ch->get_id (bx, y, bz);
												sl = block_info::from_id (id)->opacity;
												cur_sl -= sl;
												ch->set_sky_light (bx, y, bz, (cur_sl > 0) ? cur_sl : 0);
												
												// check neighbouring blocks
												if (cur_sl > 0)
													{
														char nsll = skylight_at (wr, u.x + 1, y, u.z);
														char nslr = skylight_at (wr, u.x - 1, y, u.z);
														char nslf = skylight_at (wr, u.x, y, u.z + 1);
														char nslb = skylight_at (wr, u.x, y, u.z - 1);
														
														if ((_abs (cur_sl - nsll) > 1) && !block_info::from_id (
															this->wr->get_id (u.x + 1, y, u.z))->is_solid ())
															this->enqueue (u.x + 1, y, u.z);
														if ((_abs (cur_sl - nslr) > 1) && !block_info::from_id (
															this->wr->get_id (u.x - 1, y, u.z))->is_solid ())
															this->enqueue (u.x - 1, y, u.z);
														if ((_abs (cur_sl - nslf) > 1) && !block_info::from_id (
															this->wr->get_id (u.x, y, u.z + 1))->is_solid ())
															this->enqueue (u.x, y, u.z + 1);
														if ((_abs (cur_sl - nslb) > 1) && !block_info::from_id (
															this->wr->get_id (u.x, y, u.z - 1))->is_solid ())
															this->enqueue (u.x, y, u.z - 1);
													}
												else break;
											}
									}
							}
					}
			}
	}
	
	
	
	/* 
	 * Pushes a lighting update to the update queue.
	 */
	void
	lighting_manager::enqueue (int x, int y, int z)
	{
		std::lock_guard<std::recursive_mutex> guard {this->lock};
		this->updates.emplace (x, y, z);
	}
}

