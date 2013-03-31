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
#include "player.hpp"
#include "server.hpp"
#include "stringutils.hpp"
#include "utils.hpp"
#include <sstream>
#include <cmath>


namespace hCraft {
	namespace commands {
		
		namespace {
			struct bezier_data {
				blocki bl;
			};
		}
		
		struct vector3 {
			double x, y, z;
		
		//---
			vector3 () : x (), y (), z () { }
			vector3 (double x, double y, double z)
				: x (x), y (y), z (z) 
				{ }
			vector3 (block_pos bpos)
				: x (bpos.x), y (bpos.y), z (bpos.z)
				{ }
				
		//---
			friend vector3
			operator+ (vector3 a, vector3 b)
				{ return {a.x + b.x, a.y + b.y, a.z + b.z}; }
			
			friend vector3
			operator* (double a, vector3 b)
				{ return {a * b.x, a * b.y, a * b.z}; }
		};
		
		
		static void
		draw_line (world *w, vector3 pt1, vector3 pt2, unsigned short id,
			unsigned char meta)
		{
			// construct a vector in the direction that corresponds to the
			// diagonal formed in the cuboid selected by the player.
			double vx = (pt2.x - pt1.x);
			double vy = (pt2.y - pt1.y);
			double vz = (pt2.z - pt1.z);
			
			// normalize
			double mag = std::sqrt (vx*vx + vy*vy + vz*vz);
			double ux = vx / mag;
			double uy = vy / mag;
			double uz = vz / mag;
			
			double x = pt1.x, y = pt1.y, z = pt1.z;
			while (((ux > 0.0) ? (x <= pt2.x) : (x >= pt2.x)) &&
						 ((uy > 0.0) ? (y <= pt2.y) : (y >= pt2.y)) &&
						 ((uz > 0.0) ? (z <= pt2.z) : (z >= pt2.z)))
				{
					w->queue_update (std::floor (x), std::floor (y), std::floor (z),
						id, meta);
					
					x += ux;
					y += uy;
					z += uz;
				}
		}
		
		
		static vector3
		quadratic_bezier (double t, vector3 p0, vector3 p1, vector3 p2)
		{
			return ((1.0 - t) * ((1.0 - t) * p0 + (t * p1)))
					 + (t * ((1.0 - t) * p1 + (t * p2)));
		}
		
		static vector3
		cubic_bezier (double t, vector3 p0, vector3 p1, vector3 p2, vector3 p3)
		{
			return ((1.0 - t) * quadratic_bezier (t, p0, p1, p2))
					 + (t * quadratic_bezier (t, p1, p2, p3));
		}
		
		
		static bool
		on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			bezier_data *data = static_cast<bezier_data *> (pl->get_data ("bezier"));
			if (!data) return true; // shouldn't happen
			
			vector3 prev_pt = marked[0];
			
			int pt_count = 50;
			double t_inc = 1.0 / pt_count;
			for (double t = 0.0; t <= 1.0; t += t_inc)
				{
					vector3 next_pt = cubic_bezier (t, marked[0], marked[1], marked[2], marked[3]);
					draw_line (pl->get_world (), prev_pt, next_pt, data->bl.id, data->bl.meta);
					prev_pt = next_pt;
				}
			
			pl->delete_data ("bezier");
			pl->message ("§3Bezier curve complete");
			return true;
		}
		
		/* 
		 * /bezier -
		 * 
		 * Draws a quadratic beizer curves using three control points.
		 * The curve will pass through the first and last points, but not necessarily
		 * through the middle one.
		 * 
		 * Permissions:
		 *   - command.draw.bezier
		 *       Needed to execute the command.
		 */
		void
		c_bezier::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm ("command.draw.bezier"))
					return;
		
			if (!reader.parse_args (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			std::string& str = reader.next ().as_str ();
			if (!sutils::is_block (str))
				{
					pl->message ("§c * §7Invalid block§f: §c" + str);
					return;
				}
			
			blocki bl = sutils::to_block (str);
			if (bl.id == BT_UNKNOWN)
				{
					pl->message ("§c * §7Unknown block§f: §c" + str);
					return;
				}
			
			bezier_data *data = new bezier_data {bl};
			pl->create_data ("bezier", data,
				[] (void *ptr) { delete static_cast<bezier_data *> (ptr); });
			pl->get_nth_marking_callback (4) += on_blocks_marked;
			
			pl->message ("§8Bezier curve §7(§8Block§7: §b" + str + "§7):");
			pl->message ("§8 * §7Please mark §bfour §7blocks§7.");
		}
	}
}

