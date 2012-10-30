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

#include "slot.hpp"


namespace hCraft {
	
	/* 
	 * Constructs a new slot item from the given ID number, damage\metadata
	 * and amount (optional).
	 */
	slot_item::slot_item (unsigned short id, unsigned short damage,
		unsigned short amount)
	{
		this->set (id, damage, amount);
	}
	
	slot_item::slot_item ()
	{
		this->set (BT_UNKNOWN, 0, 0);
	}
	
	
	
	/* 
	 * Resets the item's fields.
	 */
	void
	slot_item::set (unsigned short id, unsigned short damage,
		unsigned short amount)
	{
		if (amount == 0)
			{
				// this sets the id and damage to 0 as well.
				this->set_amount (0);
			}
		else
			{
				this->set_id (id);
				this->set_damage (damage);
				this->set_amount (amount);
			}
	}
	
	void
	slot_item::set_id (unsigned short id)
	{
		this->s_id = id;
		if (!this->is_valid ())
			this->s_info.ptr = nullptr;
		else
			{
				if (this->is_block ())
					this->s_info.binf = block_info::from_id (id);
				else if (this->is_item ())
					this->s_info.iinf = item_info::from_id (id);
			}
	}

	void
	slot_item::set_damage (unsigned short damage)
		{ this->s_damage = damage; }
	
	void
	slot_item::set_amount (unsigned short amount)
	{
		this->s_amount = amount;
		if (this->s_amount == 0)
			{
				this->set_id (0);
				this->set_damage (0);
			}
	}
		
		
	
	const char*
	slot_item::name () const
	{
		if (this->s_info.ptr)
			{
				if (this->is_block ())
					return this->s_info.binf->name;
				else if (this->is_item ())
					return this->s_info.iinf->name;
			}
		
		return "unknown";
	}
	
	
	
//----
	/* 
	 * Comparison operators:
	 */
	
	bool
	slot_item::operator== (const slot_item& other) const
	{
		return ((this->s_id == other.s_id) &&
						(this->s_damage == other.s_damage) &&
						(this->s_amount == other.s_amount));
	}
	
	bool
	slot_item::operator!= (const slot_item& other) const
	{
		return !(this->operator== (other));
	}
}

