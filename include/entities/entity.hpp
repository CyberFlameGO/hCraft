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

#ifndef _hCraft__ENTITY_H_
#define _hCraft__ENTITY_H_

#include "util/position.hpp"
#include <vector>
#include <chrono>


namespace hCraft {
	
	class world;
	class player;
	class server;
	
	
		
//----
	/* 
	 * Entity metadata.
	 * NOTE: This is a pretty experimental implementation.
	 */
	
	enum entity_metadata_type
	{
		EMT_BYTE,
		EMT_SHORT,
		EMT_INT,
		EMT_FLOAT,
		EMT_STRING,
		EMT_SLOT,
		EMT_BLOCK,
	};
	
	struct entity_metadata_record
	{
		unsigned char type;
		unsigned char index;
		
		union
			{
				char  i8;
				short i16;
				int   i32;
				float f32;
				char* str; // heap-allocated.
				
				struct
					{
						short id;
						char  count;
						short damage;
					} slot;
				
				struct
					{
						int x;
						int y;
						int z;
					} block;
			} data;
		
	public:
		/* 
		 * Constructs a new metadata record.
		 */
		entity_metadata_record (unsigned char index);
		
		/* 
		 * `set' methods:
		 */
		
		void set_byte (char val);
		void set_short (short val);
		void set_int (int val);
		void set_float (float val);
		void set_string (const char *str);
		void set_slot (short id, char count, short damage);
		void set_block (int x, int y, int z);
	};
	
	/* 
	 * The entity metadata "dictionary".
	 */
	class entity_metadata
	{
		std::vector<entity_metadata_record> records;
		
	public:
		inline std::vector<entity_metadata_record>::iterator
		begin ()
			{ return records.begin (); }
			
		inline std::vector<entity_metadata_record>::iterator
		end ()
			{ return records.end (); }
			
	public:
		entity_metadata () { }
		entity_metadata (const entity_metadata &) = delete;
		
		/* 
		 * Class destructor.
		 */
		~entity_metadata ();
		
		
		void
		add (entity_metadata_record rec)
			{ this->records.push_back (rec); }
	};
	
//----
	
	
	
	/* 
	 * Supported types of entities.
	 */
	enum entity_type
	{
		ET_PLAYER,
		ET_PLAYER_BOT,
		
		ET_PIG,
		
		ET_BOAT,
		ET_MINECART,
		ET_ITEM,
		ET_EXPERIENCE_ORB,
		ET_ARROW,
		ET_THROWABLE, // snowballs and chicken eggs
		ET_ENDER_PEARL,
		ET_EYE_OF_ENDER,
		ET_TNT,
		ET_FALLING, // sand/gravel/dragon egg
		ET_FISHING_ROD_BOBBER,
		ET_FIREBALL,
		ET_ENDER_CRYSTAL,
	};
	
	
	
	struct bounding_box
	{
	  vector3 min, max;
	  
	//----
	  double width () const { return this->max.x - this->min.x; }
	  double height () const { return this->max.y - this->min.y; }
	  double depth () const { return this->max.z - this->min.z; }
	  
	//----
	  bool intersects (const bounding_box& other);
	};
	
	
	
	/* 
	 * An entity can be any movable dynamic object in a world that encompasses
	 * a certain "state" (e.g.: players, mobs, minecarts, etc...).
	 */
	class entity
	{
	private:
	  entity_pos old_pos;
	  
	protected:
		server &srv;
		world *curr_world;
		int eid;
		
		bool on_fire;
		bool crouched; // TODO: move to player? since only players can crouch...
		bool riding;
		bool sprinting;
		bool right_action;
		
	public:
		entity_pos pos;
		std::chrono::steady_clock::time_point spawn_time;
		
		vector3 velocity; // blocks per second
		bounding_box bbox;
		
	public:
		inline int get_eid () { return this->eid; }
		
		inline bool is_on_fire () { return this->on_fire; }
		inline bool is_crouched () { return this->crouched; }
		inline bool is_riding () { return this->riding; }
		inline bool is_sprinting () { return this->sprinting; }
		inline bool is_right_action () { return this->right_action; }
		
		virtual double get_width () const { return 1.0; }
		virtual double get_height () const { return 1.0; }
		virtual double get_depth () const { return 1.0; }
		
		inline world* get_world () const { return this->curr_world; }
		inline void set_world (world *wr) { this->curr_world = wr; }
		
	public:
		/* 
		 * Constructs a new entity.
		 */
		entity (server &srv);
		
		// copy constructor.
		entity (const entity&) = delete;
		
		/* 
		 * Class destructor.
		 */
		virtual ~entity ();
		
		/* 
		 * Returns the type of this entity.
		 */
		virtual entity_type get_type () = 0;
		
		
		
		/* 
		 * Returns true if this entity can be seen by the specified one.
		 */
		bool visible_to (entity *ent);
		
		
		
		/* 
		 * Called by the world that's holding the entity every tick (50ms).
		 * A return value of true will cause the world to destroy the entity.
		 */
		virtual bool tick (world &w);
		
		
		
		/* 
		 * Constructs metdata records according to the entity's type.
		 */
		virtual void build_metadata (entity_metadata& dict);
		
		
		
		/* 
		 * Spawns the entity to the specified player.
		 */
		virtual void spawn_to (player *pl) { }
		
		/* 
		 * Removes the entity from the view of the specified player.
		 */
		virtual bool despawn_from (player *pl);
	};
	
	
	
	/* 
	 * Anything that has a health bar, basically.
	 */
	class living_entity: public entity
	{
	protected:
		int hearts; // 1 = half a heart
		int hunger; // 1 = half a hunger thingy
		float hunger_saturation;
		float exhaustion;
		
	public:
		inline int get_hearts () { return this->hearts; }
		inline int get_hunger () { return this->hunger; }
		inline int get_hunger_saturation () { return this->hunger_saturation; }
		inline bool is_dead () { return this->hearts <= 0; }
		
	public:
		living_entity (server &srv);
		

		
		/* 
		 * Health modification:
		 */
		
		/* 
		 * Modifies the entity's health.
		 */
		virtual void set_health (int hearts, int hunger, float hunger_saturation);
		
		// these call set_health () in some way.
		void set_hearts (int hearts);
		void set_hunger (int hunger);
		void set_hunger_saturation (float hunger_saturation);
		void increment_exhaustion (float val);
		void kill () { this->set_hearts (0); }
	};
}

#endif

