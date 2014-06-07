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

#include "entities/pickup.hpp"
#include "player/player.hpp"
#include "system/server.hpp"
#include <vector>
#include <utility>


namespace hCraft {
	
	/* 
	 * Class constructor.
	 */
	e_pickup::e_pickup (server &srv, const slot_item& item)
		: entity (srv),
			data (item)
	{
		this->valid = true;
	}
	
	
	/* 
	 * Constructs metdata records according to the entity's type.
	 */
	void
	e_pickup::build_metadata (entity_metadata& dict) 
	{
		entity::build_metadata (dict);
		
		entity_metadata_record rec (10);
		rec.set_slot (this->data.id (), this->data.amount (), this->data.damage ());
		dict.add (rec);
	}
	
	
	
	/* 
	 * Spawns the entity to the specified player.
	 */
	void
	e_pickup::spawn_to (player *pl)
	{
		if (!this->valid) return;
		
		pl->send (
			packets::play::make_spawn_object (this->eid, 2, this->pos.x, this->pos.y,
				this->pos.z, 0.0f, 0.0f, 1, 0, 2000, 0));
			
		entity_metadata dict;
		this->build_metadata (dict);
		pl->send (
			packets::play::make_entity_metadata (this->eid, dict));
	}
	
	
	
	/* 
	 * Half a second should pass after a pickup item spawns before it could be
	 * collected by a player. This method checks if enough time has passed.
	 */
	bool
	e_pickup::pickable ()
	{
		return ((std::chrono::steady_clock::now () - this->spawn_time)
			>= std::chrono::milliseconds (500)) && (this->data.amount () > 0);
	}
	
	
	static double
	calc_distance_squared (entity_pos origin, entity_pos other)
	{
		double dx = other.x - origin.x;
		double dy = other.y - origin.y;
		double dz = other.z - origin.z;
		return dx*dx + dy*dy + dz*dz;
	}

	/* 
	 * Called by the world that's holding the entity every tick (50ms).
	 * A return value of true will cause the world to destroy the entity.
	 */
	bool
	e_pickup::tick (world &w)
	{
		if (!valid)
			return false;
		if (entity::tick (w))
		  return true;
		
		if (!pickable ())
			return false;
		
		// fetch closest player
		std::pair<player *, double> closest;
		int i = 0;
		e_pickup *me = this;
		w.get_players ().all (
			[&i, me, &closest] (player *pl)
				{
					if (pl->is_dead ())
						return;
					
					double dist = calc_distance_squared (me->pos, pl->pos);
					if (i == 0)
						closest = {pl, dist};
					else
						{
							if (dist < closest.second)
								closest = {pl, dist};
						}
					++ i;
				});
		
		if ((i == 0) || (closest.second > 2.25))
			return false;
		
		player *pl = closest.first;
		if (!pl) return false;
		
		int r = pl->inv.add (this->data);
		if (r == 0)
			{
			  w.get_players ().send_to_all_visible (
			    packets::play::make_collect_item (this->eid, pl->get_eid ()),
			    this);
			}
		
		this->data.set_amount (r);
		pl->send (packets::play::make_sound_effect ("random.pop",
			this->pos.x, this->pos.y, this->pos.z, 0.2f, 98));
		
		if (this->data.empty ())
			{
				this->valid = false;
				w.despawn_entity (this);
				return true;	
			}
		return false;
	}
}

