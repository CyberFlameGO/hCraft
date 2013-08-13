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

#include "slot.hpp"


namespace hCraft {
	
	/* 
	 * Constructs a new slot item from the given ID number, damage\metadata
	 * and amount (optional).
	 */
	slot_item::slot_item (unsigned short id, unsigned short damage,
		unsigned short amount, const enchantment_list *enchants)
	{
		this->set (id, damage, amount);
		if (enchants)
			this->set_enchants (*enchants);
	}
	
	slot_item::slot_item ()
	{
		this->set (BT_AIR, 0, 0);
	}
	
	slot_item::slot_item (const slot_item& other)
	{
		this->set (other.s_id, other.s_damage, other.s_amount);
		this->enchants = other.enchants;
		this->display_name = other.display_name;
		this->lore = other.lore;
	}
	
	
	
	/* 
	 * Resets the item's fields.
	 */
	
	void
	slot_item::set_enchants (const enchantment_list& enc)
	{
		this->enchants = enc;
	}
	
	void
	slot_item::copy_enchants_from (const slot_item& other)
	{
		this->set_enchants (other.enchants);
	}
	
	void
	slot_item::set (const slot_item& other)
	{
		this->set (other.s_id, other.s_damage, other.s_amount);
		this->copy_enchants_from (other);
		this->display_name = other.display_name;
		this->lore = other.lore;
	}
	
	void
	slot_item::set (unsigned short id, unsigned short damage,
		unsigned short amount, const enchantment_list *enchants)
	{
		this->set_id (id);
		this->set_damage (damage);
		this->set_amount (amount);
		if (enchants)
			this->set_enchants (*enchants);
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
				if (this->s_id != 0xFFFF && this->s_id != 0)
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
	
	
	int
	slot_item::max_stack () const
	{
		if (is_block () && this->s_info.binf)
			return this->s_info.binf->max_stack;
		else if (is_item ())
			{
				if (this->s_info.iinf)
					return this->s_info.iinf->max_stack;
				if (item_info::is_tool (this->s_id))
					return 1;
			}
		return 64;
	}
	
	
	
	/* 
	 * Checks whether the given slot item can be stacked on top of this one.
	 */
	bool
	slot_item::compatible_with (const slot_item& s)
	{
		if ((this->id () == BT_AIR) || (s.id () == BT_AIR))
			return true;
		
		bool this_tool = item_info::is_tool (this->id ());
		bool s_tool = item_info::is_tool (s.id ());
		return ((this->id () == s.id ()) &&
				(this->damage () == s.damage ()) &&
				(!this_tool && !s_tool));
	}
	
	
	/* 
	 * Increase\reduce amount
	 */
	void
	slot_item::take (int a)
	{
		int new_a = this->amount () - a;
		if (new_a < 0)
			new_a = 0;
		this->set_amount (new_a);
	}
	
	void
	slot_item::give (int a)
	{
		int new_a = this->amount () + a;
		if (new_a > this->max_stack ())
			new_a = this->max_stack ();
		this->set_amount (new_a);
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
	
	slot_item&
	slot_item::operator= (const slot_item& other)
	{
		this->set (other.s_id, other.s_damage, other.s_amount);
		
		this->enchants = other.enchants;
		this->display_name = other.display_name;
		this->lore = other.lore;
			
		return *this;
	}
}

