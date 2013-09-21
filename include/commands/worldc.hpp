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

#ifndef _hCraft__COMMANDS__WORLD_H_
#define _hCraft__COMMANDS__WORLD_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /wcreate -
		 * 
		 * Creates a new world, and if requested, loads it into the current world
		 * list.
		 * 
		 * Permissions:
		 *   - command.world.wcreate
		 *       Needed to execute the command.
		 */
		class c_wcreate: public command
		{
		public:
			const char* get_name () { return "wcreate"; }
			
			const char** get_aliases ()
			{
				static const char* aliases[] =
					{
						"create-world",
						"world-create",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Creates a new world, and if requested, also loads it into the "
								 "server's world list."; }
								 
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.wcreate"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /wload -
		 * 
		 * Loads a world from disk onto the server's world list. Doing this enables
		 * players to go to that world using /w.
		 * 
		 * Permissions:
		 *   - command.world.wload
		 *       Needed to execute the command.
		 */
		class c_wload: public command
		{
		public:
			const char* get_name () { return "wload"; }
			
			const char**
			get_aliases ()
			{
				static const char* aliases[] =
					{
						"load-world",
						"world-load",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Loads a world from disk into the server's online world list."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.wload"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /wunload -
		 * 
		 * Saves and removes a requested world from the server's online world world
		 * list, optionally removing it from the autoload list as well.
		 * 
		 * Permissions:
		 *   - command.world.wunload
		 *       Needed to execute the command.
		 */
		class c_wunload: public command
		{
		public:
			const char* get_name () { return "wunload"; }
			
			const char**
			get_aliases ()
			{
				static const char* aliases[] =
					{
						"unload-world",
						"world-unload",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Saves and removes a requested world from the server's online "
								 "world list, optionally removing it from the autoload list as "
								 "well."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.wunload"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /goto - 
		 * 
		 * Teleports the player to a requested world.
		 * 
		 * Permissions:
		 *   - command.world.goto
		 *       Needed to execute the command.
		 */
		class c_goto: public command
		{
		public:
			const char* get_name () { return "goto"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"g",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Teleports the player to a requested world."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.goto"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /tp -
		 * 
		 * Teleports the player to a requested location.
		 * 
		 * Permissions:
		 *   - command.world.tp
		 *       Needed to execute the command.
		 *   - command.world.tp.others
		 *       Allows the user to teleport players other than themselves.
		 */
		class c_tp: public command
		{
		public:
			const char* get_name () { return "tp"; }
			
			const char*
			get_summary ()
				{ return "Teleports the player to a requested location."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.tp"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /physics -
		 * 
		 * Changes the current state of physics of the player's world.
		 * 
		 * Permissions:
		 *   - command.world.physics
		 *       Needed to execute the command.
		 */
		class c_physics: public command
		{
		public:
			const char* get_name () { return "physics"; }
			
			const char*
			get_summary ()
				{ return "Changes the current state of physics of the player's world."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.physics"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /wsetspawn -
		 * 
		 * Changes a world's default spawn position.
		 * 
		 * Permissions:
		 *   - command.world.wsetspawn
		 *       Needed to execute the command.
		 */
		class c_wsetspawn: public command
		{
		public:
			const char* get_name () { return "wsetspawn"; }
			
			const char*
			get_summary ()
				{ return "Changes a world's default spawn position."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.wsetspawn"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		// Should this command even be in the world subdirectory?
		/* 
		 * /block-physics -
		 * 
		 * Modifies physics properties for individual blocks.
		 * 
		 * Permissions:
		 *   - command.world.block-physics
		 *       Needed to execute the command.
		 */
		class c_block_physics: public command
		{
		public:
			const char* get_name () { return "block-physics"; }
			
			const char**
			get_aliases ()
			{
				static const char* aliases[] =
					{
						"bp",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Modifies physics properties for individual blocks."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.block-physics"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		// Should this command even be in the world subdirectory?
		/* 
		 * /block-type -
		 * 
		 * Modifies the type of a selected block.
		 * 
		 * Permissions:
		 *   - command.world.block-type
		 *       Needed to execute the command.
		 */
		class c_block_type: public command
		{
		public:
			const char* get_name () { return "block-type"; }
			
			const char**
			get_aliases ()
			{
				static const char* aliases[] =
					{
						"bt",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Modifies the type of a selected block."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.block-type"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /portal - 
		 * 
		 * Turns blocks in the user's selected area to portals.
		 * 
		 * Permissions:
		 *   - command.world.portal
		 *       Needed to execute the command.
		 *   - command.world.portal.interworld
		 *       Needed to place portals between different worlds.
		 */
		class c_portal : public command
		{
		public:
			const char* get_name () { return "portal"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"ptl",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Turns blocks in the user's selected area to portals."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.portal"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /world - 
		 * 
		 * Lets the user modify or view world related information.
		 * 
		 * Permissions:
		 *   - command.world.world
		 *       Needed to execute the command.
		 *   - command.world.world.change-members
		 *       Needed to add\remove world members.
		 *   - command.world.world.change-owners
		 *       Needed to add\remove world owners.
		 *   - command.world.world.owner.change-members
		 *       If a world owner is allowed to add\remove members.
		 *   - command.world.world.owner.change-owners
		 *       If a world owner is allowed to add\remove owners.
		 *   - command.world.world.set-perms
		 *       Required to set build-perms or join-perms
		 *   - command.world.world.get-perms
		 *       Required to view build-perms or join-perms
		 *   - commands.world.world.resize
		 *       Required to resize the world.
		 *   - commands.world.world.regenerate
		 *       Required to regenerate the world.
		 */
		class c_world : public command
		{
		public:
			const char* get_name () { return "world"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"w",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Lets the user modify or view world related information."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.world"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		
		/* 
		 * /realm - 
		 * 
		 * Inspired by WOM realms.
		 * Lets players visit realms (worlds).
		 * If a player tries to access a realm/world that matches their username,
		 * it will be created automatically if it did not already exist.
		 * 
		 * Permissions:
		 *   - command.world.realm
		 *       Needed to execute the command.
		 */
		class c_realm : public command
		{
		public:
			const char* get_name () { return "realm"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Teleports the player to a requested realm."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.world.realm"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

