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

#include "rank.hpp"
#include <cstring>
#include <sstream>
#include <string>
#include <stdexcept>


namespace hCraft {
	
	/* 
	 * Class constructor.
	 */
	group::group (permission_manager& perm_man, int id, int power, const char *name)
		: name (name), perm_man (perm_man)
	{
		this->id = id;
		this->power = power;
		this->color = 'f';
		this->text_color = 'f';
		this->can_chat = true;
		this->can_build = true;
		this->can_move = true;
		this->all_perms = false;
		this->no_perms = false;
	}
	
	
	
	/* 
	 * Adds the specified permission node into the group's permissions list.
	 */
	void
	group::add (permission perm)
	{
		// TODO: do not insert duplicates.
		if (perm.neg)
			{
				if (perm.nodes[0] == PERM_WILD_ALL && perm.nodes[1] == -1)
					this->no_perms = true;
				this->neg_perms.insert (perm);
			}
		else
			{
				if (perm.nodes[0] == PERM_WILD_ALL && perm.nodes[1] == -1)
					this->no_perms = false;
				this->perms.insert (perm);
			}
	}
	
	void
	group::add (const char *perm)
	{
		permission res = this->perm_man.add (perm);
		this->add (res);
	}
	
	
	/* 
	 * Adds all permission nodes contained within the specified null-terminated
	 * array into the group's permissions list.
	 */
	void
	group::add (const char **perms)
	{
		const char** ptr = perms;
		while (*ptr)
			this->add (*ptr++);
	}
	
	
	
	/* 
	 * Inherits all permissions from the specified group.
	 */
	void
	group::inherit (group *grp)
	{
		if (!grp || grp == this)
			return;
		this->parents.push_back (grp);
	}
	
	
	
	/* 
	 * Checks whether this group has the given permission node.
	 */
	bool
	group::has (permission perm) const
	{
		if (this->no_perms)
			return false;
		if (!perm.valid ())
			{
				if (this->all_perms)
					return true;
				return false;
			}
		
		int node;
		
		// check negated perms first
		for (permission p : this->neg_perms)
			{
				bool match = true;
				for (int i = 0; i < PERM_NODE_COUNT; ++i)
					{
						node = p.nodes[i];
						if (node == 0)
							break;
						
						if (node != PERM_WILD_ALL && node != perm.nodes[i])
							{ match = false; break; }
					}
				
				if (match)
					return false;
			}
		
		auto itr = this->perms.find (perm);
		if (itr != this->perms.end ())
			return true;
			
		for (permission p : this->perms)
			{
				bool match = true;
				for (int i = 0; i < PERM_NODE_COUNT; ++i)
					{
						node = p.nodes[i];
						if (node == 0)
							break;
						
						if (node != PERM_WILD_ALL && node != perm.nodes[i])
							{ match = false; break; }
					}
				
				if (match)
					return true;
			}
		
		// Nope? Check parent groups
		for (group *grp : this->parents)
			if (grp->has (perm))
				return true;
		
		return false;
	}
	
	bool
	group::has (const char *str) const
	{
		permission res = this->perm_man.get (str);
		return this->has (res);
	}
	
	
	
//----
	
	/* 
	 * Class constructor.
	 */
	group_manager::group_manager (permission_manager& perm_man)
		: perm_man (perm_man)
	{
		this->id_counter = 0;
	}
	
	/* 
	 * Class destructor.
	 */
	group_manager::~group_manager ()
	{
		this->clear ();
	}
	
	
	
	/* 
	 * Creates and return a new group.
	 */
	group*
	group_manager::add (int power, const char *name)
	{
		std::string nm {name};
		auto itr = this->groups.find (nm);
		if (itr != this->groups.end ())
			{ return nullptr; }
		
		group* grp = new group (this->perm_man, (this->id_counter)++, power, name);
		this->groups[std::move (nm)] = grp;
		return grp;
	}
	
	/* 
	 * Removes all groups from this manager.
	 */
	void
	group_manager::clear ()
	{
		for (auto itr = this->groups.begin (); itr != this->groups.end (); ++itr)
			{
				group *grp = itr->second;
				delete grp;
			}
		this->groups.clear ();
	}
	
	
	
	/* 
	 * Searches for the group that has the specified name (case-sensitive).
	 */
	group*
	group_manager::find (const char *name)
	{
		auto itr = this->groups.find (name);
		if (itr != this->groups.end ())
			return itr->second;
		return nullptr;
	}
	
	/* 
	 * Returns the group with the highest power number.
	 */
	group*
	group_manager::highest ()
	{
		if (this->groups.empty ())
			return nullptr;
		
		auto itr = this->groups.begin ();
		group *highest = itr->second;
		
		for (++itr; itr != this->groups.end (); ++itr)
			if (itr->second->power > highest->power)
				highest = itr->second;
		
		return highest;
	}
	
	
	
	/* 
	 * Attempts to find a suitable ladder for the given group.
	 */
	group_ladder*
	group_manager::default_ladder (group *grp)
	{
		for (auto itr = this->ladders.begin (); itr != this->ladders.end (); ++itr)
			{
				group_ladder &ladder = itr->second;
				if (ladder.has_group (grp))
					return &ladder;
			}
		
		return nullptr;
	}
	
	/* 
	 * Finds a group ladder by its name.
	 */
	group_ladder*
	group_manager::find_ladder (const char *name)
	{
		auto itr = this->ladders.find (name);
		if (itr == this->ladders.end ())
			return nullptr;
		return &itr->second;
	}
	
	/* 
	 * Creates and return a new group ladder.
	 */
	group_ladder*
	group_manager::add_ladder (const char *name)
	{
		return &this->ladders[name];
	}
	
	
	
//----
	
	/* 
	 * Constructs an empty rank, that does not hold any groups.
	 */
	rank::rank ()
		{ this->main_group = nullptr; }
	
	/* 
	 * Constructs a new rank from the given group string (in the form of
	 * <group1>;<group2>; ... ;<groupN>).
	 */
	rank::rank (const char *group_str, group_manager& groups)
	{
		this->set (group_str, groups);
	}
	
	/* 
	 * Copy constructor.
	 */
	rank::rank (const rank& other)
	{
		this->set (other);
	}
	
	
	
	/* 
	 * Returns a group string representation of this rank object (groups names
	 * seperated by semicolons).
	 */
	void
	rank::get_string (std::string& out)
	{
		out.clear ();
		for (auto& p : this->groups)
			{
				group *grp = p.grp;
				group_ladder *ladder = p.ladder;
				
				if (!out.empty ())
					out.push_back (';');
				
				if (grp == this->main_group)
					out.push_back ('@');
				out.append (grp->name);
				
				// ladder
				if (ladder)
					{
						out.push_back ('[');
						out.append (ladder->name);
						out.push_back (']');
					}
			}
	}
	
	
	
	/* 
	 * Inserts all the groups contained within the given group string
	 * (in the form of: <group1>;<group2>;...<groupN>). And previous
	 * groups that were held by the rank are removed.
	 */
	void
	rank::set (const char *group_str, group_manager& groups)
	{
		this->groups.clear ();
		this->main_group = nullptr;
		
		std::stringstream ss {group_str};
		std::string str;
		while (std::getline (ss, str, ';'))
			{
				std::string grp_name;
				std::string ladder_name;
				bool is_main = false;
				
				const char *ptr = str.c_str ();
				if (*ptr == '@')
					{
						is_main = true;
						++ ptr;
					}
				
				while (*ptr && (*ptr != '['))
					grp_name.push_back (*ptr++);
				if (*ptr == '[')
					{
						++ ptr;
						while (*ptr && (*ptr != ']'))
							ladder_name.push_back (*ptr++);
					}
				
				group_ladder *ladder = nullptr;
				if (!ladder_name.empty ())
					{
				 		ladder = groups.find_ladder (ladder_name.c_str ());
				 		if (!ladder)
				 			throw std::runtime_error ("ladder \"" + ladder_name + "\" does not exist");
					}
				
				group *grp = groups.find (grp_name.c_str ());
				if (!grp)
					throw std::runtime_error ("group \"" + grp_name + "\" does not exist");
				this->groups.push_back ({grp, ladder});
				if (is_main)
					this->main_group = grp;
			}
		
		if (!this->main_group && !this->groups.empty ())
			this->main_group = this->groups[0].grp;
	}
	
	void
	rank::set (const rank& other)
	{
		if (this == &other)
			return;
		
		this->groups = other.groups;
		this->main_group = other.main_group;
	}
	
	void
	rank::operator= (const rank& other)
	{
		this->set (other);
	}
	
	
	
	/* 
	 * Searches through the rank's group list for the highest power field.
	 */
	int
	rank::power () const
	{
		group *grp = this->highest ();
		return grp ? grp->power : 0;
	}
	
	/* 
	 * Returns the group that has the highest power field.
	 */
	group*
	rank::highest () const
	{
		if (this->groups.size () == 0)
			return nullptr;
		
		group *max = this->groups[0].grp;
		for (auto itr = this->groups.begin () + 1; itr != this->groups.end (); ++itr)
			{
				group *grp = (*itr).grp;
				if (grp->power > max->power)
					max = grp;
			}
		
		return max;
	}
	
	
	/* 
	 * Checks whether one or more of the groups contained in this rank have
	 * the given permission node registered.
	 */
	bool
	rank::has (const char *perm) const
	{
		if (this->groups.empty ())
			return false;
		
		const permission_manager& perm_man = this->groups[0].grp->perm_man;
		permission perm_struct = perm_man.get (perm);
		if (!perm_struct.valid ())
			return false;
		
		for (rgroup g : this->groups)
			{
				if (g.grp->has (perm_struct))
					return true;
			}
		
		return false;
	}
	
	
	
	/* 
	 * Comparison between rank objects:
	 */
	
	bool
	rank::operator== (const rank& other) const
	{
		return (this->groups == other.groups);
	}
	
	bool
	rank::operator!= (const rank& other) const
	{
		return (this->groups != other.groups);
	}
	
	bool
	rank::operator< (const rank& other) const
	{
		return (this->power () < other.power ());
	}
	
	bool
	rank::operator> (const rank& other) const
	{
		return (this->power () > other.power ());
	}
	
	bool
	rank::operator<= (const rank& other) const
	{
		return (this->power () <= other.power ());
	}
	
	bool
	rank::operator>= (const rank& other) const
	{
		return (this->power () >= other.power ());
	}
	
	
	
//-----------------------------------------------------------------------------
	
	/* 
	 * Returns the group that comes right after the specified one in the ladder.
	 * If the given group does not have a successor in the ladder, null will be
	 * returned.
	 */
	group*
	group_ladder::successor (group *grp)
	{
		if (!grp) return nullptr;
		for (size_t i = 0; i < this->groups.size (); ++i)
			if (this->groups[i] == grp)
				{
					if (i == (this->groups.size () - 1))
						return nullptr;
					return this->groups[i + 1];
				}
		
		return nullptr;
	}
	
	/* 
	 * Returns the group that comes right before the specified one in the ladder.
	 * If the given group does not have a predecessor in the ladder, null will be
	 * returned.
	 */
	group*
	group_ladder::predecessor (group *grp)
	{
		if (!grp) return nullptr;
		for (size_t i = 0; i < this->groups.size (); ++i)
			if (this->groups[i] == grp)
				{
					if (i == 0)
						return nullptr;
					return this->groups[i - 1];
				}
		
		return nullptr;
	}
	
	
	
	/* 
	 * Inserts group @{grp} right after group @{pos}, making group @{grp} the
	 * successor of group @{pos}.
	 */
	void
	group_ladder::insert_after (group *pos, group *grp)
	{
		if (!grp) return;
		if (!pos)
			{
				auto itr = this->groups.begin ();
				if (itr == this->groups.end ())
					this->groups.push_back (grp);
				else
					{
						++ itr;
						this->groups.insert (itr, grp);
					}
				return;
			}
		
		bool found = false, inserted = false;
		for (auto itr = this->groups.begin (); itr != this->groups.end (); ++itr)
			{
				if (found)
					{
						this->groups.insert (itr, grp);
						inserted = true;
						break;
					}
				else if (*itr == pos)
					found = true;
			}
		
		if (found && !inserted)
			this->groups.push_back (grp);
	}
	
	/* 
	 * Inserts group @{grp} right before group @{pos}, making grou @{grp} the
	 * predecessor of group @{pos}.
	 */
	void
	group_ladder::insert_before (group *pos, group *grp)
	{
		if (!grp) return;
		if (!pos)
			{
				this->groups.push_back (grp);
				return;
			}
		
		for (auto itr = this->groups.begin (); itr != this->groups.end (); ++itr)
			{
				if (*itr == pos)
					{
						this->groups.insert (itr, grp);
						break;
					}
			}
	}
	
	/* 
	 * Inserts the specified group to the end of the ladder.
	 */
	void
	group_ladder::insert (group *grp)
	{
		this->groups.push_back (grp);
	}
	
	
	
	/* 
	 * Returns the group with the highest power value in the ladder.
	 */
	group*
	group_ladder::highest ()
	{
		if (this->groups.empty ()) return nullptr;
		
		group *c = this->groups[0];
		for (size_t i = 1; i < this->groups.size (); ++i)
			{
				if (this->groups[i]->power > c->power)
					c = this->groups[i];
			}
		
		return c;
	}
	
	/* 
	 * Returns the group with the lowest power value in the ladder.
	 */
	group*
	group_ladder::lowest ()
	{
		if (this->groups.empty ()) return nullptr;
		
		group *c = this->groups[0];
		for (size_t i = 1; i < this->groups.size (); ++i)
			{
				if (this->groups[i]->power < c->power)
					c = this->groups[i];
			}
		
		return c;
	}
	
	
	
	/* 
	 * Checks whether the ladder contain the specified group.
	 */
	bool
	group_ladder::has_group (group *grp)
	{
	 	for (group *g : this->groups)
			if (grp == g)
				return true;
		return false;
	}
}

