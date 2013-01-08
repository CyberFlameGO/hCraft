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

#include "world_transaction.hpp"
#include "utils.hpp"
#include "world.hpp"
#include "player.hpp"
#include "packet.hpp"
#include "playerlist.hpp"
#include <cstring>
#include <bitset>


namespace hCraft {
	
	world_transaction_subchunk::world_transaction_subchunk ()
	{
		std::memset (this->ids, 0xFF, 4096);
		std::memset (this->add, 0xFF, 2048);
		std::memset (this->meta, 0, 2048);
		this->changes = 0;
	}
	
	
	
	/* 
	 * Constructs a new transaction to the modify the blocks within the given
	 * area.
	 */
	world_transaction::world_transaction (chunk_pos p1, chunk_pos p2, bool enq_physics)
	{
		this->x_start = utils::min (p1.x, p2.x);
		this->z_start = utils::min (p1.z, p2.z);
		this->x_end = utils::max (p1.x, p2.x);
		this->z_end = utils::max (p1.z, p2.z);
		
		this->cwidth = this->x_end - this->x_start + 1;
		this->cdepth = this->z_end - this->z_start + 1;
		
		this->x_start <<= 4;
		this->z_start <<= 4;
		this->x_end <<= 4;
		this->z_end <<= 4;
		
		this->chunks = new world_transaction_chunk* [this->cwidth * this->cdepth];
		for (int i = 0; i < (this->cwidth * this->cdepth); ++i)
			this->chunks[i] = nullptr;
		this->physics = enq_physics;
	}
	
	world_transaction::world_transaction (block_pos p1, block_pos p2, bool enq_physics)
		: world_transaction (chunk_pos (p1), chunk_pos (p2), enq_physics)
	{
	}
	
	// destructor
	world_transaction::~world_transaction ()
	{
		int j = this->cwidth * this->cdepth;
		for (int i = 0; i < j; ++i)
			if (this->chunks[i])
				delete this->chunks[i];
		delete[] this->chunks;
	}
	
	
	
	/* 
	 * Commit all changes made on the given world.
	 */
	void
	world_transaction::commit (world *wr)
	{
		static const int chunk_cap = 3000;
		
		for (int cx = 0; cx < this->cwidth; ++cx)
			for (int cz = 0; cz < this->cdepth; ++cz)
				{
					int rcx = cx + (this->x_start >> 4);
					int rcz = cz + (this->z_start >> 4);
					int ci  = (cx * this->cdepth) + cz;
					
					chunk *wch = wr->load_chunk (rcx, rcz);
					world_transaction_chunk *ch = this->chunks[ci];
					if (!ch) continue;
					
					std::vector<block_change_record> records;
					bool add_records = (ch->changes < chunk_cap);
					
					std::bitset<256> column_changed;
					
					for (int sy = 0; sy < 16; ++sy)
						{
							int yy = sy << 4;
							world_transaction_subchunk *sub = ch->subs[sy];
							if (sub)
								{
								
									for (int x = 0; x < 16; ++x)
										for (int z = 0; z < 16; ++z)
											for (int y = 0; y < 16; ++y)
												{
													unsigned int index = (y << 8) | (z << 4) | x;
													unsigned int half  = index >> 1;
													unsigned char meta;
													unsigned short id;
													
													id = sub->ids[index];
													if (index & 1)
														{
															meta = sub->meta[half] >> 4;
															id |= (sub->add[half] >> 4) << 8;
														}
													else
														{
															meta = sub->meta[half] & 0xF;
															id |= (sub->add[half] & 0xF) << 8;
														}
													
													if (id != 0xFFF)
														{
															column_changed.set ((z << 4) | x);
															
															if (add_records)
																{
																	block_change_record rec;
																	rec.x = x;
																	rec.z = z;
																	rec.y = yy + y;
																	rec.id = id;
																	rec.meta = meta;
																	records.push_back (rec);
																}
															
															int wx = (rcx << 4);
															int wy = yy + y;
															int wz = (rcz << 4);
															
															wch->set_id_and_meta (x, wy, z, id, meta);
															//if (wr->auto_lighting)
																wr->queue_lighting_nolock (wx, wy, wz);
															if (this->physics)
																{
																	physics_block *ph = wr->get_physics_of (id);
																	if (ph)
																		wr->queue_physics_nolock (wx, wy, wz);
																}
														}
												}
								}
						}
					
					// adjust heightmap
					for (int x = 0; x < 16; ++x)
						for (int z = 0; z < 16; ++z) 
							{
								if (column_changed.test ((z << 4) | x))
									wch->recalc_heightmap (x, z);
							}
					
					if (ch->changes >= chunk_cap)
						{
							wr->get_players ().all (
								[rcx, rcz, wch] (player *pl)
									{
										if (pl->can_see_chunk (rcx, rcz))
											pl->send (packet::make_chunk (rcx, rcz, wch));
									});
						}
					else if (ch->changes > 200)
						{
							packet *mbcp = packet::make_multi_block_change (rcx, rcz, records);
							packet *cp   = packet::make_chunk (rcx, rcz, wch);
							
							// send the smaller between the two
							if (mbcp->size < cp->size)
								{
									delete cp;
									wr->get_players ().send_to_all (mbcp);
								}
							else
								{
									delete mbcp;
									wr->get_players ().all (
										[rcx, rcz, cp] (player *pl)
											{
												if (pl->can_see_chunk (rcx, rcz))
													pl->send (new packet (*cp));
											});
									delete cp;
								}
						}
					else
						{
							wr->get_players ().send_to_all (
								packet::make_multi_block_change (rcx, rcz, records));
						}
					
				}
		
	}
	
	
	
	/* 
	 * Block modification:
	 */
	
	void
	world_transaction::set_id (int x, int y, int z, int id)
	{
		if ((x < this->x_start) || (z < this->z_start) ||
				(x >= (this->x_end + 16))  || (z >= (this->z_end + 16)))
			return;
		
		x -= this->x_start;
		z -= this->z_start;
		
		int cx = x >> 4;
		int cz = z >> 4;
		int ci = (cx * this->cdepth) + cz;
		
		int bx = x & 15;
		int by = y & 15;
		int bz = z & 15;
		
		world_transaction_chunk *ch = this->chunks[ci];
		if (!ch)
			ch = this->chunks[ci] = new world_transaction_chunk ();
		
		int sy = y >> 4;
		world_transaction_subchunk *sub = ch->subs[sy];
		if (!sub)
			sub = ch->subs[sy] = new world_transaction_subchunk ();
		
		unsigned int index = (by << 8) | (bz << 4) | bx;
		unsigned int half  = index >> 1;
		
		if (id != 0xFFF)
			{
				++ ch->changes;
				++ sub->changes;
			}
		
		int id_hi = (id >> 8) & 0xF;
		int id_lo = id & 0xFF;
		sub->ids[index] = id_lo;
		
		if (index & 1)
			{ sub->add[half] &= 0x0F; sub->add[half] |= (id_hi << 4); }
		else
			{ sub->add[half] &= 0xF0; sub->add[half] |= id_hi; }
	}
	
	void
	world_transaction::set_meta (int x, int y, int z, unsigned char meta)
	{
		if ((x < this->x_start) || (z < this->z_start) ||
				(x >= (this->x_end + 16))  || (z >= (this->z_end + 16)))
			return;
		
		x -= this->x_start;
		z -= this->z_start;
		
		int cx = x >> 4;
		int cz = z >> 4;
		int ci = (cx * this->cdepth) + cz;
		
		int bx = x & 15;
		int by = y & 15;
		int bz = z & 15;
		
		world_transaction_chunk *ch = this->chunks[ci];
		if (!ch)
			ch = this->chunks[ci] = new world_transaction_chunk ();
		
		int sy = y >> 4;
		world_transaction_subchunk *sub = ch->subs[sy];
		if (!sub)
			sub = ch->subs[sy] = new world_transaction_subchunk ();
		
		++ ch->changes;
		++ sub->changes;
		
		unsigned int index = (by << 8) | (bz << 4) | bx;
		unsigned int half  = index >> 1;
		if (index & 1)
			{ sub->meta[half] &= 0x0F; sub->meta[half] |= (meta << 4); }
		else
			{ sub->meta[half] &= 0xF0; sub->meta[half] |= meta; }
	}
	
	void
	world_transaction::set_id_and_meta (int x, int y, int z, int id,
		unsigned char meta)
	{
		if ((x < this->x_start) || (z < this->z_start) ||
				(x >= (this->x_end + 16))  || (z >= (this->z_end + 16)))
			return;
		
		x -= this->x_start;
		z -= this->z_start;
		
		int cx = x >> 4;
		int cz = z >> 4;
		int ci = (cx * this->cdepth) + cz;
		
		int bx = x & 15;
		int by = y & 15;
		int bz = z & 15;
		
		world_transaction_chunk *ch = this->chunks[ci];
		if (!ch)
			ch = this->chunks[ci] = new world_transaction_chunk ();
		
		int sy = y >> 4;
		world_transaction_subchunk *sub = ch->subs[sy];
		if (!sub)
			sub = ch->subs[sy] = new world_transaction_subchunk ();
		
		if (id != 0xFFF)
			{
				++ ch->changes;
				++ sub->changes;
			}
		
		unsigned int index = (by << 8) | (bz << 4) | bx;
		unsigned int half  = index >> 1;
		
		int id_hi = (id >> 8) & 0xF;
		int id_lo = id & 0xFF;
		sub->ids[index] = id_lo;
		
		if (index & 1)
			{
				sub->add[half] &= 0x0F; sub->add[half] |= (id_hi << 4);
				sub->meta[half] &= 0x0F; sub->meta[half] |= (meta << 4);
			}
		else
			{
				sub->add[half] &= 0xF0; sub->add[half] |= id_hi;
				sub->meta[half] &= 0xF0; sub->meta[half] |= meta;
			}
	}
}

