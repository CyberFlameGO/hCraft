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

#ifndef _hCraft__WORLDPROVIDER_H_
#define _hCraft__WORLDPROVIDER_H_

#include "util/position.hpp"
#include "world/portal.hpp"
#include <vector>
#include <utility>
#include <string>
#include <stdexcept>


namespace hCraft {
	
	class chunk;
	class world;
	class world_security;
	
	
	/* 
	 * Fields required to be saved to\loaded from all formats.
	 */
	struct world_information
	{
		std::string world_type;
		std::string def_gm;
		std::string def_inv;
		bool use_def_inv;
		
		long long time;
		bool time_frozen;
		
		int width;
		int depth;
		entity_pos spawn_pos;
		
		int chunk_count;
		
		std::string generator;
		int seed;
	};
	
	
	class world_provider_naming
	{
	public:
		virtual ~world_provider_naming () {}
		virtual const char* provider_name () = 0;
		
		
		/* 
		 * Returns true if the format is stored within a separate directory
		 * (like Anvil).
		 */
		virtual bool is_directory_format () = 0;
			
		/* 
		 * Adds required prefixes, suffixes, etc... to the specified world name so
		 * that the importer's claims_name () function returns true when passed to
		 * it.
		 */
		virtual std::string make_name (const char *world_name) = 0;
		
		/* 
		 * Checks whether the specified path name meets the format required by this
		 * exporter (could be a name prefix, suffix, extension, etc...).
		 */
		virtual bool claims_name (const char *path) = 0;
		
		
		
		static world_provider_naming* create (const char *name);
	};
	
	
	
	/* 
	 * Thrown by a world provider on load failure.
	 */
	class world_load_error: public std::runtime_error
	{
	public:
		world_load_error (const std::string& str)
			: std::runtime_error (str)
			{ }
	};
	
	
	/* 
	 * Abstract base class for all world importer\exporter implementations.
	 */
	class world_provider
	{
	public:
		virtual ~world_provider () { }
		
		
		/* 
		 * Returns the name of this world provider.
		 */
		virtual const char* name () = 0;
		
		
		
		/* 
		 * Opens the underlying file stream for reading\writing.
		 * By using open () and close (), multiple chunks can be read\written
		 * without reopening the world file everytime.
		 */
		virtual void open (world &wr) = 0;
		
		/* 
		 * Closes the underlying file stream.
		 */
		virtual void close () = 0;
		
		
		
		/* 
		 * Saves only the specified chunk.
		 */
		virtual void save (world& wr, chunk *ch, int x, int z) = 0;
		
		/* 
		 * Saves the specified world without writing out any chunks.
		 * NOTE: If a world file already exists at the destination path, an empty
		 *       template will NOT be written out.
		 */
		virtual void save_empty (world &wr) = 0; 
		
		/* 
		 * Updates world information for a given world. 
		 */
		virtual void save_info (world &w, const world_information &info) = 0;
		
		
		
		/* 
		 * Updates\saves owner\member list, build\join permissions, etc...
		 */
		virtual void save_security (world &w, const world_security& sec) = 0;
		
		/* 
		 * Loads world security information.
		 */
		virtual void load_security (world &w, world_security& sec) = 0;
		
		
		
		/* 
		 * Saves the specified list of portals to disk.
		 */
		virtual void save_portals (world &wr, const std::vector<portal *>& portals) = 0;
		
		/* 
		 * Loads the portal list from disk into the given vector.
		 */
		virtual void load_portals (world &wr, std::vector<portal *>& portals) = 0;
		
		
		
		/* 
		 * Opens the file located at path @{path} and performs a check to see if it
		 * is of the same format created by this exporter.
		 */
		virtual bool claims (const char *path) = 0;
		
		/* 
		 * Attempts to load the chunk located at the specified coordinates into
		 * @{ch}. Returns true on success, and false if the chunk is not present
		 * within the world file.
		 */
		virtual bool load (world &wr, chunk *ch, int x, int z) = 0;
		
		/* 
		 * Returns a structure that contains essential information about the
		 * underlying world.
		 */
		virtual const world_information& info () = 0;
		
		
		
		/* 
		 * Returns the filesystem path to the world file.
		 */
		virtual const char* get_path () = 0;
		
		
		
		/* 
		 * Returns a new instance of the world provider named @{name}.
		 * @{path} specifies the directory to which the world should be exported to/
		 * imported from.
		 */
		static world_provider* create (const char *name, const char *path,
			const char *world_name);
		
		/* 
		 * Attempts to determine the type of provider used by the world that has
		 * the specified name (the world must already exist). On success, the name
		 * of the provider is returned; otherwise, an empty string is returned.
		 */
		static std::string determine (const char *path,
			const char *world_name);
	};
}

#endif

