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

#include "util/position.hpp"
#include "util/utils.hpp"
#include <cmath>


namespace hCraft {
	
	entity_pos::entity_pos (const block_pos &other)
	{
		this->operator= (other);
	}
	
	entity_pos::entity_pos (const chunk_pos &other)
	{
		this->operator= (other);
	}
	
	
	entity_pos&
	entity_pos::operator= (const block_pos &other)
	{
		this->x = other.x;
		this->y = other.y;
		this->z = other.z;
		this->r = 0.0f;
		this->l = 0.0f;
		this->on_ground = true;
		return *this;
	}
	
	entity_pos&
	entity_pos::operator= (const chunk_pos &other)
	{
		this->x = other.x << 4;
		this->y = 0.0;
		this->z = other.z << 4;
		this->r = 0.0f;
		this->l = 0.0f;
		this->on_ground = true;
		return *this;
	}
	
	
	
//----
	
	block_pos::block_pos (const entity_pos &other)
	{
		this->operator= (other);
	}
	
	block_pos::block_pos (const chunk_pos &other)
	{
		this->operator= (other);
	}
	
	
	block_pos&
	block_pos::operator= (const entity_pos &other)
	{
		this->x = utils::floor (other.x);
		this->y = utils::floor (other.y);
		this->z = utils::floor (other.z);
		
		//if (this->y <   0) this->y = 0;
		//if (this->y > 255) this->y = 255;
		return *this;
	}
	
	block_pos&
	block_pos::operator= (const chunk_pos &other)
	{
		this->x = other.x << 4;
		this->y = 0;
		this->z = other.z << 4;
		return *this;
	}
	
	
	
//----
	
	chunk_pos::chunk_pos (const entity_pos &other)
	{
		this->operator= (other);
	}
	
	chunk_pos::chunk_pos (const block_pos &other)
	{
		this->operator= (other);
	}
	
	
	chunk_pos&
	chunk_pos::operator= (const entity_pos &other)
	{
		this->x = utils::floor (other.x) >> 4;
		this->z = utils::floor (other.z) >> 4;
		return *this;
	}
	
	chunk_pos&
	chunk_pos::operator= (const block_pos &other)
	{
		this->x = other.x >> 4;
		this->z = other.z >> 4;
		return *this;
	}
	
	
	
//----
	
	vector3::vector3 ()
	{
		this->x = this->y = this->z = 0;
	}
	
	vector3::vector3 (double x, double y, double z)
	{
		this->x = x;
		this->y = y;
		this->z = z;
	}
	
	vector3::vector3 (entity_pos& epos)
	{
		this->x = epos.x;
		this->y = epos.y;
		this->z = epos.z;
	}
	
	vector3::vector3 (block_pos bpos)
	{
		this->x = bpos.x;
		this->y = bpos.y;
		this->z = bpos.z;
	}
	
	
	
	double
	vector3::magnitude ()
	{
		return std::sqrt (x*x + y*y + z*z);
	}
	
	vector3
	vector3::normalize ()
	{
		return *this / this->magnitude ();
	}
}

