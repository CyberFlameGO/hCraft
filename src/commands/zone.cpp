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
				{ "build-perms", _handle_build_perms },
				{ "enter-perms", _handle_enter_perms },
				{ "leave-perms", _handle_leave_perms },
				{ "enter-msg", _handle_enter_msg },
				{ "leave-msg", _handle_leave_msg },
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

