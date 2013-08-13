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
#include "player.hpp"
#include "world.hpp"
#include "stringutils.hpp"
#include "server.hpp"
#include "portal.hpp"
#include "editstage.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		namespace {
			struct portal_data {
				world *src_world;
				sparse_edit_stage *es;
				portal *ptl;
			};
		}
		
		
		static void
		_finish_portal (player *pl)
		{
			portal_data *data = static_cast<portal_data *> (pl->get_data ("portal"));
			if (!data)
				{
					pl->message ("§c * §7You are not placing any portals§c.");
					return;
				}
			
			world *dest_world = pl->get_world ();
			portal *ptl = data->ptl;
			ptl->dest_world.assign (dest_world->get_name ());
			ptl->dest_pos = pl->pos;
			data->src_world->add_portal (ptl);
			
			data->es->commit ();
			delete data->es;
			
			std::ostringstream ss;
			ss << "§7 | Portal complete [" << data->src_world->get_colored_name () << " §7-> " << dest_world->get_colored_name () << "§7]";
			pl->message (ss.str ());
			
			pl->delete_data ("portal");
		}
		
		static void
		_cancel_portal (player *pl)
		{
			portal_data *data = static_cast<portal_data *> (pl->get_data ("portal"));
			if (!data)
				{
					pl->message ("§c * §7Nothing to cancel§c.");
					return;
				}
			
			delete data->es;
			delete data->ptl;
			pl->delete_data ("portal");
			pl->message ("§7 | §cPortal cancelled");
		}
		
		
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
		void
		c_portal::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			if (reader.no_args ())
				{
					pl->message ("§c * §7Usage§f: §e/portal §ctarget-block §8[§creplace-block§8]");
					return;
				}
			
			std::string& target_block_str = reader.next ().as_str ();
			if (sutils::iequals (target_block_str, "exit"))
				{
					_finish_portal (pl);
					return;
				}
			else if (sutils::iequals (target_block_str, "cancel"))
				{
					_cancel_portal (pl);
					return;
				}
			else
				{
					portal_data *data = static_cast<portal_data *> (pl->get_data ("portal"));
					if (data)
						{
							pl->message ("§c * §7Type §c/portal cancel §7to cancel your previous portal§c.");
							return;
						}
				}
			
			if (pl->selections.empty ())
				{ pl->message ("§c * §7You§f'§7re not selecting anything§f."); return; }
			world_selection *sel = pl->curr_sel;
			world *w = pl->get_world ();
			
			blocki target_block;
			if (!(target_block = sutils::to_block (target_block_str)).valid ())
				{
					pl->message ("§c * §7Invalid block§f: §c" + target_block_str);
					return;
				}
			
			blocki replace_block = target_block;
			if (reader.has_next ())
				{
					std::string& replace_block_str = reader.next ().as_str ();
					if (!(replace_block = sutils::to_block (replace_block_str)).valid ())
						{
							pl->message ("§c * §7Invalid block§f: §c" + replace_block_str);
							return;
						}
				}
			
			int blocks = 0;
			sparse_edit_stage *es = new sparse_edit_stage (pl->get_world ());
			portal *ptl = new portal ();
			
			block_pos smin = sel->min (), smax = sel->max ();
			if (smin.y < 0) smin.y = 0;
			if (smin.y > 255) smin.y = 255;
			if (smax.y < 0) smax.y = 0;
			if (smax.y > 255) smax.y = 255;
			for (int x = smin.x; x <= smax.x; ++x)
				for (int y = smin.y; y <= smax.y; ++y)
					for (int z = smin.z; z <= smax.z; ++z)
						{
							if (!w->in_bounds (x, y, z)) continue;
							if (sel->contains (x, y, z))
								{
									block_data bd = w->get_block (x, y, z);
									if (bd.id == target_block.id && bd.meta == target_block.meta)
										{
											es->set (x, y, z, replace_block.id, replace_block.meta, BE_PORTAL);
											ptl->affected.emplace_back (x, y, z);
											++ blocks;
										}
								}
						}
			
			ptl->calc_bounds ();
			
			portal_data *data = new portal_data {w, es, ptl};
			pl->create_data ("portal", data,
				[] (void *ptr) { delete static_cast<portal_data *> (ptr); });
			
			{
				std::ostringstream ss;
				ss << "§7 | §b" << blocks << " §eentrance blocks have been marked.";
				pl->message (ss.str ());
				pl->message ("§7 | §eType §c/portal exit §eto complete the portal.");
				pl->message ("§7 | §eIf you wish to cancel the portal, type §c/portal cancel");
			}
		}
	}
}

