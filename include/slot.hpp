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

#ifndef _hCraft__SLOT_H_
#define _hCraft__SLOT_H_

#include "blocks.hpp"
#include "items.hpp"


namespace hCraft {
	
	/* 
	 * A structure that can represent an item or a block, along with additional
	 * metadata if necessary.
	 */
	class slot_item
	{
		unsigned short s_id;
		unsigned short s_damage; // or metadata, if it is a block.
		unsigned short s_amount;
		
		union
			{
				block_info *binf;
				item_info  *iinf;
				void       *ptr;
			} s_info;
		
	public:
		inline bool is_valid () const { return s_id != BT_UNKNOWN && s_id != IT_UNKNOWN && s_id != BT_AIR; }
		inline bool is_block () const { return is_valid () && s_id < 0x100; }
		inline bool is_item () const { return is_valid () && s_id >= 0x100; }
		
		inline unsigned short id () const { return s_id; }
		inline unsigned short damage () const { return s_damage; }
		inline unsigned short amount () const { return s_amount; }
		
		inline int max_stack () const {
			if (is_block ())
				return this->s_info.binf->max_stack;
			else if (is_item ())
				return this->s_info.iinf->max_stack;
			return 0;
		}
		inline bool empty () const { return s_amount == 0; }
		inline bool full () const
		{
			if (is_block ())
				return this->s_amount == this->s_info.binf->max_stack;
			else if (is_item ())
				return this->s_amount == this->s_info.iinf->max_stack;
			return true;
		}
		
	public:
		/* 
		 * Constructs a new slot item from the given ID number, damage\metadata
		 * and amount (optional).
		 */
		slot_item (unsigned short id, unsigned short damage = 0,
			unsigned short amount = 1);
		slot_item ();
		
		
		/* 
		 * Resets the item's fields.
		 */
		void set (unsigned short id, unsigned short damage = 0,
			unsigned short amount = 1);
		void set_id (unsigned short id);
		void set_damage (unsigned short damage);
		void set_amount (unsigned short amount);
		void clear ()
			{ this->set (BT_AIR, 0, 0); }
		
		const char* name () const;
		
	//----
		bool operator== (const slot_item& other) const;
		bool operator!= (const slot_item& other) const;
	};
}

#endif

