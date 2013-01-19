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

#include "command.hpp"
#include "player.hpp"
#include "manual.hpp"
#include "blocks.hpp"
#include "stringutils.hpp"

#include "infoc.hpp"
#include "chatc.hpp"
#include "miscc.hpp"
#include "worldc.hpp"
#include "drawc.hpp"

#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <cctype>


namespace hCraft {
	
	// info commands:
	static command* create_c_help () { return new commands::c_help (); }
	
	// chat commands:
	static command* create_c_me () { return new commands::c_me (); }
	static command* create_c_nick () { return new commands::c_nick (); }
	
	// misc commands:
	static command* create_c_ping () { return new commands::c_ping (); }
	
	// world commands:
	static command* create_c_wcreate () { return new commands::c_wcreate (); }
	static command* create_c_wload () { return new commands::c_wload (); }
	static command* create_c_wunload () { return new commands::c_wunload (); }
	static command* create_c_world () { return new commands::c_world (); }
	static command* create_c_tp () { return new commands::c_tp (); }
	static command* create_c_physics () { return new commands::c_physics (); }
	
	// draw commands:
	static command* create_c_selection () { return new commands::c_selection (); }
	static command* create_c_fill () { return new commands::c_fill (); }
	
	/* 
	 * Returns a new instance of the command named @{name}.
	 */
	command*
	command::create (const char *name)
	{
		static std::unordered_map<std::string, command* (*)()> creators {
			{ "help", create_c_help },
			{ "me", create_c_me },
			{ "ping", create_c_ping },
			{ "wcreate", create_c_wcreate },
			{ "wload", create_c_wload },
			{ "world", create_c_world },
			{ "tp", create_c_tp },
			{ "nick", create_c_nick },
			{ "wunload", create_c_wunload },
			{ "physics", create_c_physics },
			{ "selection", create_c_selection },
			{ "fill", create_c_fill },
			};
		
		auto itr = creators.find (name);
		if (itr == creators.end ())
			return nullptr;
		
		return itr->second ();
	}
	
//----
	
	/* 
	 * Returns a null-terminated array of aliases (secondary names).
	 */
	const char**
	command::get_aliases ()
	{
		static const char* arr[] = { nullptr };
		return arr;
	}
	
	
	
//----
	
	bool
	command_reader::option::is_int () const 
	{
		if (!this->found_arg)
			return false;
		return sutils::is_int (this->as_string ());
	}
	
	int
	command_reader::option::as_int () const 
	{
		return sutils::to_int (this->as_string ());
	}
	
	
	
//----
	
	/* 
	 * Constructs a new command reader from the given string (should be in the
	 * form of: /<cmd> <arg1> <arg2> ... <argN>
	 */
	command_reader::command_reader (const std::string& str)
	{
		// extract the command name.
		size_t i = 1;
		std::string::size_type sp = str.find_first_of (' ');
		this->name.reserve (((sp == std::string::npos) ? str.size () : sp) + 1);
		for (size_t j = ((sp == std::string::npos) ? str.size () : sp); i < j; ++i)
			this->name.push_back (str[i]);
		
		// extract the arguments as a whole.
		this->args.reserve ((str.size () - i) + 1);
		for (++i; i < str.size (); ++i)
			this->args.push_back (str[i]);
		
		this->arg_offset = 0;
	}
	
	
	
	/* 
	 * Returns the name of the parsed command.
	 */
	const std::string&
	command_reader::command_name ()
	{
		return this->name;
	}
	
	
	
	/* 
	 * Adds the specified option to the parser's option list.
	 * These will be parsed in a call to parse_args ().
	 */
	void
	command_reader::add_option (const char *long_name, const char *short_name,
		bool has_arg, bool arg_required, bool opt_required)
	{
		this->options.emplace_back (long_name, short_name, has_arg, arg_required,
			opt_required);
	}
	
	/* 
	 * Finds and returns the option with the given long name.
	 */
	command_reader::option*
	command_reader::opt (const char *long_name)
	{
		for (option& opt : this->options)
			if (std::strcmp (opt.lname, long_name) == 0)
				return &opt;
		return nullptr;
	}
	
	
	
	static bool
	_read_string (std::istringstream& ss, std::string& iarg, std::string& out,
		player *err)
	{
		std::ostringstream oss;
		size_t i;
		bool found_end = false;
		
		for (i = 1; i < iarg.size (); ++i)
			{
				if (iarg[i] == '\\')
					{
						// character escape
						if (iarg.size () > (i + 1) && iarg[i + 1] == '"')
							{ oss << '"'; ++i; }
						else
							oss << '\\';
					}
				else if (iarg[i] == '"')
					{ found_end = true; break; }
				else
					oss << iarg[i];
			}
		
		char c;
		//ss >> std::noskipws;
		while (!found_end && !ss.fail () && !ss.eof ())
			{
				ss >> c;
				if (c == '"')
					{ found_end = true; break; }
				else
					oss << c;
			}
		//s >> std::skipws;
		
		if (!found_end)
			{
				err->message ("§c * §eIncomplete string§f.");
				return false;
			}
		
		out = oss.str ();
		return true;
	}
	
	/* 
	 * Parses the argument list.
	 * In case of an error, false is returned and an appropriate message is sent
	 * to player @{err}.
	 */
	bool
	command_reader::parse_args (command *cmd, player *err, bool handle_help)
	{
		if (handle_help)
			{
				this->add_option ("help", "h", true);
				this->add_option ("summary", "s");
			}
		
		std::istringstream ss {this->args};
		std::string str;
		std::string opt_name;
		std::string opt_arg;
		bool has_str = false;
		bool found_nonopts = false;
		
		int strm_pos;
		
		ss >> std::noskipws;
		
		// read options
		while (!ss.fail () && !ss.eof ())
			{
				strm_pos = ss.tellg ();
				if (!has_str)
					{
						while (!ss.fail () && !ss.eof ())
							{
								if (ss.get () != ' ')
									{ ss.unget (); break; }
							}
						
						strm_pos = ss.tellg ();
						str.clear ();
						ss >> str;
					}
				has_str = false;
				
				if (str.empty ())
					break;
				
				if (str[0] == '\\')
					{
						if (str.size () > 1 && str[1] == '\\')
							{
								if (str.size () == 2)
									break; // end of option list.
								
								if (found_nonopts)
									{
										err->message ("§c * §eOptions should be specified before the arguments");
										return false;
									}
									
								/* long option */
								std::string::size_type eq = str.find_first_of ('=');
								bool has_arg = (eq != std::string::npos);
								opt_name = str.substr (2, (eq == std::string::npos)
									? (str.size () - 2) : (eq - 2));
								
								auto itr = std::find_if (this->options.begin (), this->options.end (),
									[&opt_name] (const option& opt) -> bool
										{
											return std::strcmp (opt.long_name (), opt_name.c_str ()) == 0;
										});
								if (itr == this->options.end ())
									{
										err->message ("§c * §eUnrecognized option§f: §c--" + opt_name);
										return false;
									}
								
								option& opt = *itr;
								opt.was_found = true;
								if (opt.has_arg)
									{
										if (has_arg)
											{
												opt_arg = str.substr (eq + 1, str.size () - (eq + 1));
												if (opt_arg.size () > 0 && opt_arg[0] == '"')
													{
														if (!_read_string (ss, opt_arg, opt_arg, err))
															return false;
													}
												
												opt.arg = std::move (opt_arg);
												opt.found_arg = true;
											}
										else if (opt.arg_req)
											{
												err->message ("§c * §eArgument required for option§f: §c--" + opt_name);
												return false;
											}
									}
							}
						else
							{
								if (str.size () == 1)
									{
										this->non_opts.push_back (str);
										continue;
									}
								
								if (found_nonopts)
									{
										err->message ("§c * §eOptions should be specified before the arguments");
										return false;
									}
									
								/* short option(s) */
								for (size_t i = 1; i < str.size (); ++i)
									{
										char optc = str[i];
										auto itr = std::find_if (this->options.begin (), this->options.end (),
											[optc] (const option& opt) -> bool
												{
													return (opt.short_name () && opt.short_name ()[0] == optc);
												});
										if (itr == this->options.end ())
											{
												err->message ("§c * §eUnrecognized option§f: §c-" + std::string (1, optc));
												return false;
											}
										
										option& opt = *itr;
										opt.was_found = true;
										
										if (opt.has_arg)
											{
												if (opt.arg_req && i != (str.size () - 1))
													{
														err->message ("§c * §eArgument required for option§f: §c-" + std::string (opt.sname));
														return false;
													}
												
												if (i == (str.size () - 1))
													{
														if (opt.arg_req && (ss.eof () || ss.fail ()))
															{
																err->message ("§c * §eArgument required for option§f: §c-" + std::string (opt.sname));
																return false;
															}
															
														{
															while (!ss.fail () && !ss.eof ())
																{
																	if (ss.get () != ' ')
																		{ ss.unget (); break; }
																}
															
															str.clear ();
															ss >> str;
														}
														
														if (!str.empty ())
															{
																if (str[0] == '\\')
																	{
																		has_str = true;
																	}
																else if (str[0] != ' ')
																	{
																		opt.found_arg = true;
																		if (str[0] == '"')
																			{
																				if (!_read_string (ss, str, str, err))
																					return false;
																			}
																
																		opt.arg = std::move (str);
																	}
															}
													}
											}
									}
							}
					}
				
				// non-option arguments:
				else
					{
						if (str[0] == '"')
							{
								if (!_read_string (ss, str, str, err))
									return false;
							}
						
						int pos_start = strm_pos;
						int pos_end   = ss.tellg ();
						
						this->non_opts.push_back (str);
						this->non_opts_info.push_back (std::make_pair (pos_start, pos_end));
						found_nonopts = true;
					}
			}
		
		// read non-option arguments
		while (!ss.fail () && !ss.eof ())
			{
				while (!ss.fail () && !ss.eof ())
					{
						if (ss.get () != ' ')
							{ ss.unget (); break; }
					}
				
				strm_pos = ss.tellg ();
				str.clear ();
				ss >> str;
				if (str.empty ()) break;
				
				if (str[0] == '"')
					{
						if (!_read_string (ss, str, str, err))
							return false;
					}
				
				int pos_start = strm_pos;
				int pos_end   = ss.tellg ();
				
				this->non_opts.push_back (str);
				this->non_opts_info.push_back (std::make_pair (pos_start, pos_end));
				found_nonopts = true;
			}
		
		for (option& opt : this->options)
			{
				if (opt.required && !opt.was_found)
					{
						err->message ("§c * §eRequired argument not found§f: §c--" + std::string (opt.lname));
						return false;
					}
			}
		
		if (handle_help)
			{
				if (this->opt ("summary")->found ())
					{ cmd->show_summary (err); return false; }
				else if (this->opt ("help")->found ())
					{
						option& opt = *this->opt ("help");
						if (opt.got_arg ())
							{
								if (!opt.is_int ())
									{ err->message ("§c * §eInvalid page number§: §c" + opt.as_string ()); return false; }
								int page = opt.as_int ();
								cmd->show_help (err, page, 12);
							}
						else
							cmd->show_help (err, 1, 12);
						return false;
					}
			}
					
		return true;
	}
	
	
	
	/* 
	 * Returns the next argument from the argument string.
	 */
	cmd_arg
	command_reader::next ()
	{
		static std::string str_empty;
		if (this->arg_offset >= this->arg_count ())
			return cmd_arg (str_empty);
		return cmd_arg (this->non_opts [this->arg_offset++]);
	}
	
	cmd_arg
	command_reader::peek_next ()
	{
		static std::string str_empty;
		if (this->arg_offset >= this->arg_count ())
			return cmd_arg (str_empty);
		return cmd_arg (this->non_opts [this->arg_offset]);
	}
	
	bool
	command_reader::has_next ()
	{
		return (this->arg_offset < (int)this->non_opts.size ());
	}
	
	/* 
	 * Returns everything that has not been read yet from the argument string.
	 */
	std::string
	command_reader::rest ()
	{
		return all_from (this->arg_offset);
	}
	
	
	
//------------
	
	void
	command::show_summary (player *pl)
	{
		pl->message ("§6Summary for command §a" + std::string (this->get_name ()) + "§f:");
		pl->message_spaced ("§e    " + std::string (this->get_summary ()));
		if (this->get_aliases ()[0] != nullptr)
			{
				std::ostringstream ss;
				ss << "§e    Aliases§f: ";
				
				const char **aliases = this->get_aliases();
				int count = 0;
				while (*aliases)
					{
						if (count > 0)
							ss << "§f, ";
						ss << "§a" << (*aliases++);
						++ count;
					}
				
				pl->message (ss.str ());
			}
	}
	
	void
	command::show_help (player *pl, int page, int lines_per_page)
	{
		std::vector<std::string> manual;
		man::format (manual, 60, this->get_help ());
		
		int page_count = manual.size () / lines_per_page;
		if (manual.size () % lines_per_page != 0)
			++ page_count;
		if (manual.size () <= 1)
			{ pl->message ("§aThis command has no help written for it yet."); return; }
		if (page < 1 || page > page_count)
			{ pl->message ("§c* §eNo such page§f."); return; }
		
		-- page;
		if (page < 0) page = 0;
		
		if ((int)manual.size () <= lines_per_page)
			{
				for (auto& str : manual)
					pl->message (str);
				return;
			}
		
		for (size_t i = 0; i < (size_t)lines_per_page; ++i)
			{
				size_t j = page * lines_per_page + i;
				if (j >= manual.size ())
					break;
				
				pl->message (manual[j]);
			}
		
		{
			std::ostringstream ss;
			ss << "§6                                                      PAGE " << (page + 1)
				 << "/§8" << page_count;
			pl->message (ss.str ());
		}
	}
	
	
	
//------------
	
	cmd_arg::cmd_arg (std::string& str)
		: str (str)
		{ }
	
	static std::string empty_str;
	cmd_arg::cmd_arg ()
		: cmd_arg (empty_str)
		{ }
	
	
	bool
	cmd_arg::is_int ()
	{
		return sutils::is_int (this->str);
	}

	bool
	cmd_arg::is_block ()
	{
		return sutils::is_block (this->str);
	}
	
	
	
	int
	cmd_arg::as_int ()
	{
		return sutils::to_int (this->str);
	}
	
	blocki
	cmd_arg::as_block ()
	{
		return sutils::to_block (this->str);
	}
	
	
//------------
	
	/* 
	 * Class destructor.
	 * Destroys all registered commands.
	 */
	command_list::~command_list ()
	{
		for (auto itr = this->commands.begin (); itr != this->commands.end (); ++itr)
			{
				command *cmd = itr->second;
				delete cmd;
			}
		this->commands.clear ();
	}
	
	
	
	/* 
	 * Adds the specified command to the list.
	 */
	void
	command_list::add (command *cmd)
	{
		if (!cmd)
			throw std::runtime_error ("command cannot be null");
		
		this->commands[cmd->get_name ()] = cmd;
		
		// register aliases
		const char **aliases = cmd->get_aliases ();
		const char **ptr     = aliases;
		while (*ptr)
			{
				const char *alias = *ptr++;
				auto itr = this->aliases.find (alias);
				if (itr != this->aliases.end ())
					throw std::runtime_error ("alias collision");
				
				this->aliases[alias] = cmd->get_name ();
			}
	}
	
	/* 
	 * Finds the command that has the specified name (case-sensitive)
	 * (also checks aliases).
	 */
	command*
	command_list::find (const char *name)
	{
		auto itr = this->commands.find (name);
		if (itr != this->commands.end ())
			return itr->second;
		
		// check aliases
		auto aitr = this->aliases.find (name);
		if (aitr != this->aliases.end ())
			{
				const std::string& cmd_name = aitr->second;
				itr = this->commands.find (cmd_name);
				if (itr != this->commands.end ())
					return itr->second;
			}
		
		return nullptr;
	}
}

