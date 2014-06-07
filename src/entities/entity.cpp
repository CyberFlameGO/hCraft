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

#include "entities/entity.hpp"
#include "player/player.hpp"
#include "system/server.hpp"
#include "util/utils.hpp"
#include "slot/blocks.hpp"
#include <cstring>

#include <iostream> // DEBUG


namespace hCraft {

  bool
  bounding_box::intersects (const bounding_box& other)
  {
    if ((this->max.x >= other.min.x) && (this->min.x <= other.max.x))
      {
        if ((this->max.y < other.min.y) || (this->min.y > other.max.y))
          return false;
        
        return (this->max.z >= other.min.z) && (this->min.z <= other.max.z); 
      }
    
    return false;
  }


	
	/* 
	 * Constructs a new metadata record.
	 */
	entity_metadata_record::entity_metadata_record (unsigned char index)
	{
		this->index = index;
		this->data.str = nullptr;
	}
	
	
	
	/* 
	 * `set' methods:
	 */
	
	void
	entity_metadata_record::set_byte (char val)
	{
		this->type = EMT_BYTE;
		this->data.i8 = val;
	}
	
	void
	entity_metadata_record::set_short (short val)
	{
		this->type = EMT_SHORT;
		this->data.i16 = val;
	}
	
	void
	entity_metadata_record::set_int (int val)
	{
		this->type = EMT_INT;
		this->data.i32 = val;
	}
	
	void
	entity_metadata_record::set_float (float val)
	{
		this->type = EMT_FLOAT;
		this->data.f32 = val;
	}
	
	void
	entity_metadata_record::set_string (const char *str)
	{
		this->type = EMT_STRING;
		this->data.str = new char[std::strlen (str) + 1];
		std::strcpy (this->data.str, str);
	}
	
	void
	entity_metadata_record::set_slot (short id, char count, short damage)
	{
		this->type = EMT_SLOT;
		this->data.slot.id = id;
		this->data.slot.count = count;
		this->data.slot.damage = damage;
	}
	
	void
	entity_metadata_record::set_block (int x, int y, int z)
	{
		this->type = EMT_BLOCK;
		this->data.block.x = x;
		this->data.block.y = y;
		this->data.block.z = z;
	}
	
	
	
//----
	
	/* 
	 * Class destructor.
	 */
	entity_metadata::~entity_metadata ()
	{
		for (auto& rec : this->records)
			{
				if (rec.type == EMT_STRING)
					delete[] rec.data.str;
			}
	}
	
	
	
//----
	
	/* 
	 * Constructs a new entity.
	 */
	entity::entity (server &srv)
		: srv (srv)
	{
		this->eid = srv.register_entity (this);
		
		this->pos.set (0.0, 0.0, 0.0, 0.0f, 0.0f, true);
		this->on_fire = false;
		this->crouched = false;
		this->riding = false;
		this->sprinting = false;
		this->right_action = false;
		
		this->bbox = bounding_box { vector3 (-0.5, -0.5, -0.5), vector3 (0.5, 0.5, 0.5) };
	}
	
	/* 
	 * Class destructor.
	 */
	entity::~entity ()
	{
		this->curr_world = nullptr;
	}
	
	
	
	/* 
	 * Returns true if this entity can be seen by the specified one.
	 */
	bool
	entity::visible_to (entity *ent)
	{
	  chunk_pos me_pos = this->pos;
		chunk_pos ot_pos = ent->pos;
		
		return (
			(utils::iabs (me_pos.x - ot_pos.x) <= player::chunk_radius ()) &&
			(utils::iabs (me_pos.z - ot_pos.z) <= player::chunk_radius ()));
	}
	
	
	
	/* 
	 * Constructs metdata records according to the entity's type.
	 */
	void
	entity::build_metadata (entity_metadata& dict)
	{
		unsigned char flags = 0;
		if (this->on_fire) flags |= 0x01;
		if (this->crouched) flags |= 0x02;
		if (this->riding) flags |= 0x04;
		if (this->sprinting) flags |= 0x08;
		if (this->right_action) flags |= 0x10;
		
		entity_metadata_record rec (0);
		rec.set_byte (flags);
		dict.add (rec);
	}
	
	
	
	/* 
	 * Removes the entity from the view of the specified player.
	 */
	bool
	entity::despawn_from (player *pl)
	{
		if (this->get_type () == ET_PLAYER)
			if (static_cast<player *> (this) == pl)
				return false;
		
		pl->send (packets::play::make_destroy_entity (this->eid));
		return true;
	}
	
	
	
	static void
	_coll_x (entity& ent)
	{
	  if (ent.velocity.x == 0.0)
	    return;
	  
	  world *wr = ent.get_world ();
	  if (!wr)
	    return;
	  
	  block_pos bpos = ent.pos;
	  if (wr->get_id (bpos.x, bpos.y, bpos.z) != BT_AIR)
	    return;
	  
	  double sx = ent.pos.x - 0.5*ent.get_width ();
	  double ex = sx + ent.get_width ();
	  double sy = ent.pos.y;
	  double ey = sy + 1.0;
	  double sz = ent.pos.z - 0.5*ent.get_depth ();
	  double ez = sz + ent.get_depth ();
	   
	  if (ent.velocity.x < 0.0)
	    sx += ent.velocity.x;
	  else
	    ex += ent.velocity.x;
	  
	  int cp;
	  bool found = false;
	  for (double x = sx; x <= ex; x += 1.0)
	    for (double y = sy; y <= ey; y += 1.0)
	      for (double z = sz; z <= ez; z += 1.0)
	        {
	          block_info *binf = block_info::from_id (wr->get_id (utils::floor (x), utils::floor (y), utils::floor (z)));
	          if (binf && binf->opaque)
	            {
	              if (ent.velocity.x > 0.0)
	                {
	                  if (wr->get_id (utils::floor (x) - 1, utils::floor (y), utils::floor (z)) != BT_AIR)
	                    continue;
	                }
	              else
	                {
	                  if (wr->get_id (utils::floor (x) + 1, utils::floor (y), utils::floor (z)) != BT_AIR)
	                    continue;
	                }
	              
	              // found collision
	              wr->queue_update (utils::floor (x), utils::floor (y), utils::floor (z), BT_GLASS);
	              if (!found)
	                {
	                  cp = x;
	                  found = true;
	                }
	              else
	                {
	                  if (((ent.velocity.x > 0.0) && (x < cp)) ||
	                      ((ent.velocity.x < 0.0) && (x > cp)))
	                      cp = x;
	                }
	            }
	        }
	  
	  if (found)
	    {
	      double v = ent.velocity.x;
	      ent.velocity.x = 0.0;
	      
	      ent.pos.x = cp;
	      if (v > 0.0)
	        ent.pos.x -= ent.get_width ()*0.51;
	      else
	        ent.pos.x += 1.0 + ent.get_width ()*0.51;
      }
	}
	
	static void
	_coll_z (entity& ent)
	{
	  if (ent.velocity.z == 0.0)
	    return;
	  
	  world *wr = ent.get_world ();
	  if (!wr)
	    return;
	  
	  block_pos bpos = ent.pos;
	  if (wr->get_id (bpos.x, bpos.y, bpos.z) != BT_AIR)
	    return;
	  
	  double sx = ent.pos.x - 0.5*ent.get_width ();
	  double ex = sx + ent.get_width ();
	  double sy = ent.pos.y;
	  double ey = sy + 1.0;
	  double sz = ent.pos.z - 0.5*ent.get_depth ();
	  double ez = sz + ent.get_depth ();
	   
	  if (ent.velocity.z < 0.0)
	    sz += ent.velocity.z;
	  else
	    ez += ent.velocity.z;
	  
	  int cp;
	  bool found = false;
	  for (double x = sx; x <= ex; x += 1.0)
	    for (double y = sy; y <= ey; y += 1.0)
	      for (double z = sz; z <= ez; z += 1.0)
	        {
	          block_info *binf = block_info::from_id (wr->get_id (utils::floor (x), utils::floor (y), utils::floor (z)));
	          if (binf && binf->opaque)
	            {
	              if (ent.velocity.z > 0.0)
	                {
	                  if (wr->get_id (utils::floor (x), utils::floor (y), utils::floor (z) - 1) != BT_AIR)
	                    continue;
	                }
	              else
	                {
	                  if (wr->get_id (utils::floor (x), utils::floor (y), utils::floor (z) + 1) != BT_AIR)
	                    continue;
	                }
	              
	              // found collision
	              wr->queue_update (utils::floor (x), utils::floor (y), utils::floor (z), BT_GLASS);
	              if (!found)
	                {
	                  cp = z;
	                  found = true;
	                }
	              else
	                {
	                  if (((ent.velocity.z > 0.0) && (z < cp)) ||
	                      ((ent.velocity.z < 0.0) && (z > cp)))
	                      cp = z;
	                }
	            }
	        }
	  
	  if (found)
	    {
	      double v = ent.velocity.z;
	      ent.velocity.z = 0.0;
	      
	      ent.pos.z = cp;
	      if (v > 0.0)
	        ent.pos.z -= ent.get_depth ()*0.51;
	      else
	        ent.pos.z += 1.0 + ent.get_depth ()*0.51;
	    }
	}
	
	inline static double
	_dist_squared (double x1, double y1, double x2, double y2)
	{
	  double d1 = x1 - x2;
	  double d2 = y1 - y2;
	  return d1*d1 + d2*d2;
	}
	
	/*
	static void
	_coll_xz (entity& ent)
	{
	  if (ent.velocity.x == 0.0)
	    return;
	  
	  world *wr = ent.get_world ();
	  if (!wr)
	    return;
	  
	  double sx = ent.pos.x - 0.5*ent.get_width ();
	  double ex = sx + ent.get_width ();
	  double sy = ent.pos.y;
	  double ey = sy + 1.0;
	  double sz = ent.pos.z - 0.5*ent.get_depth ();
	  double ez = sz + ent.get_depth ();
	   
	  if (ent.velocity.x < 0.0)
	    sx += ent.velocity.x;
	  else
	    ex += ent.velocity.x;
	  
	  if (ent.velocity.z < 0.0)
	    sz += ent.velocity.z;
	  else
	    ez += ent.velocity.z;
	  
	  int cp_x, cp_z;
	  bool found = false;
	  for (double x = sx; x <= ex; x += 1.0)
	    for (double y = sy; y <= ey; y += 1.0)
	      for (double z = sz; z <= ez; z += 1.0)
	        {
	          block_info *binf = block_info::from_id (wr->get_id (utils::floor (x), utils::floor (y), utils::floor (z)));
	          if (binf && binf->opaque)
	            {
	              // found collision
	              if (!found)
	                {
	                  cp_x = x;
	                  cp_z = z;
	                  found = true;
	                }
	              else if (_dist_squared (cp_x, cp_z, ent.pos.x, ent.pos.z) > _dist_squared (x, z, ent.pos.x, ent.pos.z))
	                {
	                  cp_x = x;
	                  cp_z = z;
	                }
	            }
	        }
	  
	  if (found)
	    {
	      ent.pos.x = cp_x;
	      ent.pos.z = cp_z;
	      
	      if ((ent.velocity.x > 0.0) && (wr->get_id (cp_x - 1, utils::floor (ent.pos.y), cp_z) == BT_AIR))
	        ent.pos.x -= ent.get_width ()*0.51;
	      else if ((ent.velocity.x < 0.0) && (wr->get_id (cp_x + 1, utils::floor (ent.pos.y), cp_z) == BT_AIR))
	        ent.pos.x += 1.0 + ent.get_width ()*0.51;
	      
	      if (wr->get_id (utils::floor (ent.pos.x), utils::floor (ent.pos.y), cp_z) != BT_AIR)
	        {
	          if ((ent.velocity.z > 0.0) && (wr->get_id (utils::floor (ent.pos.x), utils::floor (ent.pos.y), cp_z - 1) == BT_AIR))
	            ent.pos.z -= ent.get_depth ()*0.51;
	          else if ((ent.velocity.z < 0.0) && (wr->get_id (utils::floor (ent.pos.x), utils::floor (ent.pos.y), cp_z + 1) == BT_AIR))
	            ent.pos.z += 1.0 + ent.get_depth ()*0.51;
	        }
	      
	      ent.velocity.x = 0.0;
	      ent.velocity.z = 0.0;
	    }
	}
	*/
	
	static void
	_coll_y (entity& ent)
	{
	  if (ent.velocity.y == 0.0)
	    return;
	  
	  world *wr = ent.get_world ();
	  if (!wr)
	    return;
	  
	  double sx = ent.pos.x - 0.5*ent.get_width ();
	  double ex = sx + ent.get_width ();
	  double sy = ent.pos.y;
	  double ey = sy + 1.0 /*ent.get_height ()*/;
	  double sz = ent.pos.z - 0.5*ent.get_depth ();
	  double ez = sz + ent.get_depth ();
	  
	  if (ent.velocity.y < 0.0)
	    sy += ent.velocity.y;
	  else
	    ey += ent.velocity.y;
	  
	  // clamp
	  if (sy < 0.0) sy = 0.0;
	  if (sy > 255.0) sy = 255.0;
	  if (ey < 0.0) ey = 0.0;
	  if (ey > 255.0) ey = 255.0;
	  
	  int cp;
	  bool found = false;
	  for (double x = sx; x <= ex; x += 1.0)
	    for (double y = sy; y <= ey; y += 1.0)
	      for (double z = sz; z <= ez; z += 1.0)
	        {
	          block_info *binf = block_info::from_id (wr->get_id (utils::floor (x), utils::floor (y), utils::floor (z)));
	          if (binf && binf->opaque)
	            {
	              // found collision
	              if (!found)
	                {
	                  cp = y;
	                  found = true;
	                }
	              else
	                {
	                  if (((ent.velocity.y > 0.0) && (y < cp)) ||
	                      ((ent.velocity.y < 0.0) && (y > cp)))
	                      cp = y;
	                }
	            }
	        }
	  
	  if (found)
	    {
	      ent.pos.y = cp;
	      if (ent.velocity.y > 0.0)
	        ent.pos.y -= 2.0;
	      else
	        ++ ent.pos.y;
	      
	      ent.velocity.y = 0.0;
	    }
	}
	
	
	/* 
	 * Called by the world that's holding the entity every tick (50ms).
	 * A return value of true will cause the world to destroy the entity.
	 */
	bool
	entity::tick (world &w)
	{
		// drag force
		this->velocity = 0.98 * this->velocity;
		
		// ground friction
		if ((this->pos.y >= 1.0) && ((this->pos.y - (int)this->pos.y) == 0.0))
		  {
		    this->velocity = 0.5 * this->velocity;
		  }
		
		// acceleration due to gravity
		this->velocity.y -= 0.08;
		if (this->velocity.y < -3.92)
		  this->velocity.y = -3.92;
		
		// collision detection
	  {
	    entity_pos tmp, pos1, pos2;
	    vector3 vel = this->velocity;
	    
	    // first way
	    tmp = this->pos;
	    _coll_x (*this);
	    _coll_z (*this);
	    pos1 = this->pos;
	    
	    // second way
	    this->pos = tmp;
	    this->velocity = vel;
	    _coll_z (*this);
	    _coll_x (*this);
	    pos2 = this->pos;
	    
	    if (_dist_squared (pos1.x, pos1.y, tmp.x, tmp.y) < _dist_squared (pos2.x, pos2.y, tmp.x, tmp.y))
	      this->pos = pos1;
	    else
	      this->pos = pos2;
	  }
    _coll_y (*this);
		
		this->pos.x += this->velocity.x;
		this->pos.y += this->velocity.y;
		this->pos.z += this->velocity.z;
		
		// notify players of new movement
		if (this->pos != this->old_pos)
		  {
		    if (this->pos.x != this->old_pos.x || this->pos.y != this->old_pos.y ||
		      this->pos.z != this->old_pos.z)
		      {
		        // just position
		        w.get_players ().send_to_all_visible (
		          packets::play::make_entity_move (this->get_eid (),
					      this->pos.x, this->pos.y, this->pos.z, this->pos.r, this->pos.l), this);
		      }
		  }
		
		this->old_pos = pos;
		return false;
	}
	
	
	
//----

	living_entity::living_entity (server &srv)
		: entity (srv)
		{ }
	
	
	
	/* 
	 * Modifies the entity's health.
	 */
	void
	living_entity::set_health (int hearts, int hunger, float hunger_saturation)
	{
		this->hearts = hearts;
		this->hunger = hunger;
		this->hunger_saturation = hunger_saturation;
	}
	
	void
	living_entity::set_hearts (int hearts)
	{
		if (hearts < this->hearts)
			{
				// exhaustion from damage
				this->increment_exhaustion (0.3);
			}
		this->set_health (hearts, this->hunger, this->hunger_saturation);
	}
	
	void
	living_entity::set_hunger (int hunger)
	{
		if (this->hunger_saturation > hunger)
			this->hunger_saturation = hunger;
		this->set_health (this->hearts, hunger, this->hunger_saturation);
	}
	
	void
	living_entity::set_hunger_saturation (float hunger_saturation)
	{
		this->set_health (this->hearts, this->hunger, hunger_saturation);
	}
	
	void
	living_entity::increment_exhaustion (float val)
	{
		this->exhaustion += val;
		if (this->exhaustion >= 4.0)
			{
				this->exhaustion -= 4.0;
				if (this->hunger_saturation <= 0.0)
					this->set_hunger (this->hunger - 1);
				else
					{
						double new_sat = this->hunger_saturation - 1.0;
						if (new_sat < 0.0) new_sat = 0.0;
						this->set_hunger_saturation (new_sat);
					}
			}
	}
}

