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

#include "slot/window.hpp"
#include "player/player.hpp"
#include "system/packet.hpp"
#include "slot/items.hpp"
#include <mutex>
#include <algorithm>


namespace hCraft {
	
	/* 
	 * Constructs a new window with the given ID and title.
	 */
	window::window (window_type type, unsigned char id, const char *title,
		int slot_count)
		: w_title (title), w_slots (slot_count)
	{
		this->w_id = id;
		this->w_type = type;
	}
	
	// destructor
	window::~window ()
	{
	}
	
	
	
	/* 
	 * Generates and returns a unique window identification number.
	 */
	unsigned char
	window::next_id ()
	{
		static unsigned char curr_id = 1;
		static std::mutex    id_lock;
		
		unsigned char id;
		{
			std::lock_guard<std::mutex> guard {id_lock};
			id = curr_id ++;
			if (curr_id > 127)
				curr_id = 1;
		}
		
		return id;
	}
	
	
	
	/* 
	 * Inserts the specified player into this window's subscriber list.
	 */
	void
	window::subscribe (player *pl)
	{
		std::lock_guard<std::mutex> guard {this->w_lock};
		if (std::find (this->w_subscribers.begin (), this->w_subscribers.end (),
			pl) != this->w_subscribers.end ())
			return; // already subscribed.
		
		this->w_subscribers.push_back (pl);
	}
	
	/* 
	 * Removes player @{pl} from the subscriber list.
	 */
	void
	window::unsubscribe (player *pl)
	{
		std::lock_guard<std::mutex> guard {this->w_lock};
		this->w_subscribers.erase (
			std::remove (this->w_subscribers.begin (), this->w_subscribers.end (),
				pl), this->w_subscribers.end ());
	}
	
	/* 
	 * Removes all players from the subscriber list.
	 */
	void
	window::unsubscribe_all ()
	{
		std::lock_guard<std::mutex> guard {this->w_lock};
		this->w_subscribers.clear ();
	}
	
	
	
	/* 
	 * Sends the specified packet to all subscribed players.
	 */
	void
	window::notify (packet *pack)
	{
		bool pack_released = false;
		
		std::lock_guard<std::mutex> guard {this->w_lock};
		for (auto itr  = std::begin (this->w_subscribers);
							itr != std::end (this->w_subscribers);
							++itr)
			{
				player *pl = *itr;
				
				bool is_last = (itr + 1) == std::end (this->w_subscribers);
				if (is_last)
					{
						// this is the last player, send the packet directly, do not copy it.
						pl->send (pack);
						pack_released = true;
					}
				else
					pl->send (new packet (*pack));
			}
		
		if (!pack_released)
			delete pack;
	}
	
	/* 
	 * Queues an update that must be sent to all subscribers.
	 */
	void
	window::enqueue (int index, const slot_item& item)
	{
		std::lock_guard<std::mutex> guard {this->w_out_lock};
		this->w_out_queue.push (std::make_pair (index, item));
	}
	
	/* 
	 * Calls either enqueue () or notify () based on @{update}.
	 */
	void
	window::update_slot (int index, const slot_item& item, bool update)
	{
		if (update)
			{
				packet *pack = packets::play::make_set_slot (this->wid (), index, item);
				this->notify (pack);
			}
		else
			this->enqueue (index, item);
	}
	
	/* 
	 * Sends queued slot updates to all subscribers.
	 */
	void
	window::update ()
	{
		std::lock_guard<std::mutex> guard {this->w_out_lock};
		if (this->w_out_queue.size () > 2)
			{
				// sending the entire inventory will we be faster in this case.
				this->notify (packets::play::make_window_items (0, this->w_slots));
				while (!this->w_out_queue.empty ())
					this->w_out_queue.pop ();
			}
		else
			{
				while (!this->w_out_queue.empty ())
					{
						std::pair<int, slot_item>& p = this->w_out_queue.front ();
						this->notify (packets::play::make_set_slot (0, p.first, p.second));
						this->w_out_queue.pop ();
					}
			}
	}
	
	/* 
	 * Shows this window to the specified player.
	 */
	void
	window::show (player *pl)
	{
		pl->send (packets::play::make_open_window (
			this->w_id, this->w_type, this->w_title.c_str (),
			this->w_slots.size (), true));
	}
	
	
	
	/* 
	 * Sets the slot located at @{index} to @{item}.
	 */
	void
	window::set (int index, const slot_item& item, bool update)
	{
		if (index > 44 || index < 0)
			return;
		
		slot_item& curr = this->w_slots[index];
		if (!curr.is_valid ())
			curr.set (BT_AIR, 0, 0);
		else if (curr.amount () > 64)
			curr.set_amount (64);
		
		if (curr == item)
			return;
		curr = item;
		if (curr.id () == BT_UNKNOWN)
			curr.set_id (BT_AIR);
		
		this->update_slot (index, item, update);
	}
	
	/* 
	 * Returns the item located at the specified slot index.
	 */
	slot_item&
	window::get (int index)
	{
		static slot_item invalid_slot (BT_UNKNOWN, 0, 0);
		if (index > 44 || index < 0)
			return invalid_slot;
		return this->w_slots[index];
	}
	
	/* 
	 * Clears out all slots in this window.
	 */
	void
	window::clear ()
	{
		for (int i = 0; i < 45; ++i)
			{
				slot_item& item = this->w_slots[i];
				if (!item.empty ())
					{
						item.clear ();
						this->notify (packets::play::make_set_slot (this->wid (), i, item));
					}
			}
	}
	
	
	
//------------------------------------------------------------------------------
/* 
 * Player inventory:
 */
	
	/* 
	 * Constructs a new empty player inventory.
	 */
	inventory::inventory ()
		: window (WT_INVENTORY, 0, "Inventory", 45)
		{ }
	
	
	
	int
	inventory::try_add (int index, const slot_item& item, int left, bool update)
	{
		slot_item &curr_item = this->w_slots[index];
		if (curr_item.empty ())
			{
				int take = (left > 64) ? 64 : left;
				curr_item = item;
				curr_item.set_amount (take);
				this->update_slot (index, curr_item, update);
				return take;
			}
		
		// check if compatible
		if (curr_item.id () != item.id () || curr_item.damage () != item.damage ())
			return 0;
		if (curr_item.full ()) return 0;
		
		int take = 64 - curr_item.amount ();
		if (take > item.amount ())
			take = item.amount ();
		int am = take + curr_item.amount ();
		
		curr_item = item;
		curr_item.set_amount (am);
		this->update_slot (index, curr_item, update);
		return take;
	}
	
	/* 
	 * Attempts to add @{item} at empty or compatible locations.
	 * Returns the number of items NOT added due to insufficient room.
	 */
	int
	inventory::add (const slot_item& item, bool update)
	{
		int i, added;
		int left = item.amount ();
		
		// hotbar first
		for (i = 36; i <= 44 && left > 0; ++i)
			{
				added = this->try_add (i, item, left, update);
				left -= added;
			}
		
		// and the rest of the inventory
		for (i = 9; i <= 35 && left > 0; ++i)
			{
				added = this->try_add (i, item, left, update);
				left -= added;
			}
		
		return left;
	}
	
	
	
	int
	inventory::try_remove (int index, const slot_item& item, int left, bool update)
	{
		slot_item &curr_item = this->w_slots[index];
		if (curr_item.empty ())
			return 0;
		if (curr_item.id () != item.id () || curr_item.damage () != item.damage ())
			return 0;
		
		int take = curr_item.amount ();
		if (take > left)
			take = left;
		int am = curr_item.amount () - take;
		
		curr_item = item;
		curr_item.set_amount (am);
		this->update_slot (index, curr_item, update);
		return take;
	}
	
	/* 
	 * Attempts to remove items matching item @{item}.
	 * Returns the number of items removed.
	 */
	int
	inventory::remove (const slot_item& item, bool update)
	{
		int i, removed, taken;
		int left = item.amount ();
		
		removed = 0;
		for (i = 9; i <= 44 && left > 0; ++i)
			{
				taken    = this->try_remove (i, item, left, update);
				left    -= taken;
				removed += taken;
			}
		
		return removed;
	}
	
	
	
	/* 
	 * Returns a pair of two integers that describe the range of slots where
	 * the item at the specified slot should be moved to when shift clicked.
	 */
	std::pair<int, int>
	inventory::shift_range (int slot)
	{
		if (slot >= 36 && slot <= 44)
			return std::make_pair (9, 35);
		else if (slot >= 9 && slot <= 35)
			return std::make_pair (36, 44);
		else if (slot >= 0 && slot <= 4)
			return std::make_pair (44, 9);
		else if (slot >= 5 && slot <= 8)
			return std::make_pair (9, 44);
		return std::make_pair (-1, -1);
	}
	
	/* 
	 * Hotbar slot range
	 */
	std::pair<int, int>
	inventory::hotbar_range ()
	{
		return std::make_pair (36, 44);
	}
	
	/* 
	 * Returns true if players can place the given item at the specified.
	 */
	bool
	inventory::can_place_at (int slot, slot_item& item)
	{
		if (slot < 0 || slot > 44)
			return false;
		
		// crafting slot
		if (slot == 0)
			return false;
		
		if (slot >= 5 && slot <= 8)
			{
				// armor slots
				if (!item_info::is_armor (item.id ()))
					return false;
			}
		
		return true;
	}
	
	
	
//------------------------------------------------------------------------------
	
	/* 
	 * Constructs a new workbench ontop of the specifeid inventory window.
	 */
	workbench::workbench (inventory& inv, unsigned char id)
		: window (WT_WORKBENCH, id, "Workbench", 10),
			inv (inv)
	{
	}
	
	
	
	/* 
	 * Sets the slot located at @{index} to @{item}.
	 */
	void
	workbench::set (int index, const slot_item& item, bool update)
	{
		if (index >= 10 && index <= 45)
			this->inv.set (index - 1, item, update);
		window::set (index, item, update);
	}
	
	/* 
	 * Returns the item located at the specified slot index.
	 */
	slot_item&
	workbench::get (int index)
	{
		if (index >= 10 && index <= 45)
			return this->inv.get (index - 1);
		return window::get (index);
	}
	
	
	
	/* 
	 * Attempts to add @{item} at empty or compatible locations.
	 * Returns the number of items NOT added due to insufficient room.
	 */
	int
	workbench::add (const slot_item& item, bool update)
	{
		return this->inv.add (item, update);
	}
	
	/* 
	 * Attempts to remove items matching item @{item}.
	 * Returns the number of items removed.
	 */
	int
	workbench::remove (const slot_item& item, bool update)
	{
		return this->inv.remove (item, update);
	}
	
	
	
	/* 
	 * Returns a pair of two integers that describe the range of slots where
	 * the item at the specified slot should be moved to when shift clicked.
	 */
	std::pair<int, int>
	workbench::shift_range (int slot)
	{
		if (slot >= 37 && slot <= 45)
			return std::make_pair (10, 36);
		else if (slot >= 10 && slot <= 36)
			return std::make_pair (37, 45);
		else if (slot >= 0 && slot <= 9)
			return std::make_pair (10, 45);
		return std::make_pair (-1, -1);
	}
	
	/* 
	 * Hotbar slot range.
	 */
	std::pair<int, int>
	workbench::hotbar_range ()
	{
		return std::make_pair (37, 45);
	}
	
	/* 
	 * Returns true if players can place the given item at the specified slot.
	 */
	bool
	workbench::can_place_at (int slot, slot_item& item)
	{
		return (slot >= 1 && slot <= 45);
	}
}

