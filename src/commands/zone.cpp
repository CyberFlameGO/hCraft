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

#include "commands/zone.hpp"
#include "player/player.hpp"
#include "util/stringutils.hpp"
#include "system/server.hpp"
#include "world/world.hpp"
#include "util/cistring.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		static void
		_handle_create (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f: §e/zone create §czone-name");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			// TODO: Group all active selections into one compound selection,
			//       and turn that into a zone.
			
			if (!pl->curr_sel)
				{
					pl->message ("§c * §7You must have an active selection§c.");
					return;
				}
			
			world *w = pl->get_world ();
			if (w->get_zones ().find (zone_name))
				{
					pl->message ("§c * §7A zone with that name already exists§c.");
					return;
				}
			
			zone *zn = new zone (zone_name, pl->curr_sel->copy ());
			w->get_zones ().add (zn);
			pl->message ("§eCreated new zone§f: §3z@§b" + zone_name);
		}
		
		
		
		static void
		_handle_build_perms (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f:");
					pl->message ("§c * (1) §e/zone build-perms §czone-name");
					pl->message ("§c * (2) §e/zone build-perms §czone-name §eset §cperms");
					pl->message ("§c * (3) §e/zone build-perms §czone-name §eclear");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			zone *zn = pl->get_world ()->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §cz@" + zone_name);
					return;
				}
			zone_name = zn->get_name ();
			
			if (!reader.has_next ())
				{
					pl->message ("§9Displaying build perms for zone §3z@§b" + zone_name);
					if (zn->get_security ().get_build_perms ().empty ())
						pl->message ("§8    not set");
					else
						pl->message ("§7    " + zn->get_security ().get_build_perms ());
					return;
				}
			else
				{
					std::string arg = reader.next ();
					if (sutils::iequals (arg, "set"))
						{
							if (!reader.has_next ())
								{
									pl->message ("§c * §7Usage§f: §e/zone build-perms §czone-name §eset §cperms");
									return;
								}
							
							std::string perms = reader.rest ();
							zn->get_security ().set_build_perms (perms);
							pl->message ("§eZone §3z@§b" + zone_name + "§e's build perms have been set to§f:");
							pl->message ("§f  > §7" + perms);
						}
					else if (sutils::iequals (arg, "clear"))
						{
							zn->get_security ().set_build_perms ("");
							pl->message ("§eZone §3z@§b" + zone_name + "§e's build perms have been cleared");
						}
					else
						{
							pl->message ("§c * Invalid sub-command§f: §cbuild-perms." + arg);
						}
				}
		}
		
		
		
		static void
		_handle_enter_perms (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f:");
					pl->message ("§c * (1) §e/zone enter-perms §czone-name");
					pl->message ("§c * (2) §e/zone enter-perms §czone-name §eset §cperms");
					pl->message ("§c * (3) §e/zone enter-perms §czone-name §eclear");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			zone *zn = pl->get_world ()->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §cz@" + zone_name);
					return;
				}
			zone_name = zn->get_name ();
			
			if (!reader.has_next ())
				{
					pl->message ("§9Displaying enter perms for zone §3z@§b" + zone_name);
					if (zn->get_security ().get_enter_perms ().empty ())
						pl->message ("§8    not set");
					else
						pl->message ("§7    " + zn->get_security ().get_enter_perms ());
					return;
				}
			else
				{
					std::string arg = reader.next ();
					if (sutils::iequals (arg, "set"))
						{
							if (!reader.has_next ())
								{
									pl->message ("§c * §7Usage§f: §e/zone enter-perms §czone-name §eset §cperms");
									return;
								}
							
							std::string perms = reader.rest ();
							zn->get_security ().set_enter_perms (perms);
							pl->message ("§eZone §3z@§b" + zone_name + "§e's enter perms have been set to§f:");
							pl->message ("§f  > §7" + perms);
						}
					else if (sutils::iequals (arg, "clear"))
						{
							zn->get_security ().set_enter_perms ("");
							pl->message ("§eZone §3z@§b" + zone_name + "§e's enter perms have been cleared");
						}
					else
						{
							pl->message ("§c * Invalid sub-command§f: §center-perms." + arg);
						}
				}
		}
		
		
		
		static void
		_handle_leave_perms (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f:");
					pl->message ("§c * (1) §e/zone leave-perms §czone-name");
					pl->message ("§c * (2) §e/zone leave-perms §czone-name §eset §cperms");
					pl->message ("§c * (3) §e/zone leave-perms §czone-name §eclear");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			zone *zn = pl->get_world ()->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §cz@" + zone_name);
					return;
				}
			zone_name = zn->get_name ();
			
			if (!reader.has_next ())
				{
					pl->message ("§9Displaying leave perms for zone §3z@§b" + zone_name);
					if (zn->get_security ().get_leave_perms ().empty ())
						pl->message ("§8    not set");
					else
						pl->message ("§7    " + zn->get_security ().get_leave_perms ());
					return;
				}
			else
				{
					std::string arg = reader.next ();
					if (sutils::iequals (arg, "set"))
						{
							if (!reader.has_next ())
								{
									pl->message ("§c * §7Usage§f: §e/zone leave-perms §czone-name §eset §cperms");
									return;
								}
							
							std::string perms = reader.rest ();
							zn->get_security ().set_leave_perms (perms);
							pl->message ("§eZone §3z@§b" + zone_name + "§e's leave perms have been set to§f:");
							pl->message ("§f  > §7" + perms);
						}
					else if (sutils::iequals (arg, "clear"))
						{
							zn->get_security ().set_leave_perms ("");
							pl->message ("§eZone §3z@§b" + zone_name + "§e's leave perms have been cleared");
						}
					else
						{
							pl->message ("§c * Invalid sub-command§f: §cleave-perms." + arg);
						}
				}
		}
		
		
		
		static void
		_handle_enter_msg (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f:");
					pl->message ("§c * (1) §e/zone enter-msg §czone-name");
					pl->message ("§c * (2) §e/zone enter-msg §czone-name §eset §cmsg");
					pl->message ("§c * (3) §e/zone enter-msg §czone-name §eclear");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			zone *zn = pl->get_world ()->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §cz@" + zone_name);
					return;
				}
			zone_name = zn->get_name ();
			
			if (!reader.has_next ())
				{
					pl->message ("§9Displaying the enter message for zone §3z@§b" + zone_name);
					if (zn->get_enter_msg ().empty ())
						pl->message ("§8    not set");
					else
						pl->message ("§7    " + zn->get_enter_msg ());
					return;
				}
			else
				{
					std::string arg = reader.next ();
					if (sutils::iequals (arg, "set"))
						{
							if (!reader.has_next ())
								{
									pl->message ("§c * §7Usage§f: §e/zone enter-msg §czone-name §eset §cmsg");
									return;
								}
							
							std::string msg = "§b" + reader.rest ();
							zn->set_enter_msg (msg);
							pl->message ("§eZone §3z@§b" + zone_name + "§e's enter message has been set to§f:");
							pl->message ("§f  > §7" + msg);
						}
					else if (sutils::iequals (arg, "clear"))
						{
							zn->set_enter_msg ("");
							pl->message ("§eZone §3z@§b" + zone_name + "§e's enter message has been cleared");
						}
					else
						{
							pl->message ("§c * Invalid sub-command§f: §center-msg." + arg);
						}
				}
		}
		
		
		
		static void
		_handle_leave_msg (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f:");
					pl->message ("§c * (1) §e/zone leave-msg §czone-name");
					pl->message ("§c * (2) §e/zone leave-msg §czone-name §eset §cmsg");
					pl->message ("§c * (3) §e/zone leave-msg §czone-name §eclear");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			zone *zn = pl->get_world ()->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §cz@" + zone_name);
					return;
				}
			zone_name = zn->get_name ();
			
			if (!reader.has_next ())
				{
					pl->message ("§9Displaying the leave message for zone §3z@§b" + zone_name);
					if (zn->get_leave_msg ().empty ())
						pl->message ("§8    not set");
					else
						pl->message ("§7    " + zn->get_leave_msg ());
					return;
				}
			else
				{
					std::string arg = reader.next ();
					if (sutils::iequals (arg, "set"))
						{
							if (!reader.has_next ())
								{
									pl->message ("§c * §7Usage§f: §e/zone leave-msg §czone-name §eset §cmsg");
									return;
								}
							
							std::string msg = "§b" + reader.rest ();
							zn->set_leave_msg (msg);
							pl->message ("§eZone §3z@§b" + zone_name + "§e's leave message has been set to§f:");
							pl->message ("§f  > §7" + msg);
						}
					else if (sutils::iequals (arg, "clear"))
						{
							zn->set_leave_msg ("");
							pl->message ("§eZone §3z@§b" + zone_name + "§e's leave message has been cleared");
						}
					else
						{
							pl->message ("§c * Invalid sub-command§f: §cleave-msg." + arg);
						}
				}
		}
		
		
		
		static void
		_handle_delete (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f: §e/zone delete §czone-name");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			world *w = pl->get_world ();
			zone *zn = w->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §c" + zone_name);
					return;
				}
			
			w->get_zones ().remove (zn);
			pl->message ("§eZone §3z@§b" + zone_name + " §ehas been removed§f.");
		}
		
		
		
		static void
		_handle_select (player *pl, command_reader& reader)
		{
			if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f: §e/zone select §czone-name");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			world *w = pl->get_world ();
			zone *zn = w->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §c" + zone_name);
					return;
				}
			
			std::ostringstream ss;
			ss << pl->sb_next_unused ();
			std::string sel_name = ss.str ();
			
			world_selection *sel = zn->get_selection ()->copy ();
			pl->selections[sel_name.c_str ()] = sel;
			pl->curr_sel = sel;
			sel->show (pl);
			pl->sb_commit ();
			
			pl->message ("§eCreated new selection §b@" + sel_name + " §efrom zone §3z@§b" + zn->get_name ());
		}
		
		
		
		static void
		_handle_reset (player *pl, command_reader& reader)
		{
			if (reader.arg_count () != 3)
				{
					pl->message ("§c * §7Usage§f: §e/zone reset §czone-name sel-name");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			std::string sel_name = reader.next ();
			if (sel_name.empty () || (sel_name[0] != '@'))
				{
					pl->message ("§c * §7Invalid selection name§f: §c" + sel_name);
					return;
				}
			sel_name.erase (0, 1);
			
			world *w = pl->get_world ();
			zone *zn = w->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §c" + zone_name);
					return;
				}
			
			// find selection
			auto itr = pl->selections.find (sel_name.c_str ());
			if (itr == pl->selections.end ())
				{
					pl->message ("§c * §7Unknown selection§f: §c" + sel_name);
					return;
				}
			
			world_selection *sel = itr->second;
			zone *zcopy = new zone (zn, sel->copy ());
			
			w->get_zones ().remove (zn);
			w->get_zones ().add (zcopy);
			
			pl->message ("§eSelection changed to §b@" + sel_name + " §efor zone §3z@§b" + zn->get_name ());
		}
		
		
		
		static bool
		_check_callback (player *pl, block_pos marked[], int markedc)
		{
			std::vector<zone *> zones;
			pl->get_world ()->get_zones ().find (marked[0].x, marked[0].y, marked[0].z, zones);
			
			if (zones.empty ())
				pl->message ("§7There are no zones in here.");
			else
				{
					std::ostringstream ss;
					ss << "§eThere " << ((zones.size () == 1) ? "is" : "are") << " §b" << zones.size () << " §ezone"
						<< ((zones.size () == 1) ? "" : "s") << " at §f[§7" << marked[0].x << "§f, §7" << marked[0].y
						<< "§f, " << marked[0].z << "§f]:";
					pl->message (ss.str ());
					
					ss.str (std::string ());
					ss << "§7    ";
					for (zone *zn : zones)
						ss << "§3z@§b" << zn->get_name () << " ";
					pl->message_spaced (ss.str ());
				}
			
			return true;
		}
		
		static void
		_handle_check (player *pl, command_reader& reader)
		{
			pl->get_nth_marking_callback (1) += _check_callback;
			pl->message ("§ePlease mark §bone §eblock§f.");
		}
		
		
		static void
		_handle_pvp (player *pl, command_reader& reader)
		{
		  if (!reader.has_next ())
				{
					pl->message ("§c * §7Usage§f:");
					pl->message ("§c * (1) §e/zone pvp §czone-name");
					pl->message ("§c * (2) §e/zone pvp §czone-name §8on§7/§8off");
					return;
				}
			
			std::string zone_name = reader.next ();
			if (zone_name.size () < 3 || zone_name[0] != 'z' || zone_name[1] != '@')
				{
					pl->message ("§c * §7Invalid zone name§f: §c" + zone_name);
					return;
				}
			zone_name.erase (0, 2);
			
			world *w = pl->get_world ();
			zone *zn = w->get_zones ().find (zone_name);
			if (!zn)
				{
					pl->message ("§c * §7Unknown zone§f: §c" + zone_name);
					return;
				}
		  
		  if (!reader.has_next ())
		    {
		      pl->message ("§ePvP is " + std::string (zn->pvp_enabled () ? "§aON" : "§cOFF")
		        + " §ein zone §3z@§b" + zn->get_name ());
		      return;
		    }
		  
		  std::string arg = reader.next ();
		  if (sutils::iequals (arg, "on"))
		    {
		      zn->enable_pvp ();
		      pl->message ("§ePvP has been turned §aON §ein zone §3z@§b" + zn->get_name ());
		    }
		  else if (sutils::iequals (arg, "off"))
		    {
		      zn->disable_pvp ();
		      pl->message ("§ePvP has been turned §cOFF §ein zone §3z@§b" + zn->get_name ());
		    }
		  else
		    {
		      pl->message ("§c * §7Usage§f: §e/zone pvp §czone-name §8on§7/§8off");
		    }
		}
		
		
		
		/*
		 * /zone
		 * 
		 * Zone management.
		 * 
		 * Permissions:
		 *   - command.world.zone
		 *       Needed to execute the command.
		 */
		void
		c_zone::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			if (reader.no_args ())
				{ this->show_summary (pl); return; }
			
			std::string arg = reader.next ();
			static const std::unordered_map<cistring, void (*)(player *, command_reader&)> _map {
				{ "create", _handle_create },
				{ "delete", _handle_delete },
				{ "build-perms", _handle_build_perms },
				{ "enter-perms", _handle_enter_perms },
				{ "leave-perms", _handle_leave_perms },
				{ "enter-msg", _handle_enter_msg },
				{ "leave-msg", _handle_leave_msg },
				{ "select", _handle_select },
				{ "reset", _handle_reset },
				{ "check", _handle_check },
				{ "pvp", _handle_pvp },
			};
			
			auto itr = _map.find (arg.c_str ());
			if (itr == _map.end ())
				{
					pl->message ("§c * §7Invalid sub-command§f: §c" + arg);
					return;
				}
			
			itr->second (pl, reader);
		}
	}
}

