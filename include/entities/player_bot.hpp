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

#ifndef _hCraft__PLAYER_BOT_H_
#define _hCraft__PLAYER_BOT_H_

#include "entities/entity.hpp"
#include "util/uuid.hpp"
#include <string>
#include <random>


namespace hCraft {

  class server;
  
  
  /* 
   * AI-controlled player.
   */
  class player_bot: public living_entity
  {
    std::string username;
    uuid_t uuid;
    
    std::minstd_rand rnd;
    block_pos dest;
    long long ticks;
    
  public:
    inline const std::string& get_username () const { return this->username; }
		inline uuid_t get_uuid () const { return this->uuid; }
		
		virtual double get_width () const override { return 0.9; }
		virtual double get_height () const override { return 1.9; }
		virtual double get_depth () const override { return 0.9; }
    
  public:
    /* 
     * Constructs a new bot with the specified username.
     */
    player_bot (server& srv, const std::string& username);
    
    
    
    /* 
		 * Returns the type of this entity.
		 */
		virtual entity_type get_type () override
		  { return ET_PLAYER_BOT; }
	  
	  
	  
	  /* 
		 * Called by the world that's holding the entity every tick (50ms).
		 * A return value of true will cause the world to destroy the entity.
		 */
		virtual bool tick (world &w) override;
		
		
		
		/* 
		 * Spawns the entity to the specified player.
		 */
		virtual void spawn_to (player *pl) override;
  };
}

#endif

