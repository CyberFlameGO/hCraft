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

#ifndef _hCraft__EDIT_STAGE_H_
#define _hCraft__EDIT_STAGE_H_

#include "util/position.hpp"
#include "slot/blocks.hpp"
#include <unordered_map>
#include <unordered_set>
#include <bitset>
#include <vector>


namespace hCraft {
	
	class world; // forward dec
	class player;
	class chunk;
	
	
	#define ES_NONE	0xFFF
	#define ES_REM  0xFFE
	
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
	protected:
		world *w;
		
	public:
		edit_stage (world *w = nullptr)
			: w (w)
			{ }
		virtual ~edit_stage () { }
		
		
		// @{w} can be null
		virtual void set_world (world *w, bool reset = true);
		world* get_world () { return this->w; }
		
		
		/*   
		 * Block modification \ retrieval:
		 */
		virtual void set (int x, int y, int z, unsigned short id, unsigned char meta = 0, unsigned char ex = 0) = 0;
		virtual blocki get (int x, int y, int z) = 0;
		virtual void reset (int x, int y, int z) = 0;
		
		virtual int mod_count_at (int cx, int cz) { return 1; }
		
		
		/* 
		 * Sends all modified blocks to the specified player(s).
		 */
		void preview_to (player *pl, bool update_sbs = true);
		virtual void preview (std::vector<player *>& players, bool update_sbs = true) { }
		
		/* 
		 * Same as preview (), but only sends a single chunk.
		 */
		void preview_chunk_to (player *pl, int cx, int cz, bool update_sbs = true);
		virtual void preview_chunk (std::vector<player *>& players, int cx, int cz, bool update_sbs = true) { }
		
		/* 
		 * Restores back all block modifications sent by preview().
		 */
		void restore_to (player *pl, bool update_sbs = true);
		virtual void restore (std::vector<player *>& players, bool update_sbs = true) { }
		
		
		/* 
		 * Commits all block modifications to the underlying world.
		 * The edit stage is then cleared.
		 */
		virtual void commit (bool physics = true) = 0;
		
		/* 
		 * Does not notify players, nor does this activate physics blocks.
		 */
		virtual void commit_chunk (chunk *ch, int cx, int cz) = 0;
		
		
		/* 
		 * Clears the edit stage.
		 */
		virtual void clear () = 0;
	};
	
	
	
//------------------------------------------------------------------------------

	// 8x8x8
	struct des_microchunk
	{
		// an array of both IDs and meta values packed together in 16-bit shorts.
		// (id << 12) | meta
		unsigned short data[512];
		unsigned char  ex[512];
		
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
		
		// the total number of blocks modified (out of 65,536).
		int mod_count;
	
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
		std::unordered_map<chunk_pos, des_chunk, chunk_pos_hash> chunks;
		
	private:
		void send_to_players (std::vector<player *>& players,
			int cx, int cz, des_chunk& ch, bool restore, bool update_sbs);
		
	public:
		dense_edit_stage (world *w = nullptr);
		
		
		/* 
		 * Block modification \ retrieval:
		 */
		virtual void set (int x, int y, int z, unsigned short id, unsigned char meta = 0, unsigned char ex = 0) override;
		virtual blocki get (int x, int y, int z) override;
		virtual void reset (int x, int y, int z) override;
		
		virtual int mod_count_at (int cx, int cz) override;
		
		
		/* 
		 * Sends all modified blocks to the specified player(s).
		 */
		virtual void preview (std::vector<player *>& players, bool update_sbs = true) override;
		
		/* 
		 * Same as preview (), but only sends a single chunk.
		 */
		virtual void preview_chunk (std::vector<player *>& players, int cx, int cz, bool update_sbs = true) override;
		
		/* 
		 * Restores back all block modifications sent by preview().
		 */
		virtual void restore (std::vector<player *>& players, bool update_sbs = true) override;
		
		
		/* 
		 * Commits all block modifications to the underlying world.
		 * The edit stage is then cleared.
		 */
		virtual void commit (bool physics = true) override;
		
		/* 
		 * Does not notify players, nor does this activate physics blocks.
		 */
		virtual void commit_chunk (chunk *ch, int cx, int cz) override;
		
		
		/* 
		 * Clears the edit stage.
		 */
		virtual void clear () override;
	};

	
	
//------------------------------------------------------------------------------
	
	struct ses_chunk
	{
		std::unordered_map<block_pos, unsigned int, block_pos_hash> changes;
	};
	
	
	
	/* 
	 * This type of an edit stage is more suitable for cases in which a relatively
	 * small amount of blocks are going to be modified.
	 */
	class sparse_edit_stage: public edit_stage
	{
		std::unordered_map<chunk_pos, ses_chunk, chunk_pos_hash> chunks;
		
	private:
		void send_to_players (std::vector<player *>& players,
			int cx, int cz, ses_chunk& ch, bool restore, bool update_sbs);
		
	public:
		sparse_edit_stage (world *w = nullptr);
		
		
		/* 
		 * Block modification \ retrieval:
		 */
		virtual void set (int x, int y, int z, unsigned short id, unsigned char meta = 0, unsigned char ex = 0) override;
		virtual blocki get (int x, int y, int z) override;
		virtual void reset (int x, int y, int z) override;
		
		virtual int mod_count_at (int cx, int cz) override;
		
		
		/* 
		 * Sends all modified blocks to the specified player(s).
		 */
		virtual void preview (std::vector<player *>& players, bool update_sbs = true) override;
		
		/* 
		 * Same as preview (), but only sends a single chunk.
		 */
		virtual void preview_chunk (std::vector<player *>& players, int cx, int cz, bool update_sbs = true) override;
		
		/* 
		 * Restores back all block modifications sent by preview().
		 */
		virtual void restore (std::vector<player *>& players, bool update_sbs = true) override;
		
		
		/* 
		 * Commits all block modifications to the underlying world.
		 * The edit stage is then cleared.
		 */
		virtual void commit (bool physics = true) override;
		
		/* 
		 * Does not notify players, nor does this activate physics blocks.
		 */
		virtual void commit_chunk (chunk *ch, int cx, int cz) override;
		
		
		/* 
		 * Clears the edit stage.
		 */
		virtual void clear () override;
	};
	
	
	
//------------------------------------------------------------------------------
	
	/* 
	 * This type of edit stage serves as a bare wrapper around a world, and should
	 * used mainly as an interface between a drawops instance and a world.
	 */
	class direct_edit_stage: public edit_stage
	{
		bool queue_updates;
		
	public:
		/* 
		 * @{queue_updates} determins the set of methods that will be used to set\get
		 * blocks. If it's true, then all block modifications will be sent to the
		 * world's queue_update () method, otherwise, set_id_and_meta () will be used
		 * instead. The same applies for block retrieval - if @{queue_updates} is
		 * true, get_final_block(), otherwise, get_block().
		 */
		direct_edit_stage (world *w, bool queue_updates = true);
		
		
		/* 
		 * Block modification \ retrieval:
		 */
		virtual void set (int x, int y, int z, unsigned short id, unsigned char meta = 0, unsigned char ex = 0) override;
		virtual blocki get (int x, int y, int z) override;
		virtual void reset (int x, int y, int z) { }
	};
}

#endif

