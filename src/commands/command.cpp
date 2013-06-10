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

#include "commands/command.hpp"
#include "player.hpp"
#include "manual.hpp"
#include "blocks.hpp"
#include "stringutils.hpp"

#include "commands/infoc.hpp"
#include "commands/chatc.hpp"
#include "commands/miscc.hpp"
#include "commands/worldc.hpp"
#include "commands/drawc.hpp"
#include "commands/adminc.hpp"

#include <unordered_map>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <cctype>
#include <list>


namespace hCraft {
	
	// info commands:
	static command* create_c_help () { return new commands::c_help (); }
	static command* create_c_status () { return new commands::c_status (); }
	static command* create_c_money () { return new commands::c_money (); }
	
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
	static command* create_c_wsetspawn () { return new commands::c_wsetspawn (); }
	
	// draw commands:
	static command* create_c_select () { return new commands::c_select (); }
	static command* create_c_fill () { return new commands::c_fill (); }
	static command* create_c_cuboid () { return new commands::c_cuboid (); }
	static command* create_c_line () { return new commands::c_line (); }
	static command* create_c_bezier () { return new commands::c_bezier (); }
	static command* create_c_aid () { return new commands::c_aid (); }
	static command* create_c_circle () { return new commands::c_circle (); }
	static command* create_c_ellipse () { return new commands::c_ellipse (); }
	static command* create_c_sphere () { return new commands::c_sphere (); }
	static command* create_c_polygon () { return new commands::c_polygon (); }
	static command* create_c_curve () { return new commands::c_curve (); }
	
	// admin commands
	static command* create_c_gm () { return new commands::c_gm (); }
	static command* create_c_rank () { return new commands::c_rank (); }
	static command* create_c_kick () { return new commands::c_kick (); }
	static command* create_c_ban () { return new commands::c_ban (); }
	static command* create_c_unban () { return new commands::c_unban (); }
	static command* create_c_mute () { return new commands::c_mute (); }
	static command* create_c_unmute () { return new commands::c_unmute (); }
	
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
			{ "select", create_c_select },
			{ "fill", create_c_fill },
			{ "gm", create_c_gm },
			{ "cuboid", create_c_cuboid },
			{ "line", create_c_line },
			{ "bezier", create_c_bezier },
			{ "aid", create_c_aid },
			{ "circle", create_c_circle },
			{ "ellipse", create_c_ellipse },
			{ "sphere", create_c_sphere },
			{ "polygon", create_c_polygon },
			{ "curve", create_c_curve },
			{ "rank", create_c_rank },
			{ "status", create_c_status },
			{ "money", create_c_money },
			{ "kick", create_c_kick },
			{ "ban", create_c_ban },
			{ "unban", create_c_unban },
			{ "mute", create_c_mute },
			{ "wsetspawn", create_c_wsetspawn },
			{ "unmute", create_c_unmute },
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
		int min_args, int max_args, bool opt_required)
	{
		this->options.emplace_back (long_name, short_name, min_args, max_args,
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
	
	/* 
	 * Short name equivalent.
	 */
	command_reader::option*
	command_reader::short_opt (char name)
	{
		for (option& opt : this->options)
			if (opt.sname && (opt.sname[0] == name))
				return &opt;
		return nullptr;
	}
	
	
	
//------------------------------------------------------------------------------
	/* 
	 * Command parsing:
	 */
	
	/* 
	 * Converts a stream of characters into a more managable list of tokens.
	 */
	class command_parser
	{
		command_reader& reader;
		std::istringstream ss;
		
	private:
		int
		skip_whitespace ()
		{
			int c, r = 0;
			while (!ss.eof ())
				{
					c = ss.peek ();
					if (c == ' ')
						{
							ss.get ();
							++ r;
						}
					else
						break;
				}
			
			return r;
		}
		
		
		// expects no whitespace at the beginning.
		// first character must be "
		int
		parse_string (player *err, std::string& out)
		{
			ss.get (); // consume "
			
			int c;
			for (;;)
				{
					c = ss.get ();
					if (c == std::istringstream::traits_type::eof ())
						{
							err->message ("§c * §7Unexpected end of string§c.");
							return -1;
						}
					
					if (c == '"')
						break;
					
					if (c == '\\')
						{
							// handle escape characters
							if (ss.eof ())
								{
									err->message ("§c * §7Expected escape character in string§c.");
									return -1;
								}
							
							c = ss.get ();
							switch (c)
								{
									case 'n': out.push_back ('\n'); break;
									case '\\': out.push_back ('\\'); break;
									case '"': out.push_back ('"'); break;
									
									default:
										err->message ("§c * §7Invalid escape character§f: §c\\" + std::string (1, c));
										return -1;
								}
						}
					else
						out.push_back (c);
				}
			
			return 0;
		}
		
		// expects no whitespace at the beginning
		int
		parse_arg (player *err, std::string& out, bool opt_args = false)
		{
			int c = ss.peek ();
			if (c == '"')
				{
					if (parse_string (err, out) == -1) 
						return -1;
				}
			else
				{
					// extracts characters until whitespace is encountered
					for (;;)
						{
							c = ss.peek ();
							if (c == ' ' || c == std::istringstream::traits_type::eof ())
								break;
							if (opt_args && (c == ',' || c == ';'))
								{
									// don't consume the comma\semicolon
									break;
								}
							out.push_back (c);
							ss.get ();
						}
				}
			
			return 0;
		}
		
		
		int
		parse_opt_args (player *err, std::vector<command_reader::option *>& opts)
		{
			if (opts.empty ()) 
				return 0;
			
			// all options must expect the same minimal amount of arguments
			int min_args = opts[0]->min_args_expected ();
			for (int i = 1; i < (int)opts.size (); ++i)
				if (opts[i]->min_args_expected () != min_args)
					{
						err->message ("§c * §7All grouped short options must expect the same amount of arguments§c.");
						return -1;
					}
			
			// no args required
			if (!opts[0]->expects_args ())
				return 0;
			
			int args_extracted = 0;
			int c;
			for (;;)
				{
					this->skip_whitespace ();
					if (ss.eof ())
						{
							if (args_extracted < min_args)
								{
									std::ostringstream ess;
									if (opts.size () == 1)
										ess << "§c * §7Option §4\\\\§c" << opts[0]->long_name ()
												<< " §7expects at least §c" << opts[0]->min_args_expected ()
												<< " §7argument(s)§c.";
									else
										ess << "§c * §7Option group expects at least §c" << min_args << " §7argument(s)§c.";
									err->message (ess.str ());
									return -1;
								}
							break;
						}
					
					std::string argstr;
					if (parse_arg (err, argstr, true) == -1)
						return -1;
					if (!argstr.empty ())
						{
							for (auto opt : opts)
								opt->args.emplace_back (argstr);
							++ args_extracted;
						}
					
					// NOTE: whitespace may NOT follow a semicolon, otherwise it would
					//       be considered a regular argument.
					c = ss.peek ();
					if (c == ';' && !argstr.empty ())
						{
							// end of arguments
							ss.get ();
							break;
						}
					
					this->skip_whitespace (); 
					if (ss.eof ())
						{
							if (args_extracted < min_args)
								{
									std::ostringstream ess;
									if (opts.size () == 1)
										ess << "§c * §7Option §4\\\\§c" << opts[0]->long_name ()
												<< " §7expects at least §c" << opts[0]->min_args_expected ()
												<< " §7argument(s)§c.";
									else
										ess << "§c * §7Option group expects at least §c" << min_args << " §7argument(s)§c.";
									err->message (ess.str ());
									return -1;
								}
							break;
						}
					c = ss.peek ();
					if (c == ',')
						{
							// continue reading arguments
							ss.get ();
							continue;
						}
					
					// end of arguments
					break;
				}
			
			return 0;
		}
		
		
		
		int
		parse_short_opt (player *err)
		{
			int c;
			bool read_args = true;
			
			std::string optstr;
			for (;;)
				{
					c = ss.peek ();
					if (c == ';')
						{
							ss.get ();
							read_args = false;
							break;
						}
					if (c == ' ' || c == std::istringstream::traits_type::eof ())
						break;
					
					optstr.push_back (ss.get ());
				}
			
			if (optstr.empty ())
				{
					err->message ("§c * §7Expected option characters§c.");
					return -1;
				}
			
			// compile option list
			std::vector<command_reader::option *> opts;
			for (char c : optstr)
				{
					command_reader::option *opt = this->reader.short_opt (c);
					if (!opt)
						{
							err->message ("§c * §7No such command option§f: §4\\§c" + std::string (1, c));
							return -1;
						}
					
					opts.push_back (opt);
					opt->was_found = true;
				}
			
			// parse read arguments, if any
			if (opts[0]->expects_args ())
				{
					if (read_args)
						{
							if (this->parse_opt_args (err, opts) == -1)
								return -1;
						}
					else
						{
							std::ostringstream ess;
							ess << "§c * §7Option group expects at least §c" << opts[0]->min_args_expected () << " §7argument(s)§c.";
							err->message (ess.str ());
							return -1;
						}
				}
			
			return 0;
		}
		
		int
		parse_long_opt (player *err, bool& parsing_options)
		{
			int c = -1;
			bool read_args = true;
			
			if (ss.eof () || ((c = ss.peek ()) == ' '))
				{
					// stop parsing options
					parsing_options = false;
					return 0;
				}
			
			// read option name
			std::string name;
			for (;;)
				{
					c = ss.peek ();
					if (c == std::istringstream::traits_type::eof ())
						{
							err->message ("§c * §7Expected option name§c.");
							return -1;
						}
					
					if (c == ' ')
						break; // we're done
					if (c == ';')
						{
							ss.get ();
							read_args = false;
							break;
						}
					
					if (!(std::isalnum (c) || (c == '-' || c == '_' || c == '.')))
						{
							err->message ("§c * §7Invalid option name§f: §c" + name);
							return -1;
						}
					
					ss.get ();
					name.push_back (c);
				}
			
			if (name.empty ())
				{
					err->message ("§c * §7Expected option name§c.");
					return -1;
				}
			
			// make sure the option exists
			command_reader::option *opt = reader.opt (name.c_str ());
			if (!opt)
				{
					err->message ("§c * §7No such command option§f: §4\\\\§c" + name);
					return -1;
				}
			
			opt->was_found = true;
			
			// parse arguments, if any
			this->skip_whitespace ();
			if (!ss.eof () && read_args)
				{
					std::vector<command_reader::option *> opts (1, opt);
					if (this->parse_opt_args (err, opts) == -1)
						return -1;
				}
			else
				{
					if (opt->min_args > 0)
						{
							std::ostringstream ess;
							ess << "§c * §7Option §4\\\\§c" << opt->long_name ()
									<< " §7expects at least §c" << opt->min_args_expected ()
									<< " §7argument(s)§c.";
							err->message (ess.str ());
							return -1;
						}
				}
			
			return 0;
		}
		
	public:
		command_parser (command_reader& reader, const std::string& str)
			: reader (reader), ss (str)
			{ }
		
		
		bool
		parse (player *err)
		{
			bool parsing_options = true;
			int c;
			
			this->skip_whitespace ();
			for (;;)
				{
					// options
					c = ss.peek ();
					if (c == std::istringstream::traits_type::eof ())
						break;
					
					if (c == '\\')
						{
							ss.get ();
							
							c = ss.peek ();
							if (c == '\\')
								{
									ss.get ();
									
									if (parse_long_opt (err, parsing_options) == -1)
										return false;
								}
							else
								{
									if (parse_short_opt (err) == -1)
										return false;
								}
							
							this->skip_whitespace ();
						}
					else
						{
							// regular arguments
							int start = (int)ss.tellg ();
							std::string argstr;
							if (parse_arg (err, argstr) == -1)
								return -1;
							int end = (int)ss.tellg ();
							
							command_reader::argument arg (argstr);
							arg.start = start;
							arg.end = end;
							arg.ws = this->skip_whitespace ();
							this->reader.non_opts.push_back (arg);
						}
				}
			
			return true;
		}
	};
	
	/* 
	 * Parses the argument list.
	 * In case of an error, false is returned and an appropriate message is sent
	 * to player @{err}.
	 */
	bool
	command_reader::parse (command *cmd, player *err, bool handle_help)
	{
		if (handle_help)
			{
				this->add_option ("help", "h", 0, 1);
				this->add_option ("summary", "s");
			}
		
		command_parser parser (*this, this->args);
		if (!parser.parse (err))
			return false;
		
		if (handle_help)
			{
				if (this->opt ("summary")->found ())
					{ cmd->show_summary (err); return false; }
				else if (this->opt ("help")->found ())
					{
						option& opt = *this->opt ("help");
						if (opt.got_args ())
							{
								auto& a = opt.arg (0);
								if (!a.is_int ())
									{
										err->message ("§c * §7Invalid page number§: §c" + a.as_str ());
										return false;
									}
								int page = a.as_int ();
								cmd->show_help (err, page, 12);
							}
						else
							cmd->show_help (err, 1, 12);
						return false;
					}
			}
		
		return true;
	}
	
	
	
//------------------------------------------------------------------------------
	
	/* 
	 * Returns the next argument from the argument string.
	 */
	command_reader::argument
	command_reader::next ()
	{
		static std::string str_empty;
		if (this->arg_offset >= this->arg_count ())
			return argument (str_empty);
		return argument (this->non_opts [this->arg_offset++]);
	}
	
	command_reader::argument
	command_reader::peek_next ()
	{
		static std::string str_empty;
		if (this->arg_offset >= this->arg_count ())
			return argument (str_empty);
		return argument (this->non_opts [this->arg_offset]);
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
	
	std::string
	command_reader::all_from (int n)
	{
		std::ostringstream ss;
		for (int i = n; i < (int)this->non_opts.size (); ++i)
			{
				argument& arg = this->non_opts[i];
				ss << this->args.substr (arg.start, arg.end - arg.start);
				for (int j = 0; j < arg.ws; ++j)
					ss << ' ';
			}
		
		return ss.str ();
	}
	
	std::string
	command_reader::all_after (int n)
	{
		std::ostringstream ss;
		for (int i = n + 1; i < (int)this->non_opts.size (); ++i)
			{
				argument& arg = this->non_opts[i];
				ss << this->args.substr (arg.start, arg.end - arg.start);
				for (int j = 0; j < arg.ws; ++j)
					ss << ' ';
			}
		
		return ss.str ();
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
			{ pl->message ("§c* §7No such page§f."); return; }
		
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
	
	command_reader::argument::argument (std::string str)
		: str (str)
	{
		this->ws = 0;
	}
	
	command_reader::argument::argument ()
	{
		this->ws = 0;
	}
	
	
	bool
	command_reader::argument::is_int ()
	{
		return sutils::is_int (this->str);
	}
	
	bool
	command_reader::argument::is_float ()
	{
		return sutils::is_float (this->str);
	}

	bool
	command_reader::argument::is_block ()
	{
		return sutils::is_block (this->str);
	}
	
	
	
	int
	command_reader::argument::as_int ()
	{
		return sutils::to_int (this->str);
	}
	
	double
	command_reader::argument::as_float ()
	{
		return sutils::to_float (this->str);
	}
	
	blocki
	command_reader::argument::as_block ()
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

