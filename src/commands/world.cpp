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

#include "commands/worldc.hpp"
#include "server.hpp"
#include "world.hpp"
#include "stringutils.hpp"
#include "sqlops.hpp"
#include "cistring.hpp"
#include "messages.hpp"
#include <unordered_map>
#include <sstream>


namespace hCraft {
	namespace commands {
		
		static void
		_print_names_from_pids (player *pl, const std::vector<int>& pids)
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
			
			const std::string& arg1 = reader.next ().as_str ();
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
           
           const std::string& arg2 = reader.next ().as_str ();
           
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
           
           const std::string& arg2 = reader.next ().as_str ();
           
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
					return;
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
			
			const std::string& arg1 = reader.next ().as_str ();
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
           
           const std::string& arg2 = reader.next ().as_str ();
           
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
           
           const std::string& arg2 = reader.next ().as_str ();
           
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
		  
		  const std::string& arg1 = reader.next ().as_str ();
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
		  
		  const std::string& arg1 = reader.next ().as_str ();
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
						pl->rejoin_world ();
					});
			
			std::ostringstream ss;
			ss << w->get_colored_name () << "§f'§es dimensions set to §7";
			if (w->get_width () <= 0)
				ss << "inf";
			else
				ss << w->get_width ();
			ss << " §fx §7";
			if (w->get_depth () <= 0)
				ss << "inf";
			else
				ss << w->get_depth ();
			pl->message (ss.str ());
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
			
			const std::string& gen_name = reader.next ().as_str ();
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
					});
			
			std::ostringstream ss;
			ss << w->get_colored_name () << " §eregenerated §f[§eseed§f: §7" << gen_seed << "§f]";
			pl->message (ss.str ());
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
		 *   - commands.world.world.resize
		 *       Required to resize the world.
		 *   - commands.world.world.regenerate
		 *       Required to regenerate the world.
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
					const std::string& arg1 = reader.next ().as_str ();
					
					static const std::unordered_map<cistring,
						void (*)(player *, world *, command_reader &)> _map {
						{ "owners", _handle_owners },
						{ "members", _handle_members },
						{ "build-perms", _handle_build_perms },
						{ "join-perms", _handle_join_perms },
						{ "resize", _handle_resize },
						{ "regenerate", _handle_regenerate },
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
