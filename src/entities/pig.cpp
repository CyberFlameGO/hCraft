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

#include "entities/pig.hpp"
#include "player/player.hpp"


namespace hCraft {
	
	e_pig::e_pig (server &srv)
		: living_entity (srv)
		{ }
	
	
	
	/* 
	 * Constructs metdata records according to the entity's type.
	 */
	void
	e_pig::build_metadata (entity_metadata& dict)
	{
		living_entity::build_metadata (dict);
	}
		
		
		
	/* 
	 * Spawns the entity to the specified player.
	 */
	void
	e_pig::spawn_to (player *pl)
	{
		entity_pos pos = this->pos;
		entity_metadata meta;
		this->build_metadata (meta);
		pl->send (packet::make_spawn_mob (
			this->get_eid (), 90,
			pos.x, pos.y, pos.z, pos.r, pos.l, pos.l, 0, 0, 0, meta));
	}
	
	
	
	/* 
	 * Called by the world that's holding the entity every tick (50ms).
	 * A return value of true will cause the world to destroy the entity.
	 */
	bool
	e_pig::tick (world &w)
	{
		
		
		return false;
	}
}

