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

#include <stdexcept>
#include <string>
#include <vector>
#include <ostream>
#include <istream>


namespace hCraft {
	namespace cfg {
	
		class cfg_error: public std::runtime_error {
		public:
			cfg_error (const char *what)
				: std::runtime_error (what)
				{ }
		};
	
		class cfg_find_error: public cfg_error {
		public:
			cfg_find_error (const char *what)
				: cfg_error (what)
				{ }
		};
	
		class cfg_type_error: public cfg_error {
		public:
			cfg_type_error (const char *what)
				: cfg_error (what)
				{ }
		};
	
	
		enum value_type
		{
			CFG_NONE = -1,
			
			CFG_GROUP = 0,
			CFG_STRING,
			CFG_INTEGER,
			CFG_FLOATING,
			CFG_BOOLEAN,
			CFG_ARRAY,
			CFG_SEPARATOR,
		};
	
		class value
		{
		public:
			virtual ~value () { }
		
			virtual value_type type () = 0;
			virtual void write_to (std::ostream& strm, int indent = 0) = 0;
			virtual void condensed_write_to (std::ostream& strm, int indent)
				{ this->write_to (strm, indent); }
		};
	
	
		class string: public value
		{
			std::string str;
		
		public:
			string ();
			string (const char *str);
			string (const std::string& str);
		
			std::string& val () { return str; }
		
			// assignment operator overloads
			string& operator= (const string& other);
			string& operator= (const char *str);
			string& operator= (const std::string& str);
		
			virtual value_type type () override { return CFG_STRING; }
			virtual void write_to (std::ostream& strm, int indent = 0) override;
		};
	
	
		class integer: public value
		{
			long long num;
		
		public:
			integer ();
			integer (long long num);
		
			long long val () { return num; }
		
			// assignment operator overloads
			integer& operator= (const integer& other);
			integer& operator= (long long num);
		
			virtual value_type type () override { return CFG_INTEGER; }
			virtual void write_to (std::ostream& strm, int indent = 0) override;
		};
	
	
		class floating: public value
		{
			double num;
		
		public:
			floating ();
			floating (double num);
		
			double val () { return num; }
		
			// assignment operator overloads
			floating& operator= (const floating& other);
			floating& operator= (double num);
		
			virtual value_type type () override { return CFG_FLOATING; }
			virtual void write_to (std::ostream& strm, int indent = 0) override;
		};
	
	
		class boolean: public value
		{
			bool v;
		
		public:
			boolean ();
			boolean (bool val);
		
			bool val () { return this->v; }
		
			// assignment operator overloads
			boolean& operator= (const boolean& other);
			boolean& operator= (bool v);
		
			virtual value_type type () override { return CFG_BOOLEAN; }
			virtual void write_to (std::ostream& strm, int indent = 0) override;
		};
		
		
		class separator: public value
		{
		public:
			separator () { }
			
			virtual value_type type () override { return CFG_SEPARATOR; }
			virtual void write_to (std::ostream& strm, int indent = 0) override;
		};
	
	
		class array: public value
		{
			std::vector<value *> vals;
		
		public:
			int word_wrap;
			int elems_per_line;
		
		public:
			array (int elems_per_line = 0, int word_wrap = 80);
			~array ();
		
		
		
			/* 
			 * Adds a new element to the end of the array.
			 */
			void add (value *val);
			void add_integer (long long val);
			void add_floating (double val);
			void add_string (const std::string& val);
			void add_boolean (bool val);
		
		
		
			value* at (int index);
			value*& operator[] (int index);
		
			long long get_integer (int index);
			double get_floating (int index);
			std::string get_string (int index);
		
			int size () { return this->vals.size (); }
			std::vector<value *>::iterator begin () { return this->vals.begin (); }
			std::vector<value *>::iterator end () { return this->vals.end (); }
		
		
		
			/* 
			 * Removes all elements from the array.
			 */
			void clear ();
		
		
			virtual value_type type () override { return CFG_ARRAY; }
			virtual void write_to (std::ostream& strm, int indent = 0) override;
			virtual void condensed_write_to (std::ostream& strm, int indent = 0) override;
		};
	
	
	
		struct setting
		{
			std::string name;
			value *val;
			std::string comment;
		
		//---
			setting (const std::string& n, value *v)
				: name (n), comment ()
				{ this->val = v; }
		};
	
		class group: public value
		{
			// we store the settings in a vector (and not, say, an unordered_map) so
			// that the order will be kept the same when iterating through the elements.
			std::vector<setting> settings;
			
			int sep_index;
		
		public:
			int lines_between;
		
		public:
			group (int lines_between = 0);
			~group ();
		
		
		
			// subscript operator overload
			value*& operator[] (const char *name);
			value*& operator[] (const std::string& name);
		
		
		
			/* 
			 * Removes the setting that has the specified name.
			 */
			void remove (const std::string& name);
		
			/* 
			 * Removes all settings from the group.
			 */
			void clear ();
		
		
		
			/* 
			 * Inserts a new setting into the group.
			 */
			void add (const std::string& name, value *val);
			void add_integer (const std::string& name, long long val);
			void add_floating (const std::string& name, double val);
			void add_string (const std::string& name, const std::string& val);
			void add_boolean (const std::string& name, bool val);
			void add_separator ();
			
			void attach_comment (const std::string& name, const std::string& comment);
		
		
		
			/* 
			 * Lookup:
			 */
			 
			value* find (const std::string& name);
			integer* find_integer (const std::string& name);
			floating* find_floating (const std::string& name);
			string* find_string (const std::string& name);
			boolean* find_boolean (const std::string& name);
			group* find_group (const std::string& name);
			array* find_array (const std::string& name);
		
			long long get_integer (const std::string& name);
			double get_floating (const std::string& name);
			std::string get_string (const std::string& name);
			bool get_boolean (const std::string& name);
			
			bool try_get_integer (const std::string& name, long long& out);
			bool try_get_floating (const std::string& name, double& out);
			bool try_get_string (const std::string& name, std::string& out);
			bool try_get_boolean (const std::string& name, bool& out);
			
			bool exists (const std::string& name, value_type typ = CFG_NONE);
			
		
			int size () { return this->settings.size (); }
			std::vector<setting>::iterator begin () { return this->settings.begin (); }
			std::vector<setting>::iterator end () { return this->settings.end (); }
		
		
		
			virtual value_type type () override { return CFG_GROUP; }
			virtual void write_to (std::ostream& strm, int indent = 0) override;
			virtual void condensed_write_to (std::ostream& strm, int indent = 0) override;
		
			static group* read_from (std::istream& strm);
			static group* read_from (const std::string& str);
		};
	}
}

