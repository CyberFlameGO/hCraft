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

#include "world/world_security.hpp"
#include "player/player.hpp"
#include "system/server.hpp"
#include "util/stringutils.hpp"


namespace hCraft {
	
	const std::string&
	world_security::get_build_perms () const
	{
		return this->ps_build;
	}
	
	const std::string&
	world_security::get_join_perms () const
	{
		return this->ps_join;
	}
	
	void
	world_security::set_build_perms (const std::string& str)
	{
		this->ps_build = str;
	}
	
	void
	world_security::set_join_perms (const std::string& str)
	{
		this->ps_join = str;
	}
	
	
	
	/* 
	 * Returns true if the specified player can modify blocks in the world.
	 */
	bool
	world_security::can_build (player *pl) const
	{
		return this->is_owner (pl) || this->is_member (pl)
			|| basic_security::check_perm_str (pl, this->ps_build);
	}
	
	/* 
	 * Returns true if the specified player can join the world.
	 */
	bool
	world_security::can_join (player *pl) const
	{
		return this->is_owner (pl) || this->is_member (pl)
			|| basic_security::check_perm_str (pl, this->ps_join);
	}
}

