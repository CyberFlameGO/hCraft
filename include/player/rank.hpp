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

#ifndef _hCraft__RANK_H_
#define _hCraft__RANK_H_

#include "permissions.hpp"
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>


namespace hCraft {
	
	class rank_error: public std::runtime_error {
	public:
		rank_error (const std::string& what)
			: std::runtime_error (what)
			{ }
	};
	
	class rank_load_error: public std::runtime_error {
	public:
		rank_load_error (const std::string& what)
			: std::runtime_error (what)
			{ }
	};
	
	
	struct group_ladder;
	
	/* 
	 * 
	 */
	struct group
	{
		int  id;         // unique id
		int  power;      // groups are sorted using this field (higher power = higher-ranked group).
		std::string name;
		char color;      // name color.
		char text_color;
		group_ladder *ladder;
		
		// 32 chars max
		std::string prefix, suffix;
		std::string mprefix, msuffix; // used only if the group is the main group.
		
		bool can_chat;       // if players of this group can send chat messages.
		bool can_build;      // whether players of this group can modify blocks.
		bool can_move;       // whether the players can move.
		
		bool color_codes;
		
		int fill_limit;
		int select_limit;
		
	//----
		
		bool all_perms; // true if this group has the '*' perm.
		bool no_perms;  // opposite
		
		permission_manager& perm_man;
		std::unordered_set<permission> perms;
		std::unordered_set<permission> neg_perms; // negated perms are stored separately
		std::vector<group *> parents;
		
	//----
		/* 
		 * Class constructor.
		 */
		group (permission_manager& perm_man, int id, int power, const char *name);
		
		
		/* 
		 * Adds the specified permission node into the group's permissions list.
		 */
		void add (permission perm);
		void add (const char *perm);
		
		/* 
		 * Adds all permission nodes contained within the specified null-terminated
		 * array into the group's permissions list.
		 */
		void add (const char **perms);
		
		
		/* 
		 * Checks whether this group has the given permission node.
		 */
		bool has (permission perm) const;
		bool has (const char *str) const;
		
		
		/* 
		 * Inherits all permissions from the specified group.
		 */
		void inherit (group *grp);
		
		
	//----
		
		inline bool
		operator< (const group& other) const
			{ return this->power < other.power; }
		
		inline bool
		operator> (const group& other) const
			{ return this->power > other.power; }
		
		inline bool
		operator<= (const group& other) const
			{ return this->power <= other.power; }
		
		inline bool
		operator>= (const group& other) const
			{ return this->power > other.power; }
		
		inline bool
		operator== (const group& other) const
			{ return this->id == other.id; }
		
		inline bool
		operator!= (const group& other) const
			{ return this->id != other.id; }
	};
	
	
	
	/* 
	 * A sorted sequence of groups that define a ranking ladder.
	 */
	struct group_ladder
	{
		std::string name;
		std::vector<group *> groups;
		
	//----
		group_ladder () { }
		
		
		
		/* 
		 * Returns the group that comes right after the specified one in the ladder.
		 * If the given group does not have a successor in the ladder, null will be
		 * returned.
		 */
		group* successor (group *grp);
		
		/* 
		 * Returns the group that comes right before the specified one in the ladder.
		 * If the given group does not have a predecessor in the ladder, null will be
		 * returned.
		 */
		group* predecessor (group *grp);
		
		/* 
		 * Checks whether the ladder has the given group.
		 */
		bool has (group *grp);
		
		
		
		/* 
		 * Inserts group @{grp} right after group @{pos}, making group @{grp} the
		 * successor of group @{pos}.
		 */
		void insert_after (group *pos, group *grp);
		
		/* 
		 * Inserts group @{grp} right before group @{pos}, making grou @{grp} the
		 * predecessor of group @{pos}.
		 */
		void insert_before (group *pos, group *grp);
		
		/* 
		 * Inserts the specified group to the end of the ladder.
		 */
		void insert (group *grp);
		
		void clear ()
			{ this->groups.clear (); }
		
		
		
		/* 
		 * Returns the group with the highest power value in the ladder.
		 */
		group* highest ();
		
		/* 
		 * Returns the group with the lowest power value in the ladder.
		 */
		group* lowest ();
		
		/* 
		 * Checks whether the ladder contain the specified group.
		 */
		bool has_group (group *grp);
	};
	
	
	
	class group_manager; // forward def
	
	/* 
	 * 
	 */
	struct rank
	{
		std::vector<group *> groups;
		group *main_group;
		
	public:
		/* 
		 * Constructs an empty rank, that does not hold any groups.
		 */
		rank ();
		
		/* 
		 * Constructs a new rank from the given group string (in the form of
		 * <group1>;<group2>; ... ;<groupN>).
		 */
		rank (const char *group_str, group_manager& groups);
		
		/* 
		 * Copy constructor.
		 */
		rank (const rank& other);
		
		
		/* 
		 * Returns a group string representation of this rank object (groups names
		 * seperated by semicolons).
		 */
		void get_string (std::string& out) const;
		void get_colored_string (std::string& out) const;
		
		
		/* 
		 * Inserts all the groups contained within the given group string
		 * (in the form of: <group1>;<group2>;...<groupN>). And previous
		 * groups that were held by the rank are removed.
		 */
		void set (const char *group_str, group_manager& groups);
		
		void set (const rank& other);
		void operator= (const rank& other);
		
		/* 
		 * Finds and replaces group @{orig} with @{grp}.
		 */
		void replace (group *orig, group *grp);
		
		
		/* 
		 * Searches through the rank's group list for the highest power field.
		 */
		int power () const;
		
		int fill_limit () const;
		int select_limit () const;
		
		
		
		/* 
		 * Returns the group that has the highest power field.
		 */
		group* highest () const;
		group* main () const
			{ return this->main_group; }
		
		/* 
		 * Checks whether one or more of the groups contained in this rank have
		 * the given permission node registered.
		 */
		bool has (const char *perm) const;
		
		/* 
		 * Checks whether the rank contains the specified group.
		 */
		bool contains (group *grp) const;
		
	//---
		/* 
		 * Comparison between rank objects:
		 */
		
		bool operator== (const rank& other) const;
		bool operator!= (const rank& other) const;
		bool operator< (const rank& other) const;
		bool operator> (const rank& other) const;
		bool operator<= (const rank& other) const;
		bool operator>= (const rank& other) const;
	};
	
	
	
	/* 
	 * Manages a collection of groups.
	 */
	class group_manager
	{
		permission_manager& perm_man;
		
		std::unordered_map<std::string, group *> groups;
		int id_counter;
		
		std::unordered_map<std::string, group_ladder> ladders;
		
	public:
		rank default_rank;
		
	public:
		inline std::unordered_map<std::string, group *>::iterator
		begin()
			{ return this->groups.begin (); }
		
		inline std::unordered_map<std::string, group *>::iterator
		end()
			{ return this->groups.end (); }
		
		inline int size () const { return this->groups.size (); }
		
		
		inline permission_manager&
		get_permission_manager ()
			{ return this->perm_man; }
		
	public:
		/* 
		 * Class constructor.
		 */
		group_manager (permission_manager& perm_man);
		
		/* 
		 * Class destructor.
		 */
		~group_manager ();
		
		
		
		/* 
		 * Creates and return a new group.
		 */
		group* add (int power, const char *name);
		
		/* 
		 * Removes all groups from this manager.
		 */
		void clear ();
		
		
		
		/* 
		 * Searches for the group that has the specified name (case-sensitive).
		 */
		group* find (const char *name);
		
		/* 
		 * Returns the group with the highest power value.
		 */
		group* highest ();
		
		/* 
		 * Returns the group with the lowest power value.
		 */
		group* lowest ();
		
		
		
		/* 
		 * Attempts to find a suitable ladder for the given group.
		 */
		group_ladder* default_ladder (group *grp);
		
		/* 
		 * Finds a group ladder by its name.
		 */
		group_ladder* find_ladder (const char *name);
		
		/* 
		 * Creates and return a new group ladder.
		 */
		group_ladder* add_ladder (const char *name);
		
		
		
		/* 
		 * Saves all rank information into the specified file.
		 */
		void save (const char *file_name);

		/* 
		 * Loads rank information from the specified file.
		 */		
		void load (const char *file_name);
	};
}

#endif

