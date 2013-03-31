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
#include <unordered_set>
#include <bitset>
#include <vector>


namespace hCraft {
	
	class world; // forward dec
	class player;
	
	
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
	public:
		/* 
		 * Block modification \ retrieval:
		 */
		virtual void set (int x, int y, int z, unsigned short id, unsigned char meta = 0) = 0;
		virtual blocki get (int x, int y, int z) = 0;
		
		/* 
		 * Sends all modified blocks to the specified player(s).
		 */
		void preview_to (player *pl);
		virtual void preview (std::vector<player *>& players) { };
		
		/* 
		 * Restores back all block modifications sent by preview().
		 */
		void restore_to (player *pl);
		virtual void restore (std::vector<player *>& players) { };
		
		
		/* 
		 * Clears the edit stage.
		 */
		virtual void reset () = 0;
	};
	
	
	
//------------------------------------------------------------------------------

	// 8x8x8
	struct des_microchunk
	{
		// an array of both IDs and meta values packed together in 16-bit shorts.
		// (id << 12) | meta
		unsigned short data[512];
		
	//----
		des_microchunk ();
	};
	
	// 16x16x16 (8 microchunks)
	struct des_subchunk
	{
		des_microchunk *micro[8];
	
	//----
		des_subchunk ();
		~des_subchunk ();
	};
	
	// 16x256x16 (16 subchunks)
	struct des_chunk
	{
		des_subchunk *subs[16];
		
	
	//----
		des_chunk ();
		~des_chunk ();
	};
	
	
	
	/* 
	 * The dense edit stage is more appropriate for situations that require
	 * a big amount of block modifications. This kind of edit stage stores
	 * all changes in a way very similar to how a regular chunk does.
	 */
	class dense_edit_stage: public edit_stage
	{
		world &w;
		std::unordered_map<chunk_pos, des_chunk, chunk_pos_hash> chunks;
		
	private:
		void send_to_players (std::vector<player *>& players, bool restore);
		
	public:
		dense_edit_stage (world &w);
		
		
		/* 
		 * Block modification \ retrieval:
		 */
		virtual void set (int x, int y, int z, unsigned short id, unsigned char meta = 0);
		virtual blocki get (int x, int y, int z);
		
		
		/* 
		 * Sends all modified blocks to the specified player(s).
		 */
		virtual void preview (std::vector<player *>& players);
		
		/* 
		 * Restores back all block modifications sent by preview().
		 */
		virtual void restore (std::vector<player *>& players);
		
		
		/* 
		 * Clears the edit stage.
		 */
		virtual void reset ();
	};

	
	
//------------------------------------------------------------------------------
	
	struct ses_chunk
	{
		std::unordered_map<block_pos, unsigned short, block_pos_hash> changes;
	};
	
	
	
	/* 
	 * This type of an edit stage is more suitable for cases in which a relatively
	 * small amount of blocks are going to be modified.
	 */
	class sparse_edit_stage: public edit_stage
	{
		world &w;
		std::unordered_map<chunk_pos, ses_chunk, chunk_pos_hash> chunks;
		
	private:
		void send_to_players (std::vector<player *>& players, bool restore);
		
	public:
		sparse_edit_stage (world &w);
		
		
		/* 
		 * Block modification \ retrieval:
		 */
		virtual void set (int x, int y, int z, unsigned short id, unsigned char meta = 0);
		virtual blocki get (int x, int y, int z);
		
		
		/* 
		 * Sends all modified blocks to the specified player(s).
		 */
		virtual void preview (std::vector<player *>& players);
		
		/* 
		 * Restores back all block modifications sent by preview().
		 */
		virtual void restore (std::vector<player *>& players);
		
		
		/* 
		 * Clears the edit stage.
		 */
		virtual void reset ();
	};
}

#endif

