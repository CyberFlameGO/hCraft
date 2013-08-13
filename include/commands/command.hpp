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

#ifndef _hCraft__COMMAND_H_
#define _hCraft__COMMAND_H_

#include <vector>
#include <string>
#include <unordered_map>
#include <utility>
#include "chunk.hpp"


namespace hCraft {
	
	class player; // forward dec
	class command;
	
	
	/* 
	 * Command argument parser.
	 */
	class command_reader
	{
		friend class command_parser;
		
	public:
		
		/* 
		 * Represents a command argument.
		 */
		class argument
		{
			friend class command_reader;
			friend class command_parser;
			
			std::string str;
			int start, end;
			int ws; // whitespace right after the argument
		
		public:
			argument ();
			argument (std::string str);
		
			bool is_int ();
			bool is_float ();
			bool is_block ();
		
			std::string& as_str () { return this->str; }
			const char* as_cstr () { return this->str.c_str (); }
			int as_int ();
			double as_float ();
			blocki as_block ();
		
			operator std::string&() { return this->str; }
			operator int() { return this->as_int (); }
			operator blocki() { return this->as_block (); }
		};
		
		
		/* 
		 * Represents a command option of the form:
		 *   \<opts> [args;]       : short form
		 *   \\<opt> [args;]       : long form
		 * 
		 * When more than
		 * \r 55, 10;
		 */
		class option
		{
			friend class command_reader;
			friend class command_parser;
			
			const char *lname;
			const char *sname;
			bool required;
			bool was_found;
			
			// min\max amount of arguments this option takes.
			int min_args; // if 0, no arguments are expected
			int max_args; // if 0, no limit on argument count
			std::vector<argument> args;
			
		public:
			option (const char *lname, const char *sname, int min_args = 0, int max_args = 0,
				bool opt_req = false)
			{
				this->lname = lname;
				this->sname = sname;
				this->min_args = min_args;
				this->max_args = (max_args == 0) ? ((min_args == 0) ? 0 : min_args) : max_args;
				this->required = opt_req;
				this->was_found = false;
			}
			
		public:
			inline const char* long_name () const { return this->lname; }
			inline const char* short_name () const { return this->sname; }
			
			inline bool found () const { return this->was_found; }
			inline bool is_required () const { return this->required; }
			
			inline bool args_required () const { return this->min_args > 0; }
			inline bool expects_args () const { return (this->min_args > 0) || (this->max_args > 0); }
			inline bool got_args () const { return !this->args.empty (); }
			inline int arg_count () const { return this->args.size (); }
			inline argument& arg (int n) { return this->args.at (n); }
			inline std::vector<argument>& get_args () { return this->args; }
			inline int min_args_expected () { return this->min_args; }
			inline int max_args_expected () { return this->max_args; }
		};
		
	private:
		std::string name;
		std::string args;
		std::vector<option> options;
		std::vector<argument> non_opts;
		int arg_offset;
		
	public:
		inline std::vector<option>& get_opts () { return this->options; }
		inline std::vector<argument>& get_args () { return this->non_opts; }
		
		inline bool no_opts () { return this->options.size () == 0; }
		inline bool has_opts () { return this->options.size () > 0; }
		inline int  opt_count () { return this->options.size (); }
		
		inline bool no_args () { return this->non_opts.size () == 0; }
		inline bool has_args () { return this->non_opts.size () > 0; }
		inline int  arg_count () { return this->non_opts.size (); }
		inline std::string& arg (int n) { return this->non_opts[n]; }
		inline const std::string& get_arg_string () { return this->args; }
		
		inline int seek (int n = 0) { int off = this->arg_offset; this->arg_offset = n; return off; }
		inline int offset () { return this->arg_offset; }
		
	public:
		/* 
		 * Constructs a new command reader from the given string (should be in the
		 * form of: /<cmd> <arg1> <arg2> ... <argN>
		 */
		command_reader (const std::string& str);
		
		
		/* 
		 * Returns the name of the parsed command.
		 */
		const std::string& command_name ();
		
		
		
		/* 
		 * Adds the specified option to the parser's option list.
		 * These will be parsed in a call to parse_args ().
		 */
		void add_option (const char *long_name, const char *short_name = "",
			int min_args = 0, int max_args = 0, bool opt_required = false);
		
		
		/* 
		 * Finds and returns the option with the given long name.
		 */
		option* opt (const char *long_name);
		
		/* 
		 * Short name equivalent.
		 */
		option* short_opt (char name);
		
		
		
		/* 
		 * Parses the argument\option list.
		 * In case of an error, false is returned and an appropriate message is sent
		 * to player @{err}.
		 */
		bool parse (command *cmd, player *err, bool handle_help = true);
		
	//----
		
		/* 
		 * Returns the next argument from the argument string.
		 */
		argument next ();
		argument peek_next ();
		bool has_next ();
		
		/* 
		 * Returns everything that has not been read yet from the argument string.
		 */
		std::string rest ();
		
		std::string all_from (int n);
		std::string all_after (int n);
	};
	
	
	/* 
	 * The base class for all commands.
	 */
	class command
	{
	public:
		virtual ~command () { } // destructor
		
		/* 
		 * Returns the name of the command (the same one used to execute it, /<name>).
		 */
		virtual const char* get_name () = 0;
		
		/* 
		 * Returns a null-terminated array of aliases (secondary names).
		 */
		virtual const char** get_aliases ();
		
		/* 
		 * Returns a short summary of what the command does (a brief sentence or two,
		 * highlighting the main points).
		 */
		virtual const char* get_summary () = 0;
		
		/* 
		 * Returns the MAN page (as a string) for this command.
		 */
		virtual const char* get_help () = 0;
		
		/* 
		 * Returns the permission node needed to execute the command.
		 */
		virtual const char* get_exec_permission () = 0;
		
	//----
		
		virtual void show_summary (player *pl);
		virtual void show_help (player *pl, int page, int lines_per_page);
		
	//----
		
		/* 
		 * Executes the command on the specified player.
		 */
		virtual void execute (player *pl, command_reader& reader) = 0;
		
	//----
		
		/* 
		 * Returns a new instance of the command named @{name}.
		 */
		static command* create (const char *name);
	};
	
	
	class command_list
	{
		std::unordered_map<std::string, command *> commands;
		std::unordered_map<std::string, std::string> aliases;
		
	public:
		/* 
		 * Class destructor.
		 * Destroys all registered commands.
		 */
		~command_list ();
		
		
		
		/* 
		 * Adds the specified command to the list.
		 */
		void add (command *cmd);
		
		/* 
		 * Finds the command that has the specified name (case-sensitive)
		 * (also checks aliases).
		 */
		command* find (const char *name);
	};
}

#endif

