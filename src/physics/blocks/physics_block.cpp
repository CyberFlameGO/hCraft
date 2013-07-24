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

#include "physics/blocks/physics_block.hpp"
#include "cistring.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <cctype>
#include <sstream>

// physics:
#include "physics/blocks/sand.hpp"
#include "physics/blocks/langtons_ant.hpp"
#include "physics/blocks/water.hpp"
#include "physics/blocks/snow.hpp"
#include "physics/blocks/activewater.hpp"
#include "physics/blocks/sponge.hpp"
#include "physics/blocks/c235.hpp"
#include "physics/blocks/firework.hpp"
#include "physics/blocks/water_faucet.hpp"


namespace hCraft {
	
	static bool _physics_blocks_initialized = false;
	static std::vector<std::shared_ptr<physics_block> > _phblocks;
	static std::unordered_map<cistring, int> _phblock_name_map;
	
	
	static void
	_register_physics_block (physics_block *ph)
	{
		if (ph->id () >= (int)_phblocks.size ())
			_phblocks.resize (ph->id () + 1);
		_phblocks[ph->id ()].reset (ph);
		_phblock_name_map[ph->name ()] = ph->id ();
	}
	
	
	/*  
	 * Initialization/Destruction of the global physics block list.
	 */
	void
	physics_block::init_blocks ()
	{
		if (_physics_blocks_initialized)
			return;
		
		// physics blocks
		{
			_register_physics_block (new physics::sand ());
			_register_physics_block (new physics::langtons_ant ());
			_register_physics_block (new physics::water ());
			_register_physics_block (new physics::snow ());
			_register_physics_block (new physics::active_water ());
			_register_physics_block (new physics::water_sponge ());
			_register_physics_block (new physics::water_sponge_agent ());
			_register_physics_block (new physics::c235 ());
			_register_physics_block (new physics::firework ());
			_register_physics_block (new physics::firework_rocket ());
			_register_physics_block (new physics::water_faucet ());
		}
		
		_physics_blocks_initialized = true;
	}
	
	void
	physics_block::destroy_blocks ()
	{
		if (!_physics_blocks_initialized)
			return;
		
		_phblocks.clear ();
		
		_physics_blocks_initialized = false;
	}
	
	
	
	/* 
	 * Lookup:
	 */
	
	physics_block*
	physics_block::from_id (int id)
	{
		if (!_physics_blocks_initialized)
			return nullptr;
		
		if (id >= (int)_phblocks.size ())
			return nullptr;
		
		return _phblocks[id].get ();
	}

	physics_block*
	physics_block::from_name (const char *name)
	{
		if (!_physics_blocks_initialized)
			return nullptr;
		
		auto itr = _phblock_name_map.find (name);
		if (itr == _phblock_name_map.end ())
			return nullptr;
		
		return physics_block::from_id (itr->second);
	}
	
	physics_block*
	physics_block::from_id_or_name (const char *str)
	{
		bool is_num = true;
		const char *ptr = str;
		while (*ptr)
			if (!std::isdigit (*ptr++))
				{ is_num = false; break; }
		
		if (is_num)
			{
				std::istringstream ss (str);
				int num;
				ss >> num;
				return physics_block::from_id (num);
			}
		return physics_block::from_name (str);
	}
}

