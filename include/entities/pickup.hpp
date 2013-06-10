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

#ifndef _hCraft__PICKUP_H_
#define _hCraft__PICKUP_H_

#include "entities/entity.hpp"
#include "slot.hpp"


namespace hCraft {
	
	/* 
	 * Represents a floating inventory item.
	 */
	class e_pickup: public entity
	{
		slot_item data;
		bool valid;
		
	public:
		/* 
		 * Class constructor.
		 */
		e_pickup (int eid, const slot_item& item);
		
		/* 
		 * Returns the type of this entity.
		 */
		virtual entity_type get_type () { return ET_ITEM; }
		
		
		
		/* 
		 * Constructs metdata records according to the entity's type.
		 */
		virtual void build_metadata (entity_metadata& dict) override;
		
		
		
		/* 
		 * Spawns the entity to the specified player.
		 */
		virtual void spawn_to (player *pl) override;
		
		
		
		/* 
		 * Half a second should pass after a pickup item spawns before it could be
		 * collected by a player. This method checks if enough time has passed.
		 */
		bool pickable ();
		
		/* 
		 * Called by the world that's holding the entity every tick (50ms).
		 * A return value of true will cause the world to destroy the entity.
		 */
		virtual bool tick (world &w) override;
	};
}

#endif

