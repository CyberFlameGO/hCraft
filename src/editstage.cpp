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

#include "editstage.hpp"
#include "world.hpp"
#include <cstring>


namespace hCraft {
	
	es_microchunk::es_microchunk ()
	{
		std::memset (this->data, 0xFF, 512 * sizeof (unsigned short));
	}
	
	
	
	es_subchunk::es_subchunk ()
	{
		for (int i = 0; i < 8; ++i)
			this->micro[i] = nullptr;
	}
	
	es_subchunk::~es_subchunk ()
	{
		for (int i = 0; i < 8; ++i)
			delete this->micro[i];
	}
	
	
	
	es_chunk::es_chunk ()
	{
		for (int i = 0; i < 16; ++i)
			this->subs[i] = nullptr;
	}
	
	es_chunk::~es_chunk ()
	{
		for (int i = 0; i < 16; ++i)
			delete this->subs[i];
	}
	
	
	
//----
	
	edit_stage::edit_stage (world &w)
		: w (w)
		{ }
	
	
	
	/* 
	 * Block modification \ retreival:
	 */
	
	void
	edit_stage::set (int x, int y, int z, unsigned short id, unsigned char meta)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		es_chunk &ch = this->chunks[{cx, cz}];
		
		int sy = y >> 4;
		es_subchunk *sub = ch.subs[sy];
		if (!sub)
			sub = ch.subs[sy] = new es_subchunk ();
		
		int bx = x & 0xF;
		int by = y & 0xF;
		int bz = z & 0xF;
		int m_index = ((by >> 3) << 2) | ((bz >> 3) << 1) | ((bx >> 3));
		es_microchunk *micro = sub->micro[m_index];
		if (!micro)
			micro = sub->micro[m_index] = new es_microchunk ();
		
		int b_index = ((y & 0x7) << 6) | ((z & 0x7) << 3) | ((x & 0x7));
		micro->data[b_index] = (id << 4) | (meta & 0xF);
		}
	
	blocki
	edit_stage::get (int x, int y, int z)
	{
		int cx = x >> 4;
		int cz = z >> 4;
		
		es_chunk *ch;
		auto itr = this->chunks.find ({cx, cz});
		if (itr == this->chunks.end ())
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		else
			ch = &itr->second;
		
		int sy = y >> 4;
		es_subchunk *sub = ch->subs[sy];
		if (!sub)
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		int bx = x & 0xF;
		int by = y & 0xF;
		int bz = z & 0xF;
		int m_index = ((by >> 3) << 2) | ((bz >> 3) << 1) | ((bx >> 3));
		es_microchunk *micro = sub->micro[m_index];
		if (!micro)
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		int b_index = ((y & 0x7) << 6) | ((z & 0x7) << 3) | ((x & 0x7));
		unsigned short val = micro->data[b_index];
		if (val == 0xFFFF)
			{
				block_data bd = this->w.get_block (x, y, z);
				return {bd.id, bd.meta};
			}
		
		return {(unsigned short)(val >> 4), (unsigned char)(val & 0xF)};
	}
	
	
	
	/* 
	 * Clears the edit stage.
	 */
	void
	edit_stage::reset ()
	{
		this->chunks.clear ();
	}
}

