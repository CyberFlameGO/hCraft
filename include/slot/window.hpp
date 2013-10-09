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

#ifndef _hCraft__WINDOW_H_
#define _hCraft__WINDOW_H_

#include "slot.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <utility>


namespace hCraft {
	
	// forward decs
	class player;
	class packet;
	
	
	/* 
	 * Base class for all window implementation (inventory, chest, workbench,
	 * etc...)
	 */
	class window
	{
	protected:
		unsigned char 				w_id;
		std::string   				w_title;
		
		std::vector<player *>	w_subscribers;
		std::mutex w_lock;
		std::queue<std::pair<int, slot_item>> w_out_queue;
		std::mutex w_out_lock;
		
		std::vector<slot_item> w_slots;
		
		
	//----
		/* 
		 * Queues an update that must be sent to all subscribers.
		 */
		void enqueue (int index, const slot_item& item);
		
		/* 
		 * Sends the specified packet to all subscribed players.
		 */
		void notify (packet *pack);
		
	public:
		inline unsigned char wid () { return w_id; }
		inline const std::string& title () { return w_title; }
		inline int slot_count () { return w_slots.size (); }
		
	public:
		/* 
		 * Constructs a new window with the given ID and title.
		 */
		window (unsigned char id, const char *title, int slot_count);
		virtual ~window ();
		
		/* 
		 * Generates and returns a unique window identification number.
		 */
		static unsigned char next_id ();
		
		
		
		/* 
		 * Inserts the specified player into this window's subscriber list.
		 */
		void subscribe (player *pl);
		
		/* 
		 * Removes player @{pl} from the subscriber list.
		 */
		void unsubscribe (player *pl);
		
		/* 
		 * Removes all players from the subscriber list.
		 */
		void unsubscribe_all ();
		
		
		
		/* 
		 * Sends queued slot updates to all subscribers.
		 */
		void update ();
		
		/* 
		 * Calls either enqueue () or notify () based on @{update}.
		 */
		void update_slot (int index, const slot_item& item, bool update = true);
		
		
		
	//----
    
		/* 
		 * Sets the slot located at @{index} to @{item}.
		 */
		virtual void set (int index, const slot_item& item, bool update = true);
		
		/* 
		 * Returns the item located at the specified slot index.
		 */
		virtual slot_item& get (int index);
		
		/* 
		 * Attempts to add @{item} at empty or compatible locations.
		 * Returns the number of items NOT added due to insufficient room.
		 */
		virtual int add (const slot_item& item, bool update = true) = 0;
		
		/* 
		 * Attempts to remove items matching item @{item}.
		 * Returns the number of items removed.
		 */
		virtual int remove (const slot_item& item, bool update = true) = 0;
		
		/* 
		 * Clears out all slots in this window.
		 */
		virtual void clear ();
		
		
		
		/* 
		 * Returns a pair of two integers that describe the range of slots where
		 * the item at the specified slot should be moved to when shift clicked.
		 */
		virtual std::pair<int, int> shift_range (int slot) = 0;
		
		/* 
		 * Hotbar slot range.
		 */
		virtual std::pair<int, int> hotbar_range () = 0;
		
		/* 
		 * Returns true if players can place the given item at the specified 
		 */
		virtual bool can_place_at (int slot, slot_item& item) { return true; }
	};
	
	
	
	/* 
	 * Player inventory.
	 */
	class inventory: public window
	{
		/* 
		 * Used by add () and remove ().
		 */
		
		int try_add (int index, const slot_item& item, int left, bool update = true);
		int try_remove (int index, const slot_item& item, int left, bool update = true);
		
	public:
		/* 
		 * Constructs a new empty player inventory.
		 */
		inventory ();
		
		
		
		/* 
		 * Attempts to add @{item} at empty or compatible locations.
		 * Returns the number of items NOT added due to insufficient room.
		 */
		virtual int add (const slot_item& item, bool update = true) override;
		
		/* 
		 * Attempts to remove items matching item @{item}.
		 * Returns the number of items removed.
		 */
		virtual int remove (const slot_item& item, bool update = true) override;
		
		
		
		/* 
		 * Returns a pair of two integers that describe the range of slots where
		 * the item at the specified slot should be moved to when shift clicked.
		 */
		virtual std::pair<int, int> shift_range (int slot) override;
		
		/* 
		 * Hotbar slot range.
		 */
		virtual std::pair<int, int> hotbar_range () override;
		
		/* 
		 * Returns true if players can place the given item at the specified slot.
		 */
		virtual bool can_place_at (int slot, slot_item& item) override;
	};
}

#endif

