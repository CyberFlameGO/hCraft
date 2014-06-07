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

#include "entities/player_bot.hpp"
#include "player/player.hpp"

#include <iostream> // DEBUG


namespace hCraft {
  
  /* 
   * Constructs a new bot with the specified username.
   */
  player_bot::player_bot (server& srv, const std::string& username)
    : username (username),
      living_entity (srv)
  {
    this->uuid = generate_uuid ();
    this->bbox = bounding_box {
      vector3 (-(this->get_width () / 2.0), -(this->get_height () / 2.0), -(this->get_depth () / 2.0)),
      vector3 (this->get_width () / 2.0, this->get_height () / 2.0, this->get_depth () / 2.0) };
    
    this->ticks = 0;
  }
  
  
  
  static bool
  _check_pos (world& w, int x, int y, int z)
  {
    return w.get_id (x, y, z) == BT_AIR &&
       w.get_id (x, y + 1, z) == BT_AIR;
  }
  
  /* 
	 * Called by the world that's holding the entity every tick (50ms).
	 * A return value of true will cause the world to destroy the entity.
	 */
	bool
	player_bot::tick (world &w)
	{
	  if (entity::tick (w))
	    return true;
	  
	  if (this->ticks == 0)
	    this->dest = this->pos;
	  
	  std::uniform_int_distribution<> act (0, 4);
	  if (act (this->rnd) == 0)
	    {
	      std::uniform_int_distribution<> dis (0, 3);
	      switch (dis (this->rnd))
	        {
	        case 0: if (_check_pos (w, this->dest.x + 1, this->dest.y, this->dest.z)) ++ this->dest.x; break;
	        case 1: if (_check_pos (w, this->dest.x - 1, this->dest.y, this->dest.z)) -- this->dest.x; break;
	        case 2: if (_check_pos (w, this->dest.x, this->dest.y, this->dest.z + 1)) ++ this->dest.z; break;
	        case 3: if (_check_pos (w, this->dest.x, this->dest.y, this->dest.z - 1)) -- this->dest.z; break;
	        }
	    }
	  
	  int dx = dest.x - (int)this->pos.x;
    int dz = dest.z - (int)this->pos.z;
    this->velocity.x = dx * 0.4;
    this->velocity.z = dz * 0.4;
	  
	  ++ this->ticks;
	  return false;
	}
	
	
	
	/* 
	 * Spawns the entity to the specified player.
	 */
	void
	player_bot::spawn_to (player *pl)
	{
	  if (pl->bad ())
			return;
		
		std::string col_name;
		col_name.append ("§1" + this->username);
		
		entity_pos me_pos = this->pos;
		entity_metadata me_meta;
		this->build_metadata (me_meta);
		pl->send (packets::play::make_spawn_player (
			this->get_eid (), this->uuid.to_str ().c_str (), col_name.c_str (),
			me_pos.x, me_pos.y, me_pos.z, me_pos.r, me_pos.l, 0, me_meta));
		pl->send (packets::play::make_entity_head_look (this->get_eid (), me_pos.r));
		//pl->send (packets::play::make_entity_equipment (this->eid, 0, this->inv.get (this->held_slot)));
	}
}

