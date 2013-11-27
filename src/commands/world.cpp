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

#include "commands/world.hpp"
#include "system/server.hpp"
#include "world/world.hpp"
#include "util/stringutils.hpp"
#include "system/sqlops.hpp"
#include "util/cistring.hpp"
#include "system/messages.hpp"
#include <cstdio>
#include <sys/stat.h>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <cmath>

#include <iostream> // DEBUG


namespace hCraft {
	namespace commands {
		
		static std::string
		_get_time_str (unsigned long long ticks)
		{
			std::ostringstream ss;
			ticks += 7000;
			int hours = (ticks / 1000) % 24;
			int minutes = (int)std::round (ticks / (16.0 + (2.0 / 3.0))) % 60;
			bool pm = (hours >= 12);
			if (hours > 12)
				hours -= 12;
			ss << hours << ":" << std::setw (2) << std::setfill ('0')
				<< minutes << std::setfill (' ') << " " << (pm ? "PM" : "AM");
			return ss.str ();
		}
		
		
		static void
		_print_names_from_pids (player *pl, const std::set<int>& pids)
		{
			std::ostringstream ss;
			
			ss << "    ";
			
			soci::session sql {pl->get_server ().sql_pool ()};
			
			sqlops::player_info pinf;
			for (int pid : pids)
				{
					if (!sqlops::player_data (sql, pid, pl->get_server (), pinf))
						ss << "§8??? ";
					else
						{
							ss << "§" << pinf.rnk.main ()->color << pinf.name << " ";
						}
				}
			
			pl->message_spaced (ss.str ());
		}
		
		
		
		static void
		_handle_no_args (player *pl, world *w)
		{
			std::ostringstream ss;
			
			world_security& sec = w->security ();
			
			// header
			ss << "§6Displaying world information for " << w->get_colored_name () << "§e:";
			pl->message (ss.str ());
			ss.str (std::string ());
			
			// type
			ss << "§e  World type§f: §9" << ((w->get_type () == WT_NORMAL) ? "normal" : "light");
			pl->message (ss.str ());
			ss.str (std::string ());
			
			// dimensions
			ss << "§e  Dimensions§f: §a";
			if (w->get_width () <= 0)
				ss << "infinite";
			else
				ss << w->get_width ();
			ss << " §7x §a";
			if (w->get_depth () <= 0)
				ss << "infinite";
			else
				ss << w->get_depth ();
			pl->message (ss.str ());
			ss.str (std::string ());
			
			// time
			pl->message ("§e  Time§f: §9" + _get_time_str (w->get_time ()) + (w->is_time_frozen () ? " [Frozen]" : ""));
			
			// owners
			const auto& owner_pids = sec.get_owners ();
			if (owner_pids.empty ())
				pl->message ("§e  Owners§f: §8none");
			else
				{
					pl->message ("§e  Owners§f:");
					_print_names_from_pids (pl, owner_pids);
				}
			
			// members
			const auto& member_pids = sec.get_members ();
			if (member_pids.empty ())
				pl->message ("§e  Members§f: §8none");
			else
				{
					pl->message ("§e  Members§f:");
					_print_names_from_pids (pl, member_pids);
				}
		}
		
		
		
		static void
		_handle_owners (player *pl, world *w, command_reader& reader)
		{
		  world_security& sec = w->security ();
		  
			if (!reader.has_next ())
				{
					std::ostringstream ss;
			    
			    ss << "§6Displaying owners for " << w->get_colored_name () << "§e:";
			    pl->message (ss.str ());
			    ss.str (std::string ());
			    
			    const auto& owner_pids = sec.get_owners ();
			    if (owner_pids.empty ())
			    	pl->message ("§8    none");
			    else
			    	_print_names_from_pids (pl, owner_pids);
			    return;
				}
			
			const std::string arg1 = reader.next ().as_str ();
			if (sutils::iequals (arg1, "add"))
			  {
			  	if (!pl->has ("command.world.world.change-owners")
			  		&& !(sec.is_owner (pl->pid ()) && pl->has ("command.world.world.owner.change-owners")))
			  		{
			  			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
			  		}
			  	
			    if (!reader.has_next ())
            {
              pl->message ("§c * §7Usage§f: §e/world owners add §cplayer");
              return;
            }
           
           const std::string arg2 = reader.next ().as_str ();
           
           // fetch player info
           soci::session sql {pl->get_server ().sql_pool ()};
           sqlops::player_info pinf;
           if (!sqlops::player_data (sql, arg2.c_str (), pl->get_server (), pinf))
            {
              pl->message ("§c * §7Unknown player§f: §c" + arg2);
              return;
            }
            
            if (sec.is_owner (pinf.id))
              {
                std::ostringstream ss;
                ss << "§c * §" << pinf.rnk.main ()->color << pinf.name << " §7is already an owner of this world§c.";
                pl->message (ss.str ());
                return;
              }
              
            sec.add_owner (pinf.id);
            std::ostringstream ss;
            ss << "§" << pinf.rnk.main ()->color << pinf.name << " §ehas been added to the world§f'§es owner list§f.";
            pl->message (ss.str ());
			  }
			 else if (sutils::iequals (arg1, "remove") || sutils::iequals (arg1, "del"))
			  {
			  	if (!pl->has ("command.world.world.change-owners")
			  		&& !(sec.is_owner (pl->pid ()) && pl->has ("command.world.world.owner.change-owners")))
			  		{
			  			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
			  		}
			  	
			    if (!reader.has_next ())
            {
              pl->message ("§c * §7Usage§f: §e/world owners remove §cplayer");
              return;
            }
           
           const std::string arg2 = reader.next ().as_str ();
           
           // fetch player info
           soci::session sql {pl->get_server ().sql_pool ()};
           sqlops::player_info pinf;
           if (!sqlops::player_data (sql, arg2.c_str (), pl->get_server (), pinf))
            {
              pl->message ("§c * §7Unknown player§f: §c" + arg2);
              return;
            }
            
            if (!sec.is_owner (pinf.id))
              {
                std::ostringstream ss;
                ss << "§c * §" << pinf.rnk.main ()->color << pinf.name << " §7is not an owner of this world§c.";
                pl->message (ss.str ());
                return;
              }
              
            sec.remove_owner (pinf.id);
            std::ostringstream ss;
            ss << "§" << pinf.rnk.main ()->color << pinf.name << " §ehas been removed from the world§f'§es owner list§f.";
            pl->message (ss.str ());
			  }
			 else
			  {
			    pl->message ("§c * §7Unknown sub-command§f: §cowners." + arg1);
			  }
		}
		
		
		
		static void
		_handle_members (player *pl, world *w, command_reader& reader)
		{
		  world_security& sec = w->security ();
		  
			if (!reader.has_next ())
				{
					std::ostringstream ss;
			    
			    ss << "§6Displaying members for " << w->get_colored_name () << "§e:";
			    pl->message (ss.str ());
			    ss.str (std::string ());
			    
			    const auto& member_pids = sec.get_members ();
			    if (member_pids.empty ())
			    	pl->message ("§8    none");
			    else
			    	_print_names_from_pids (pl, member_pids);
			    return;
				}
			
			const std::string arg1 = reader.next ().as_str ();
			if (sutils::iequals (arg1, "add"))
			  {
			  	if (!pl->has ("command.world.world.change-members")
			  		&& !(sec.is_owner (pl->pid ()) && pl->has ("command.world.world.owner.change-members")))
			  		{
			  			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
			  		}
			  	
			    if (!reader.has_next ())
            {
              pl->message ("§c * §7Usage§f: §e/world members add §cplayer");
              return;
            }
           
           const std::string arg2 = reader.next ().as_str ();
           
           // fetch player info
           soci::session sql {pl->get_server ().sql_pool ()};
           sqlops::player_info pinf;
           if (!sqlops::player_data (sql, arg2.c_str (), pl->get_server (), pinf))
            {
              pl->message ("§c * §7Unknown player§f: §c" + arg2);
              return;
            }
            
            if (sec.is_member (pinf.id))
              {
                std::ostringstream ss;
                ss << "§c * §" << pinf.rnk.main ()->color << pinf.name << " §7is already a member of this world§c.";
                pl->message (ss.str ());
                return;
              }
              
            sec.add_member (pinf.id);
            std::ostringstream ss;
            ss << "§" << pinf.rnk.main ()->color << pinf.name << " §ehas been added to the world§f'§es member list§f.";
            pl->message (ss.str ());
			  }
			 else if (sutils::iequals (arg1, "remove") || sutils::iequals (arg1, "del"))
			  {
			  	if (!pl->has ("command.world.world.change-members")
			  		&& !(sec.is_owner (pl->pid ()) && pl->has ("command.world.world.owner.change-members")))
			  		{
			  			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
			  		}
			  	
			    if (!reader.has_next ())
            {
              pl->message ("§c * §7Usage§f: §e/world members remove §cplayer");
              return;
            }
           
           const std::string arg2 = reader.next ().as_str ();
           
           // fetch player info
           soci::session sql {pl->get_server ().sql_pool ()};
           sqlops::player_info pinf;
           if (!sqlops::player_data (sql, arg2.c_str (), pl->get_server (), pinf))
            {
              pl->message ("§c * §7Unknown player§f: §c" + arg2);
              return;
            }
            
            if (!sec.is_member (pinf.id))
              {
                std::ostringstream ss;
                ss << "§c * §" << pinf.rnk.main ()->color << pinf.name << " §7is not a member of this world§c.";
                pl->message (ss.str ());
                return;
              }
              
            sec.remove_member (pinf.id);
            std::ostringstream ss;
            ss << "§" << pinf.rnk.main ()->color << pinf.name << " §ehas been removed from the world§f'§es member list§f.";
            pl->message (ss.str ());
			  }
			 else
			  {
			    pl->message ("§c * §7Unknown sub-command§f: §cmembers." + arg1);
					return;
			  }
		}
		
		
		
		static void
		_handle_build_perms (player *pl, world *w, command_reader& reader)
		{
		  if (!reader.has_next ())
		    {
		    	if (!pl->has ("command.world.world.get-perms"))
		    		{
		    			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
		    		}
		    	
		      std::ostringstream ss;
		      ss << "§6Displaying " << w->get_colored_name () << "§e'§6s build permissions§e:";
		      pl->message (ss.str ());
		      ss.str (std::string ());
		      
		      const std::string& perms = w->security ().get_build_perms ();
		      if (perms.empty ())
		        ss << "§8    not set";
		      else
		        ss << "§7    " << perms;
		      pl->message (ss.str ());
		      return;
		    }
		  
		  const std::string arg1 = reader.next ().as_str ();
		  if (sutils::iequals (arg1, "set"))
		    {
		    	if (!pl->has ("command.world.world.set-perms"))
		    		{
		    			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
		    		}
		    	
		      if (!reader.has_next ())
          	{
          		pl->message ("§c * §7Usage§f: §e/world build-perms set §cnew-build-perms");
          		return;
          	}
          
          std::string val = reader.rest ();
          w->security ().set_build_perms (val);
          
          pl->message (w->get_colored_name () + "§f'§es build-perms has been set to§f:");
          pl->message_spaced ("§f  > §7" + val);
		    }
		  else if (sutils::iequals (arg1, "clear"))
		  	{
		  		if (!pl->has ("command.world.world.set-perms"))
		    		{
		    			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
		    		}
		    	
		  		w->security ().set_build_perms ("");
		  		pl->message (w->get_colored_name () + "§f'§es build-perms has been cleared");
		  	}
		  else
		    {
		      pl->message ("§c * §7Invalid sub-command§f: §cbuild-perms." + arg1);
		      return;
		    }
		}
		
		
		
		static void
		_handle_join_perms (player *pl, world *w, command_reader& reader)
		{
		  if (!reader.has_next ())
		    {
		    	if (!pl->has ("command.world.world.get-perms"))
		    		{
		    			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
		    		}
		    	
		      std::ostringstream ss;
		      ss << "§6Displaying " << w->get_colored_name () << "§e'§6s join permissions§e:";
		      pl->message (ss.str ());
		      ss.str (std::string ());
		      
		      const std::string& perms = w->security ().get_join_perms ();
		      if (perms.empty ())
		        ss << "§8    not set";
		      else
		        ss << "§7    " << perms;
		      pl->message (ss.str ());
		      return;
		    }
		  
		  const std::string arg1 = reader.next ().as_str ();
		  if (sutils::iequals (arg1, "set"))
		    {
		    	if (!pl->has ("command.world.world.set-perms"))
		    		{
		    			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
		    		}
		    	
		      if (!reader.has_next ())
          	{
          		pl->message ("§c * §7Usage§f: §e/world join-perms set §cnew-join-perms");
          		return;
          	}
          
          std::string val = reader.rest ();
          w->security ().set_join_perms (val);
          
          pl->message (w->get_colored_name () + "§f'§es join-perms has been set to§f:");
          pl->message_spaced ("§f  > §7" + val);
		    }
		  else if (sutils::iequals (arg1, "clear"))
		  	{
		  		if (!pl->has ("command.world.world.set-perms"))
		    		{
		    			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
		    		}
		    	
		  		w->security ().set_join_perms ("");
		  		pl->message (w->get_colored_name () + "§f'§es join-perms has been cleared");
		  	}
		  else
		    {
		      pl->message ("§c * §7Invalid sub-command§f: §cjoin-perms." + arg1);
		      return;
		    }
		}
		
		
		
		static void
		_handle_def_gm (player *pl, world *w, command_reader& reader)
		{
			if (!reader.has_next ())
		    {
		    	if (!pl->has ("command.world.world.get-perms"))
		    		{
		    			pl->message (messages::not_allowed ());
		    			return;
		    		}
		    	
		      pl->message ("§6Displaying " + w->get_colored_name () + "§e'§6s default gamemode§e:");
		      pl->message ("§7    " + std::string ((w->def_gm == GT_CREATIVE) ? "Creative" : "Survival"));
		      return;
		    }
		  
		  const std::string arg1 = reader.next ().as_str ();
		  if (sutils::iequals (arg1, "set"))
		    {
		    	if (!pl->has ("command.world.world.def-gm"))
		    		{
		    			pl->message ("§c * §7You are not allowed to do that§c.");
		    			return;
		    		}
		    	
		      if (!reader.has_next ())
          	{
          		pl->message ("§c * §7Usage§f: §e/world def-gm set §cnew-def-gm");
          		return;
          	}
          
          std::string val = reader.rest ();
          gamemode_type new_gm = GT_SURVIVAL;
          if (sutils::iequals (val, "survival"))
          	;
          else if (sutils::iequals (val, "creative"))
          	new_gm = GT_CREATIVE;
          else
          	{
          		pl->message ("§c * §7Invalid gamemode§f: §c" + val);
          		return;
          	}
          
          pl->get_world ()->def_gm = (int)new_gm;
          pl->message (w->get_colored_name () + "§f'§es default gamemode has been set to§f:");
          pl->message_spaced ("§f  > §7" + std::string ((new_gm == GT_SURVIVAL) ? "Survival" : "Creative"));
		    }
		  else
		    {
		      pl->message ("§c * §7Invalid sub-command§f: §cdef-gm." + arg1);
		      return;
		    }
		}
		
		
		
		static void
		_handle_def_inv (player *pl, world *w, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					if (!pl->has ("command.world.world.get-perms"))
		    		{
		    			pl->message (messages::not_allowed ());
		    			return;
		    		}
		    	
		    	pl->message ("§6Displaying " + w->get_colored_name () + "§e'§6s default inventory§e:");
		    	if (!w->use_def_inv)
		    		pl->message ("§7    Default inventory disabled");
		    	else
		    		pl->message ("§7    " + w->def_inv);
		    	return;
				}
			
			std::string arg = reader.next ();
			if (sutils::iequals (arg, "set"))
				{
					if (!pl->has ("command.world.world.def-inv"))
						{
							pl->message (messages::not_allowed ());
							return;
						}
					
					if (!reader.has_next ())
						{
							pl->message ("§c * §7Usage§f: §e/world def-inv set §cdef-inv...");
							return;
						}
					
					w->def_inv = reader.rest ();
					pl->message (w->get_colored_name () + "§f's §edefault inventory has been set to§f:");
					pl->message ("§7 | " + w->def_inv);
				}
			else if (sutils::iequals (arg, "clear"))
				{
					if (!pl->has ("command.world.world.def-inv"))
						{
							pl->message (messages::not_allowed ());
							return;
						}
					
					w->def_inv = "";
					pl->message (w->get_colored_name () + "§f's §edefault inventory has been cleared§f.");
				}
			else if (sutils::iequals (arg, "disable"))
				{
					if (!pl->has ("command.world.world.def-inv"))
						{
							pl->message (messages::not_allowed ());
							return;
						}
					
					if (!w->use_def_inv)
						{
							pl->message ("§c * §7Default inventories are already disabled in this world§c.");
							return;
						}
					
					w->def_inv = "";
					w->use_def_inv = false;
					pl->message ("§eDefault inventories in " + w->get_colored_name () + " §ehave been disabled§f.");
				}
			else if (sutils::iequals (arg, "enable"))
				{
					if (!pl->has ("command.world.world.def-inv"))
						{
							pl->message (messages::not_allowed ());
							return;
						}
					
					if (w->use_def_inv)
						{
							pl->message ("§c * §7Default inventories are already enabled in this world§c.");
							return;
						}
					
					w->use_def_inv = true;
					pl->message ("§eDefault inventories in " + w->get_colored_name () + " §ehave been enabled§f.");
				}
			else
				{
					pl->message ("§c * §7Invalid sub-command§f: §cdef-inv." + arg);
				}
		}
		
		
		
		static void
		_handle_resize (player *pl, world *w, command_reader& reader)
		{
			if (!pl->has ("command.world.world.resize"))
    		{
    			pl->message (messages::not_allowed ());
    			return;
    		}
    	
			if (reader.arg_count () != 3)
				{
					pl->message ("§c * §7Usage§f: §e/world resize §cwidth depth");
					return;
				}
			
			int width = 0, depth = 0;
			
			auto a_width = reader.next ();
			auto a_depth = reader.next ();
			
			if (!a_width.is_int ())
				{
					if (sutils::iequals (a_width.as_str (), "inf"))
						width = 0;
					else
						{
							pl->message ("§c * §7Both width and depth must be non-zero positive integers§c.");
							return;
						}
				}
			else
				{
					width = a_width.as_int ();
					if (width <= 0)
						{
							pl->message ("§c * §7Both width and depth must be non-zero positive integers§c.");
							return;
						}
				}
			
			if (!a_depth.is_int ())
				{
					if (sutils::iequals (a_depth.as_str (), "inf"))
						depth = 0;
					else
						{
							pl->message ("§c * §7Both width and depth must be non-zero positive integers§c.");
							return;
						}
				}
			else
				{
					depth = a_depth.as_int ();
					if (depth <= 0)
						{
							pl->message ("§c * §7Both width and depth must be non-zero positive integers§c.");
							return;
						}
				}
			
			if (width == w->get_width () && depth == w->get_depth ())
				{
					pl->message ("§c * §7The world is already in that size§c.");
					return;
				}
			
			w->set_size (width, depth);
			w->get_players ().all (
				[] (player *pl)
					{
						pl->rejoin_world (false);
						pl->message ("§bWorld reloaded");
					});
		}
		
		
		
		static void
		_handle_regenerate (player *pl, world *w, command_reader& reader)
		{
			if (!pl->has ("command.world.world.regenerate"))
    		{
    			pl->message (messages::not_allowed ());
    			return;
    		}
    	
			if (reader.arg_count () < 2 || reader.arg_count () > 3)
				{
					pl->message ("§c * §7Usage§f: §e/world regenerate §cgenerator §8[§cseed§8]");
					return;
				}
			
			const std::string gen_name = reader.next ().as_str ();
			long long gen_seed = w->get_generator ()->seed ();
			if (reader.has_next ())
				{
					auto arg = reader.next ();
					if (!arg.is_int ())
						{
							pl->message ("§c * §7The seed must be an integer§c.");
							return;
						}
					
					gen_seed = arg.as_int ();
				}
			
			world_generator *gen = world_generator::create (gen_name.c_str (), gen_seed);
			if (!gen)
				{
					pl->message ("§c * §7No such world generator§f: §c" + gen_name);
					return;
				}

			w->set_generator (gen);
			w->clear_chunks (false, true);
			w->get_players ().all (
				[] (player *pl)
					{
						pl->rejoin_world ();
						pl->message ("§bWorld reloaded");
					});
			
			/*
			std::ostringstream ss;
			ss << w->get_colored_name () << " §eregenerated §f[§eseed§f: §7" << gen_seed << "§f]";
			pl->message (ss.str ());
			*/
		}
		
		
		
		static bool
		_copy_file (const char *dest_path, const char *src_path)
		{
			FILE *dest = std::fopen (dest_path, "wb");
			if (!dest)
				return false;
			
			FILE *src = std::fopen (src_path, "rb");
			if (!src)
				{
					std::fclose (dest);
					return false;
				}
			
#define BACKUP_BUFFER_SIZE		524288
			unsigned char *buf = new unsigned char [BACKUP_BUFFER_SIZE];
			
			std::fseek (src, 0, SEEK_END);
			long src_size = (long)std::ftell (src);
			std::fseek (src, 0, SEEK_SET);
			
			long copied = 0, need;
			while (copied < src_size)
				{
					need = BACKUP_BUFFER_SIZE;
					if ((src_size - copied) < need)
						need = src_size - copied;
					
					long read = std::fread (buf, 1, need, src);
					if (read == 0)
						{
							delete[] buf;
							std::fclose (dest);
							std::fclose (src);
							return false;
						}
					
					std::fwrite (buf, 1, need, dest);
					copied += need;
				}
			
			delete[] buf;
			std::fclose (dest);
			std::fclose (src);
			return true;
		}
		
		static int
		_determine_backup_number (const std::string& path)
		{
			int num = 1;
			std::ostringstream ss;
			for (;;)
				{
					ss << path << "." << num;
					
					FILE *f = std::fopen (ss.str ().c_str (), "rb");
					if (!f)
						return num;
					
					std::fclose (f);
					ss.str (std::string ());
					++ num;
				}
		}
		
		static void
		_handle_backup (player *pl, world *w, command_reader& reader)
		{
			if (!pl->has ("command.world.world.backup"))
    		{
    			pl->message (messages::not_allowed ());
    			return;
    		}
    	
    	mkdir ("data/backups", 0744);
    	mkdir (("data/backups/" + std::string (w->get_name ())).c_str (), 0744);
    	
    	std::string dest = w->get_path ();
    	dest.erase (0, 12); // remove "data/worlds/" part
    	dest.insert (0, "data/backups/" + std::string (w->get_name ()) + "/");
    	
    	int backup_num = _determine_backup_number (dest.c_str ());
    	std::ostringstream ss;
    	ss << dest << "." << backup_num;
    	
    	w->save_all ();
    	if (!_copy_file (ss.str ().c_str (), w->get_path ()))
    		pl->message ("§4 * §cFailed to save backup§4.");
    	else
    		{
    			pl->message ("§eBackup saved");
    			
    			ss.str (std::string ());
    			ss << "§7 | Use §e/world restore §b" << backup_num << " §7to restore this backup.";
    			pl->message (ss.str ());
    		}
		}
		
		
		
		static void
		_handle_restore (player *pl, world *w, command_reader& reader)
		{
			if (!pl->has ("command.world.world.restore"))
    		{
    			pl->message (messages::not_allowed ());
    			return;
    		}
    	
    	if (!reader.has_next ())
    		{
    			pl->message ("§c * §7Usage§f: §e/world restore §cbackup-number");
    			return;
    		}
    	
    	int backup_num;
    	auto arg = reader.next ();
    	if (!arg.is_int () || ((backup_num = arg.as_int ()) <= 0))
    		{
    			pl->message ("§c * §7The backup number should a positive non-zero integer§c.");
    			return;
    		}
    	
    	std::string src = w->get_path ();
    	src.erase (0, 12); // remove "data/worlds/" part
    	src.insert (0, "data/backups/" + std::string (w->get_name ()) + "/");
    	std::ostringstream ss;
    	ss << src << "." << backup_num;
    	
    	FILE *f = std::fopen (ss.str ().c_str (), "rb");
    	if (!f)
    		{
    			pl->message ("§c * §7Backup does not exist§c.");
    			return;
    		}
    	std::fclose (f);
    	
    	_copy_file (w->get_path (), ss.str ().c_str ());
    	w->reload_world (w->get_name ());
    	w->get_players ().all (
				[] (player *pl)
					{
						std::cout << "W a" << std::endl;
						pl->rejoin_world ();
						std::cout << "W b" << std::endl;
						pl->message ("§bWorld reloaded");
					});
			
			ss.str (std::string ());
			ss << "§eRestored backup #§b" << backup_num;
			pl->message (ss.str ());
		}
		
		
		
		static void
		_handle_save (player *pl, world *w, command_reader& reader)
		{
			if (!pl->has ("command.world.world.save"))
    		{
    			pl->message (messages::not_allowed ());
    			return;
    		}
    	
    	pl->message ("§eSaving world§f...");
    	w->save_all ();
    	pl->message ("§7 | World " + w->get_colored_name () + " §7has been saved.");
    }
    
    
    
    static bool
    _parse_time (const std::string& str, unsigned long long& out)
    {
    	if (str.empty ())
    		return false;
    	
    	bool all_digits = true;
    	for (char c : str)
    		if (!std::isdigit (c))
    			{
    				all_digits = false;
    				break;
    			}
    	
    	if (all_digits)
    		{
    			std::istringstream ss {str};
    			ss >> out;
    			return true;
    		}
    	
    	// parse hours
    	int hours = 0;
    	int i = 0;
    	char c;
    	for (; i < str.size (); ++i)
    		{
    			c = str[i];
    			if (std::isdigit (c))
  					hours = (hours * 10) + (c - '0');
    			else
    				{
    					if (c == ':')
    						{
    							++ i;
    							break;
    						}
    					return false;
    				}
    		}
    	
    	// and minutes
    	int minutes = 0;
    	for (; i < str.size (); ++i)
    		{
    			c = str[i];
    			if (std::isdigit (c))
  					minutes = (minutes * 10) + (c - '0');
  				else
  					return false;
    		}
    	
    	if (hours >= 24 || minutes >= 60)
    		return false;
    	
    	out = (hours * 1000) + (int)(minutes * 16.666666667) + 17000;
    	return true;
    }
    
    static void
    _handle_time (player *pl, world *w, command_reader& reader)
    {
    	std::ostringstream ss;
    	
    	if (!reader.has_next ())
    		{
    			pl->message ("§6Displaying world time for " + w->get_colored_name () + "§e:");
    			pl->message ("§7    " + _get_time_str (w->get_time ()) + (w->is_time_frozen () ? " [Frozen]" : ""));
    			return;
    		}
    	
    	std::string arg = reader.next ();
    	if (sutils::iequals (arg, "set"))
    		{
    			if (!pl->has ("command.world.world.time"))
    				{
    					pl->message (messages::not_allowed ());
    					return;
    				}
    			
    			if (!reader.has_next ())
    				{
    					pl->message ("§c * §7Usage§f: §e/world time set §ctime");
    					return;
    				}
    			
    			unsigned long long time = 0;
    			arg = reader.next ().as_str ();
    			if (!_parse_time (arg, time))
    				{
    					pl->message ("§c * §7Invalid time string§c.");
    					return;
    				}
    			
    			w->set_time (time);
					ss << "§eWorld time for " << w->get_colored_name () << " §ehas been set to§f: §9"
						<< _get_time_str (time) << " §f[§9" << (time % 24000) << " ticks§f]";
					pl->message (ss.str ());
    		}
    	else if (sutils::iequals (arg, "stop"))
    		{
    			if (!pl->has ("command.world.world.time"))
    				{
    					pl->message (messages::not_allowed ());
    					return;
    				}
    			
    			if (w->is_time_frozen ())
    				{
    					pl->message ("§c * §Time is already frozen in this world§c.");
    					return;
    				}
    			
    			w->stop_time ();
    			pl->message ("§eWorld time has been stopped in world " + w->get_colored_name ());
    		}
    	else if (sutils::iequals (arg, "resume"))
    		{
    			if (!pl->has ("command.world.world.time"))
    				{
    					pl->message (messages::not_allowed ());
    					return;
    				}
    			
    			if (!w->is_time_frozen ())
    				{
    					pl->message ("§c * §Time is not frozen in this world§c.");
    					return;
    				}
    			
    			w->resume_time ();
    			pl->message ("§eWorld time has been resumed in world " + w->get_colored_name ());
    		}
    	else
    		{
    			pl->message ("§c * §7Unknown sub-command§f: §ctime." + arg);
    		}
    }
		
		
		
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
		 *   - commands.world.world.def-gm
		 *       Needed to change the world's default gamemode.
		 *   - commands.world.world.resize
		 *       Required to resize the world.
		 *   - commands.world.world.regenerate
		 *       Required to regenerate the world.
		 *   - commands.world.world.save
		 *       Required to save the world.
		 *   - commands.world.world.time
		 *       Required to change the time of the world.
		 */
		void
		c_world::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.world.world"))
				return;
			
			reader.add_option ("world", "w", 1, 1);
			if (!reader.parse (this, pl))
				return;
			
			world *w = pl->get_world ();
			auto opt_w = reader.opt ("world");
			if (opt_w->found ())
				{
					const std::string& w_name = opt_w->arg (0).as_str ();
					w = pl->get_server ().get_worlds ().find (w_name.c_str ());
					if (!w)
						{
							pl->message ("§c * §7Unknown world§f: §c" + w_name);
							return;
						}
				}
			
			if (!reader.has_next ())
				_handle_no_args (pl, w);
			else
				{
					std::string arg1 = reader.next ().as_str ();
					
					static const std::unordered_map<cistring,
						void (*)(player *, world *, command_reader &)> _map {
						{ "owners", _handle_owners },
						{ "members", _handle_members },
						{ "build-perms", _handle_build_perms },
						{ "join-perms", _handle_join_perms },
						{ "def-gm", _handle_def_gm },
						{ "def-inv", _handle_def_inv },
						{ "resize", _handle_resize },
						{ "regenerate", _handle_regenerate },
						{ "backup", _handle_backup },
						{ "restore", _handle_restore },
						{ "save", _handle_save },
						{ "time", _handle_time },
					};
					
					auto itr = _map.find (arg1.c_str ());
					if (itr == _map.end ())
						{
							pl->message ("§c * §7Unknown sub-command§f: §c" + arg1);
							return;
						}
					
					itr->second (pl, w, reader);
				}
		}
	}
}
