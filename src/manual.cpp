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

#include "manual.hpp"
#include <sstream>
#include <iomanip>


namespace hCraft {
	namespace man {
		
		static void
		convert (std::vector<std::string>& input,
			std::vector<std::string>& output, int max_line)
		{
			bool got_th = false; // whether .TH was encountered
			std::string left_footer;
			std::string center_footer;
			std::string center_header;
			std::string man_name;
			int section_number = 1;
			
			std::ostringstream line;
			
			for (auto itr = input.begin (); itr != input.end (); ++itr)
				{
					std::string& str = *itr;
					
					if (str[0] == '.')
						{
							/* Macro */
							std::string cmd = str.substr (1);
							if (cmd == "TH")
								{
									if (line.tellp () > 0)
										{ output.push_back (line.str ()); line.clear (); line.str (std::string ()); }
									
									got_th = true;
									++ itr; man_name = *itr;
									++ itr; { std::istringstream ss2 (*itr); ss2 >> section_number; }
									++ itr; center_footer = *itr;
									++ itr; left_footer = *itr;
									++ itr; center_header = *itr;	
									
									line << "§7" << man_name << '(' << section_number << ')';
									int space_count = (max_line - (man_name.size () * 2) - center_header.size ()
										- (3 * 2) /* section number plus parentheses */) / 2;
									for (int i = 0; i < space_count; ++i)
										line << ' ';
									line << center_header;
									for (int i = 0; i < space_count; ++i)
										line << ' ';
									line << man_name << '(' << section_number << ')';
									
									output.push_back (line.str ());
									line.clear (); line.str (std::string ());
								}
							else if (cmd == "SH")
								{
									if (line.tellp () > 0)
										{ output.push_back (line.str ()); line.clear (); line.str (std::string ()); }
									
									++ itr;
									std::string& header = *itr;
									line << "§l" << header;
									output.push_back (line.str ());
									line.clear (); line.str (std::string ());
								}
							else if (cmd == "PP")
								{
									if (line.tellp () > 0)
										{ output.push_back (line.str ()); line.clear (); line.str (std::string ()); }
									line << "§e    ";
									output.push_back (line.str ());
									line.clear (); line.str (std::string ());
								}
							else if (cmd == "LN")
								{
									if (line.tellp () > 0)
										{ output.push_back (line.str ()); line.clear (); line.str (std::string ()); }
									line << "§e   ";
								}
							else if (cmd == "TP")
								{
									if (line.tellp () > 0)
										{ output.push_back (line.str ()); line.clear (); line.str (std::string ()); }
									line << "§a       ";
								}
						}
					else
						{
							if ((unsigned int)line.tellp () + str.size () > (unsigned int)max_line)
								{
									output.push_back (line.str ());
									line.clear ();
									line.str (std::string ());
								}
								
							if (line.tellp () == 0)
								line << "§e    ";
							else
								line << ' ';
							
							for (size_t i = 0; i < str.size (); ++i)
								{
									char c = str[i];
									if (c == '$')
										{
											if (i < (str.size () - 1))
												{
													switch (str[i + 1])
														{
															case 'w': line << "§f"; ++i; break;
															case 'y': line << "§e"; ++i; break;
															case 'g': line << "§a"; ++i; break;
															case 'G': line << "§6"; ++i; break;
															
															case '\'': line << '"'; ++i; break;
															
															default:
																line << '$'; break;
														}
												}
											else
												line << c;
										}
									else
										line << c;
								}
						}
				}
			
			// push remanining line
			if (line.tellp () > 0)
				output.push_back (line.str ());
			line.clear (); line.str (std::string ());
			
			// footer
			if (got_th)
				{
					int space_count = (max_line - left_footer.size () - center_footer.size ()
						- man_name.size () - 3 /* section number plus parentheses */) / 2;
					line << "§7" << left_footer;
					for (int i = 0; i < space_count; ++i)
						line << ' ';
					line << center_footer;
					for (int i = 0; i < space_count; ++i)
						line << ' ';
					line << man_name << '(' << section_number << ')';
					
					output.push_back (line.str ());
				}
		}
		
		/* 
		 * Converts the specified string written in a MAN-like markup\macro language
		 * to pretty Minecraft text (as a vector of lines).
		 * 
		 * Format:
		 * 
		 * .TH [name of the thing being described] [section number] [center footer]
		 *     [left footer] [header]
		 */
		void
		format (std::vector<std::string>& output, int max_line,
			const std::string& input)
		{
			std::istringstream ss (input);
			std::string str;
			int c;
			
			std::vector<std::string> literals;
			
			ss >> std::noskipws;
			while (!ss.fail () && !ss.eof ())
				{
					if (ss.peek () == ' ')
						{
							if (!str.empty ())
								literals.push_back (str);
							str.clear ();
						}
					
					// skip spaces
					while (!ss.fail () && !ss.eof ())
						{
							if (ss.get () != ' ')
								{ ss.unget (); break; }
						}
					
					c = ss.get ();
					if (c == '"')
						{
							// read string
							while (((c = ss.get ()) != '"') && !ss.fail ())
								str.push_back (c);
						}
					else
						str.push_back (c);
				}
			
			// push remaining string
			if (!str.empty ())
				literals.push_back (str);
			
			if (!literals.empty ())
				convert (literals, output, max_line);
		}
	}
}

