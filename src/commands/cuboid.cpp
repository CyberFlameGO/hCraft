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

#include "drawc.hpp"
#include "../server.hpp"
#include "../player.hpp"
#include "../world.hpp"
#include "../utils.hpp"


namespace hCraft {
	namespace commands {
		
		namespace {
			struct cuboid_data
			{
				int block_id;
			};
		}
		
		static bool
		on_two_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			int sx = utils::min (marked[0].x, marked[1].x);
			int sy = utils::min (marked[0].y, marked[1].y);
			int sz = utils::min (marked[0].z, marked[1].z);
			int ex = utils::max (marked[0].x, marked[1].x);
			int ey = utils::max (marked[0].y, marked[1].y);
			int ez = utils::max (marked[0].z, marked[1].z);
			
			cuboid_data *data = static_cast<cuboid_data *> (pl->get_data ("cuboid"));
			if (!data) return true; // shouldn't happen
			
			world *wr = pl->get_world ();
			for (int x = sx; x <= ex; ++x)
				for (int y = sy; y <= ey; ++y)
					for (int z = sz; z <= ez; ++z)
						{
							wr->queue_update (x, y, z, data->block_id);
						}
			
			pl->delete_data ("cuboid");
			
			return true;
		}
		
		/* 
		 * /cuboid -
		 * 
		 * Fills a rectangular cuboid area with a specified block.
		 * 
		 * Permissions:
		 *   - command.draw.cuboid
		 *       Needed to execute the command.
		 */
		void
		c_cuboid::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.cuboid"))
				return;
			
			if (!reader.parse_args (this, pl))
				return;
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_usage (pl); return; }
			
			std::string& block = reader.arg (0);
			block_info *binf = block_info::from_id_or_name (block.c_str ());
			if (!binf)
				{
					pl->message ("§c * §eInvalid block name§f/§eID§f: §c" + block);
					return;
				}
			
			cuboid_data *data = new cuboid_data;
			data->block_id = binf->id;
			
			pl->create_data ("cuboid", data,
				[] (void *ptr) { delete static_cast<cuboid_data *> (ptr); });
			pl->get_nth_marking_callback (2) += on_two_blocks_marked;
			
			pl->message ("§6Cuboid §f- §eBlock§e: §c" + std::string (binf->name) + "§f.");
			pl->message ("§6[§e*§6] §ePlease mark two blocks§e.");
		}
	}
}

