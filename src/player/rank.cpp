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

#include "player/rank.hpp"
#include "util/config.hpp"
#include <cstring>
#include <sstream>
#include <string>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <memory>
#include <vector>
#include <unordered_map>


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
		this->ladder = nullptr;
		this->color_codes = false;
		this->fill_limit = 0;
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
				if (perm.nodes[0] == PERM_WILD_ALL && perm.nodes[1] == 0)
					{
						this->all_perms = false;
						this->no_perms = true;
					}
				this->neg_perms.insert (perm);
			}
		else
			{
				if (perm.nodes[0] == PERM_WILD_ALL && perm.nodes[1] == 0)
					{
						this->all_perms = true;
						this->no_perms = false;
					}
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
		else if (this->all_perms)
			return true;
		if (!perm.valid ())
			{
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
							{
								if (perm.nodes[i] != 0)
									match = false;
								break;
							}
						
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
							{
								if (perm.nodes[i] != 0)
									match = false;
								break;
							}
						
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
	 * Returns the group with the highest power value.
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
	 * Returns the group with the lowest power value.
	 */
	group*
	group_manager::lowest ()
	{
		if (this->groups.empty ())
			return nullptr;
		
		auto itr = this->groups.begin ();
		group *lowest = itr->second;
		
		for (++itr; itr != this->groups.end (); ++itr)
			if (itr->second->power < lowest->power)
				lowest = itr->second;
		
		return lowest;
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
		group_ladder &ladder = this->ladders[name];
		ladder.name.assign (name);
		return &ladder;
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
	rank::get_string (std::string& out) const
	{
		out.clear ();
		for (group *grp : this->groups)
			{
				if (!out.empty ())
					out.push_back (';');
				
				if (grp == this->main_group)
					out.push_back ('@');
				out.append (grp->name);
				
				// ladder
				if (grp->ladder)
					{
						out.push_back ('[');
						out.append (grp->ladder->name);
						out.push_back (']');
					}
			}
	}
	
	void
	rank::get_colored_string (std::string& out) const
	{
		out.clear ();
		for (group *grp : this->groups)
			{
				if (!out.empty ())
					out.append ("§7;");
				
				if (grp == this->main_group)
					out.append ("§c@");
				out.append ("§");
				out.push_back (grp->color);
				out.append (grp->name);
				
				// ladder
				if (grp->ladder)
					{
						out.append ("§8[");
						out.append (grp->ladder->name);
						out.append ("§8]");
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
				
				group *grp = groups.find (grp_name.c_str ());
				if (!grp)
					throw std::runtime_error ("group \"" + grp_name + "\" does not exist");
				
				group_ladder *ladder = nullptr;
				if (!ladder_name.empty ())
					{
				 		ladder = groups.find_ladder (ladder_name.c_str ());
				 		if (!ladder)
				 			throw std::runtime_error ("ladder \"" + ladder_name + "\" does not exist");
					}
				else
					ladder = groups.default_ladder (grp);
				grp->ladder = ladder;
				
				this->groups.push_back (grp);
				if (is_main)
					this->main_group = grp;
			}
		
		if (!this->main_group && !this->groups.empty ())
			this->main_group = this->groups[0];
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
	 * Finds and replaces group @{orig} with @{grp}.
	 */
	void
	rank::replace (group *orig, group *grp)
	{
		if (!orig || !grp) return;
		
		for (size_t i = 0; i < this->groups.size (); ++i)
			if (this->groups[i] == orig)
				this->groups[i] = grp;
		if (this->main_group == orig)
			this->main_group = grp;
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
	
	int
	rank::fill_limit () const
	{
		int max = 0;
		for (auto itr = this->groups.begin (); itr != this->groups.end (); ++itr)
			if ((*itr)->fill_limit > max)
				max = (*itr)->fill_limit;
		return max;
	}
	
	int
	rank::select_limit () const
	{
		int max = 0;
		for (auto itr = this->groups.begin (); itr != this->groups.end (); ++itr)
			if ((*itr)->select_limit > max)
				max = (*itr)->select_limit;
		return max;
	}
	
	
	
	/* 
	 * Returns the group that has the highest power field.
	 */
	group*
	rank::highest () const
	{
		if (this->groups.size () == 0)
			return nullptr;
		
		group *max = this->groups[0];
		for (auto itr = this->groups.begin () + 1; itr != this->groups.end (); ++itr)
			{
				group *grp = *itr;
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
		
		permission_manager& perm_man = this->groups[0]->perm_man;
		permission perm_struct = perm_man.add (perm);
		if (!perm_struct.valid ())
			{
				for (group *grp : this->groups)
					if (grp->all_perms)
						return true;
				
				return false;
			}
		
		for (group *grp : this->groups)
			{
				if (grp->has (perm_struct))
					return true;
			}
		
		return false;
	}
	
	/* 
	 * Checks whether the rank contains the specified group.
	 */
	bool
	rank::contains (group *grp) const
	{
		for (group *g : this->groups)
			if (g == grp)
				return true;
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
	 * Checks whether the ladder has the given group.
	 */
	bool
	group_ladder::has (group *grp)
	{
		for (size_t i = 0; i < this->groups.size (); ++i)
			if (this->groups[i] == grp)
				return true;
		return false; 
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
	
	
	
//-----
	/* 
	 * Saves all rank information into the specified file.
	 */
	void
	group_manager::save (const char *file_name)
	{
		cfg::group root {2};
		
		// default-rank
		{
			std::string def_rank;
			this->default_rank.get_string (def_rank);
			root.add_string ("default-rank", def_rank);
		}
		
		// ladders
		{
			cfg::group *grp_ladders = new cfg::group ();
			for (auto itr = this->ladders.begin (); itr != this->ladders.end (); ++itr)
				{
					cfg::array *arr = new cfg::array ();
					group_ladder& lad = itr->second;
					
					// start with the lowest and go upwards
					group *g = lad.lowest ();
					while (g)
						{
							arr->add_string (g->name);
							g = lad.successor (g);
						}
					
					grp_ladders->add (itr->first, arr);
				}
			
			root.add ("ladders", grp_ladders);
		}
		
		// groups
		{
			cfg::group *grp_groups = new cfg::group (1);
			
			// create a sorted list of groups (in ascending values of power)
			std::vector<group *> group_list;
			for (auto itr = this->groups.begin (); itr != this->groups.end (); ++itr)
				group_list.push_back (itr->second);
			std::sort (group_list.begin (), group_list.end (),
				[] (const group *a, const group *b) -> bool
					{
						return (*a) < (*b);
					});
			
			for (group *grp : group_list)
				{
					cfg::group *curr = new cfg::group ();
					
					curr->add_integer ("power", grp->power);
					
					// inheritance
					if (!grp->parents.empty ())
						{
							cfg::array *arr = new cfg::array ();
							for (group *parent : grp->parents)
								arr->add_string (parent->name);
							curr->add ("inheritance", arr);
						}
					
					if (grp->color != 'f')
						curr->add_string ("color", std::string (1, grp->color));
					if (grp->text_color != 'f')
						curr->add_string ("text-color", std::string (1, grp->text_color));
					
					if (!grp->mprefix.empty ())
						curr->add_string ("mprefix", grp->mprefix);
					if (!grp->msuffix.empty ())
						curr->add_string ("msuffix", grp->msuffix);
					if (!grp->prefix.empty ())
						curr->add_string ("prefix", grp->prefix);
					if (!grp->suffix.empty ())
						curr->add_string ("suffix", grp->suffix);
					
					if (!grp->can_chat)
						curr->add_boolean ("can-chat", grp->can_chat);
					if (!grp->can_build)
						curr->add_boolean ("can-build", grp->can_build);
					if (!grp->can_move)
						curr->add_boolean ("can-move", grp->can_move);
					if (grp->color_codes)
						curr->add_boolean ("color-codes", grp->color_codes);
						
					if (grp->fill_limit > 0)
						curr->add_integer ("fill-limit", grp->fill_limit);
					if (grp->select_limit > 0)
						curr->add_integer ("select-limit", grp->select_limit);
					
					// permissions
					{
						// sort permission list
						auto& perm_man = this->get_permission_manager ();
						std::vector<permission> perms (grp->perms.begin (), grp->perms.end ());
						perms.insert (perms.end (), grp->neg_perms.begin (), grp->neg_perms.end ());
						std::sort (perms.begin (), perms.end (),
							[&perm_man] (const permission& a, const permission& b) -> bool
								{ return (std::strcmp (perm_man.to_string (a).c_str (),
										perm_man.to_string (b).c_str ()) < 0); });
						
						cfg::array *arr = new cfg::array {1, 0};
						for (size_t i = 0; i < perms.size (); ++i)
							arr->add_string (perm_man.to_string (perms[i]));
						curr->add ("permissions", arr);
					}
					
					grp_groups->add (grp->name, curr);
				}
			
			root.add ("groups", grp_groups);
		}
		
		
		std::ofstream fs {file_name};
		root.write_to (fs);
		fs << std::endl;
		fs.close ();
	}
	
	
	
	using group_inh_map =
		std::unordered_map<std::string, std::vector<std::string>>;
	
	static void
	_load_group (const std::string& g_name, cfg::group& cg, group_manager& gman,
		group_inh_map& inh_map)
	{
		int g_power;
		try {
			g_power = cg.get_integer ("power");
		} catch (const std::exception&) {
			throw rank_load_error ("in group \"" + g_name
				+ "\": \"power\" not found, or is of an incorrect type");
		}
		
		char g_color;
		try {
			g_color = cg.get_string ("color").at (0);
		} catch (const std::exception&) {
			g_color = 'f';
		}
		
		char g_text_color;
		try {
			g_text_color = cg.get_string ("text-color").at (0);
		} catch (const std::exception&) {
			g_text_color = 'f';
		}
		
		std::string g_prefix;
		try {
			g_prefix = cg.get_string ("prefix");
		} catch (const std::exception&) {
			g_prefix = "";
		}
		
		std::string g_mprefix;
		try {
			g_mprefix = cg.get_string ("mprefix");
		} catch (const std::exception&) {
			g_mprefix = "";
		}
		
		std::string g_suffix;
		try {
			g_suffix = cg.get_string ("suffix");
		} catch (const std::exception&) {
			g_suffix = "";
		}
		
		std::string g_msuffix;
		try {
			g_msuffix = cg.get_string ("msuffix");
		} catch (const std::exception&) {
			g_msuffix = "";
		}
		
		bool g_can_build;
		try {
			g_can_build = cg.get_boolean ("can-build");
		} catch (const std::exception&) {
			g_can_build = true;
		}
		
		bool g_can_chat;
		try {
			g_can_chat = cg.get_boolean ("can-chat");
		} catch (const std::exception&) {
			g_can_chat = true;
		}
		
		bool g_can_move;
		try {
			g_can_move = cg.get_boolean ("can-move");
		} catch (const std::exception&) {
			g_can_move = true;
		}
		
		bool g_color_codes;
		try {
			g_color_codes = cg.get_boolean ("color-codes");
		} catch (const std::exception&) {
			g_color_codes = true;
		}
		
		
		int g_fill_limit;
		try {
			g_fill_limit = cg.get_integer ("fill-limit");
		} catch (const std::exception&) {
			g_fill_limit = 0;
		}
		
		int g_select_limit;
		try {
			g_select_limit = cg.get_integer ("select-limit");
		} catch (const std::exception&) {
			g_select_limit = 0;
		}
		
		
		std::vector<std::string> perms;
		try {
			auto arr = cg.find_array ("permissions");
			if (arr)
				{
					for (cfg::value *v : *arr)
						{
							if (v->type () != cfg::CFG_STRING)
								throw rank_load_error ("invalid permissions");
							perms.push_back (((cfg::string *)v)->val ());
						}
				}
		} catch (const rank_load_error& ex) {
			throw;
		} catch (const std::exception&) {
			;
		}
		
		// inheritance
		try {
			auto arr = cg.find_array ("inheritance");
			if (arr)
				{
					for (cfg::value *v : *arr)
						{
							if (v->type () != cfg::CFG_STRING)
								throw rank_load_error ("invalid permissions");
								
							std::string& parent = ((cfg::string *)v)->val ();
							inh_map[g_name].push_back (parent);
						}
				}
		} catch (const rank_load_error& ex) {
			throw;
		} catch (const std::exception& ex) {
			;
		}
		
		group *grp = gman.add (g_power, g_name.c_str ());
		grp->color = g_color;
		grp->text_color =  g_text_color;
		grp->prefix = g_prefix;
		grp->mprefix = g_mprefix;
		grp->suffix = g_suffix;
		grp->msuffix = g_msuffix;
		grp->can_build = g_can_build;
		grp->can_move = g_can_move;
		grp->can_chat = g_can_chat;
		grp->color_codes = g_color_codes;
		grp->fill_limit = g_fill_limit;
		grp->select_limit = g_select_limit;
		for (std::string& perm : perms)
			grp->add (perm.c_str ());
	}
	
	/* 
	 * Loads rank information from the specified file.
	 */		
	void
	group_manager::load (const char *file_name)
	{
		std::ifstream fs {file_name};
		if (!fs)
			throw rank_load_error (std::string ("failed to open \"") + file_name + "\"");
		
		std::unique_ptr<cfg::group> root {cfg::group::read_from (fs)};
		fs.close ();
		
		// load groups
		{
			cfg::group *grp_groups = root->find_group ("groups");
			if (!grp_groups)
				throw rank_load_error ("\"groups\" group not found");
			
			group_inh_map inh_map {};
			for (auto sett : *grp_groups)
				{
					if (sett.val->type () != cfg::CFG_GROUP)
						throw rank_load_error ("\"groups\" group has a non-group element");
					_load_group (sett.name, *dynamic_cast<cfg::group *> (sett.val), *this, inh_map);
				}
			
			// resolve inheritance
			for (auto itr = inh_map.begin (); itr != inh_map.end (); ++itr)
				{
					group *child = this->find (itr->first.c_str ());
					for (std::string& s_par : itr->second)
						{
							group *parent = this->find (s_par.c_str ());
							if (!parent)
								throw rank_load_error ("in group \"" + child->name
									+ "\": parent group \"" + s_par + "\" does not exist");
							
							child->inherit (parent);
						}
				}
		}
		
		// load ladders
		{
			cfg::group *grp_ladders = root->find_group ("ladders");
			if (grp_ladders)
				{
					for (auto sett : *grp_ladders)
						{
							if (sett.val->type () != cfg::CFG_ARRAY)
								throw rank_load_error ("invalid ladder \"" + sett.name + "\"");
							cfg::array *arr = (cfg::array *)sett.val;
							
							group_ladder *lad = this->add_ladder (sett.name.c_str ());
							
							for (cfg::value *v : *arr)
								{
									if (v->type () != cfg::CFG_STRING)
										throw rank_load_error ("invalid ladder \"" + sett.name + "\"");
									cfg::string *str = (cfg::string *)v;
									
									group *grp = this->find (str->val ().c_str ());
									if (!grp)
										throw rank_load_error ("in ladder \"" + sett.name
											+ "\": group \"" + grp->name + "\" does not exist.");
									lad->insert (grp);
								}
						}
				}
		}
		
		// load default-rank
		try {
			std::string def_rank = root->get_string ("default-rank");
			this->default_rank.set (def_rank.c_str (), *this);
		} catch (const std::exception&) {
			throw rank_load_error ("\"default-rank\" is either invalid or does not exist");
		}
	}
}

