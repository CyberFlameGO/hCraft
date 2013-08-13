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

#ifndef _hCraft__COMMANDS__DRAW_H_
#define _hCraft__COMMANDS__DRAW_H_

#include "command.hpp"


namespace hCraft {
	namespace commands {
		
		/* 
		 * /select -
		 * 
		 * A multipurpose command for changing the current selection(s) managed
		 * by the calling player.
		 * 
		 * Permissions:
		 *   - command.draw.select
		 *       Needed to execute the command.
		 */
		class c_select: public command
		{
		public:
			const char* get_name () { return "select"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"sel",
						"s",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "A multipurpose command for changing the current selection(s) managed by the calling player."; }
			
			const char*
			get_help ()
			{ return
				".TH SELECT 1 \"/select\" \"Revision 1\" \"PLAYER COMMANDS\" "
				".SH NAME "
					"select, sel, s - A multipurpose command for managing selections "
					"owned by the player.  "
				".SH SYNOPSIS "
					"$g/select $yNUMBER .LN "
					"$g/select new $G[$yNAME$G] $yTYPE .LN "
					"$g/select delete $yNAME .LN "
					"$g/select clear $G[visible/hidden/all] .LN "
					"$g/select show$G/$ghide $G<$yNAMES...$G>/all .LN "
					"$g/select move $G<$yDIRECTIONS UNITS$G>... .LN "
					"$g/select expand$G/$gcontract $G<$yDIRECTIONS UNITS$G>... .LN "
					"$g/select sb $yBLOCK .LN "
					"$g/select all $G[but/except] $yBLOCKS... .LN "
					"$g/select OPTION "
					".PP "
				".SH DESCRIPTION "
					"The select command governs almost everything related to selections: "
					"Creating  selections, deleting them, showing/hiding them, moving, "
					"contracting, expanding, etc... Detailed explanations for the "
					"subcommands will be given out in the same order that they appear "
					"in the SYNPOSIS section: "
					".PP "
					"$G/select $yNUMBER "
						".TP Sets the NUMBER'th point of the current selection to the player's "
						"position. .PP "
					"$G/select new $g[$yNAME$g] $yTYPE "
						".TP Creates a new selection of type TYPE, optionally giving it the name "
						"of NAME (must be prefixed with @). Valid selection types are (as of now): "
						"cuboid and sphere. If no name is given, then the lowest untaken number "
						"will be set as the selection's name (e.g.: @1, @2, @3, etc...). .PP "
						"$G/select delete $yNAME "
						".TP Deletes the selection that has the name NAME (must be prefixed with "
						"@) .PP "
					"$G/select clear $g[visible/hidden/all] "
						".TP Deletes all visible selections, or any selections that match the "
						"specified option (delete only visible selections, only hidden selections, "
						"or all selections, respectively). .PP "
					"$G/select show$g/$Ghide $g<$yNAMES...$g>/all "
						".TP Shows (or hides, depending on the subcommand) either all selections "
						"(if all is used), or the selections included in NAMES... (all names must "
						"be prefixed with @). .PP "
					"$G/select move $g<$yDIRECTIONS UNITS$g>... "
						".TP Moves all visible selections into one or more directions. DIRECTIONS "
						"can be either x, y, z, or a combination of the three. UNITS must be "
						"a number describing the number of blocks to move the selections in the "
						"directions specified. .PP "
					"$G/select expand$g/$Gcontract $g<$yDIRECTIONS UNITS$g>... "
						".TP Scales all visible selections with accordance to the specified "
						"subcommand. The format of the DIRECTIONS and UNITS is the same as in "
					"/select move. .PP "
						"$G/select sb $yBLOCK "
						".TP Changes the block from which selections are made out of. .PP "
					"$G/select all $g[but/except] $yBLOCKS... "
						".TP Filters all visible selections to include only (or everything but, "
						"if but/except is specified) the blocks included in BLOCKS... The resulting "
						"selection will be a block selection with the size of all visible "
						"selections combined. .PP "
					"If OPTION is specified, then execute accordingly: "
					".PP "
					"$G\\\\help \\h $gDisplays help "
					".PP "
					"$G\\\\summary \\s $gDisplays a short description "
					".PP "
				".SH EXAMPLES "
					"/s 1 ; /s 2 ; /s 3 .LN "
					"/s new cuboid ; /s new sphere ; /s new @boo sphere .LN "
					"/s expand xyz -8 ; /s move x 5 y 4 z -4 .LN "
					"/s all but air ; /s all grass dirt .LN "
					"/s show @1 @2 @boo ; /s hide all .LN "
					"/s clear ; /s clear all ; /s clear hidden .LN "
					"/s sb still-water "
				;}
			
			const char* get_exec_permission () { return "command.draw.select"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /fill -
		 * 
		 * Fills all visible selected regions with a block.
		 * 
		 * Permissions:
		 *   - command.draw.fill
		 *       Needed to execute the command.
		 */
		class c_fill: public command
		{
		public:
			const char* get_name () { return "fill"; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"f",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_summary ()
				{ return "Fills all visible selected regions with a block."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.fill"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /cuboid -
		 * 
		 * Fills a region marked by two points with a specified block.
		 * 
		 * Permissions:
		 *   - command.draw.cuboid
		 *       Needed to execute the command.
		 */
		class c_cuboid: public command
		{
		public:
			const char* get_name () { return "cuboid"; }
			
			const char*
			get_summary ()
				{ return "Fills a region marked by two points with a specified block."; }
			
			const char**
			get_aliases ()
			{
				static const char *aliases[] =
					{
						"c",
						nullptr,
					};
				return aliases;
			}
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.cuboid"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /line -
		 * 
		 * Draws a line between two selected points.
		 * 
		 * Permissions:
		 *   - command.draw.line
		 *       Needed to execute the command.
		 */
		class c_line: public command
		{
		public:
			const char* get_name () { return "line"; }
			
			const char*
			get_summary ()
				{ return "Draws a line between two selected points."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.l;ine"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /bezier -
		 * 
		 * Draws a beizer curvee using between user-specified control points.
		 * The curve will pass through the first and last points, but not necessarily
		 * through the rest.
		 * 
		 * Permissions:
		 *   - command.draw.bezier
		 *       Needed to execute the command.
		 */
		class c_bezier: public command
		{
		public:
			const char* get_name () { return "bezier"; }
			
			const char*
			get_summary ()
				{ return "Draws a beizer curve using between user-specified control points."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.bezier"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /aid -
		 * 
		 * Places a block at the player's feet.
		 * 
		 * Permissions:
		 *   - command.draw.aid
		 *       Needed to execute the command.
		 */
		class c_aid: public command
		{
		public:
			const char* get_name () { return "aid"; }
			
			const char*
			get_summary ()
				{ return "Places a block at the player's feet."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.aid"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /circle -
		 * 
		 * Draws a two-dimensional circle centered at a point.
		 * 
		 * Permissions:
		 *   - command.draw.circle
		 *       Needed to execute the command.
		 */
		class c_circle: public command
		{
		public:
			const char* get_name () { return "circle"; }
			
			const char*
			get_summary ()
				{ return "Draws a two-dimensional circle centered at a point."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.circle"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /ellipse -
		 * 
		 * Draws a two-dimensional ellipse centered at a point.
		 * 
		 * Permissions:
		 *   - command.draw.ellipse
		 *       Needed to execute the command.
		 */
		class c_ellipse: public command
		{
		public:
			const char* get_name () { return "ellipse"; }
			
			const char*
			get_summary ()
				{ return "Draws a two-dimensional ellipse centered at a point."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.ellipse"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /sphere -
		 * 
		 * Draws a three-dimensional sphere centered at a point.
		 * 
		 * Permissions:
		 *   - command.draw.sphere
		 *       Needed to execute the command.
		 */
		class c_sphere: public command
		{
		public:
			const char* get_name () { return "sphere"; }
			
			const char*
			get_summary ()
				{ return "Draws a three-dimensional sphere centered at a point."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.sphere"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /polygon -
		 * 
		 * Creates a wireframe rendering of the polygon specified by marked points.
		 * 
		 * Permissions:
		 *   - command.draw.polygon
		 *       Needed to execute the command.
		 */
		class c_polygon: public command
		{
		public:
			const char* get_name () { return "polygon"; }
			
			const char*
			get_summary ()
				{ return "Creates a wireframe rendering of the polygon specified by marked points."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.polygon"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
		
		
		/* 
		 * /curve -
		 * 
		 * Draws a curve between a given set of points.
		 * Unlike /bezier, this command will attempt to make the curve pass through
		 * ALL points except for the first and last.
		 * 
		 * Permissions:
		 *   - command.draw.curve
		 *       Needed to execute the command.
		 */
		class c_curve: public command
		{
		public:
			const char* get_name () { return "curve"; }
			
			const char*
			get_summary ()
				{ return "Draws a curve between a given set of points."; }
			
			const char*
			get_help ()
			{
				return "";
			}
			
			const char* get_exec_permission () { return "command.draw.curve"; }
			
		//----
			void execute (player *pl, command_reader& reader);
		};
	}
}

#endif

