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

#ifndef _hCraft__EDIT_STAGE_H_
#define _hCraft__EDIT_STAGE_H_

#include "position.hpp"
#include "blocks.hpp"
#include <unordered_map>


namespace hCraft {
	
	class world; // forward dec
	
	
	// 8x8x8
	struct es_microchunk
	{
		// an array of both IDs and meta values packed together in 16-bit shorts.
		// (id << 12) | meta
		unsigned short data[512];
		
	//----
		es_microchunk ();
	};
	
	// 16x16x16 (8 microchunks)
	struct es_subchunk
	{
		es_microchunk *micro[8];
	
	//----
		es_subchunk ();
		~es_subchunk ();
	};
	
	// 16x256x16 (16 subchunks)
	struct es_chunk
	{
		es_subchunk *subs[16];
		
	
	//----
		es_chunk ();
		~es_chunk ();
	};
	
	/* 
	 * When block modification updates are queued to be handled by a world,
	 * they are also committed to the world's "edit stage", which can be thought
	 * of as a "snapshot" of the world's latest state _plus_ the updates that are
	 * yet to be handled.
	 * 
	 * There are multiple reasons for keeping an edit stage.
	 * The primary reason for this is to insure consistency in the handling of
	 * physics updates. Since updates that get queued do NOT immediately modify
	 * the state of the world, querying information about a certain block might
	 * return false data. As a solution to this problem, the edit stage can be
	 * queried instead to obtian the required and _correct_ information.
	 */
	class edit_stage
	{
		world &w;
		std::unordered_map<chunk_pos, es_chunk, chunk_pos_hash> chunks;
		
	public:
		edit_stage (world &w);
		
		
		/* 
		 * Block modification \ retrieval:
		 */
		void set (int x, int y, int z, unsigned short id, unsigned char meta);
		blocki get (int x, int y, int z);
		
		/* 
		 * Clears the edit stage.
		 */
		void reset ();
	};
}

#endif

