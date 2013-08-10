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

#include "config.hpp"
#include <sstream>
#include <cctype>
#include <memory>


namespace hCraft {
	namespace cfg {
	
		namespace {
		
			static void
			indent_with_spaces (std::ostream& os, int s)
			{
				for (int i = 0; i < s; ++i)
					os.put (' ');
			}
		}
	
	
	//----
		/* 
		 * cfg::string:
		 */
	
		string::string ()
			: str ()
			{ }
		string::string (const char *str)
			: str (str)
			{ }
		string::string (const std::string& str)
			: str (str)
			{ }
	
	
		// assignment operator overloads
	
		string&
		string::operator= (const string& other)
		{
			this->str.assign (other.str);
			return *this;
		}
	
		string&
		string::operator= (const char *str)
		{
			this->str.assign (str);
			return *this;
		}
	
		string&
		string::operator= (const std::string& str)
		{
			this->str.assign (str);
			return *this;
		}
	
	
	
		void
		string::write_to (std::ostream& strm, int indent)
		{
			strm.put ('"');
		
			for (size_t i = 0; i < this->str.size (); ++i)
				{
					if (this->str[i] == '"')
						{
							strm << "\\\""; 
						}
					else if (this->str[i] == '\\')
						{
							strm << "\\\\";
						}
					else
						strm.put (this->str[i]);
				}
		
			strm.put ('"');
		}
	
	//----



	//----
		/* 
		 * cfg::integer:
		 */
	
		integer::integer ()
		{
			this->num = 0;
		}
	
		integer::integer (long long num)
		{
			this->num = num;
		}
	
	
		// assignment operator overloads
		integer& 
		integer::operator= (const integer& other)
		{
			this->num = other.num;
			return *this;
		}
	
		integer&
		integer::operator= (long long num)
		{
			this->num = num;
			return *this;
		}
	
	
	
		void
		integer::write_to (std::ostream& strm, int indent)
		{
			strm << this->num;
		}

	//----



	//----
		/* 
		 * cfg::floating:
		 */
	
		floating::floating ()
		{
			this->num = 0.0;
		}
	
		floating::floating (double num)
		{
			this->num = num;
		}
	
	
		// assignment operator overloads
		floating& 
		floating::operator= (const floating& other)
		{
			this->num = other.num;
			return *this;
		}
	
		floating&
		floating::operator= (double num)
		{
			this->num = num;
			return *this;
		}
	
	
	
		void
		floating::write_to (std::ostream& strm, int indent)
		{
			strm << this->num;
		}

	//----



	//----
		/* 
		 * cfg::boolean:
		 */
	
		boolean::boolean ()
		{
			this->v = 0;
		}
	
		boolean::boolean (bool v)
		{
			this->v = v;
		}
	
	
		// assignment operator overloads
		boolean& 
		boolean::operator= (const boolean& other)
		{
			this->v = other.v;
			return *this;
		}
	
		boolean&
		boolean::operator= (bool v)
		{
			this->v = v;
			return *this;
		}
	
	
	
		void
		boolean::write_to (std::ostream& strm, int indent)
		{
			strm << (this->v ? "true" : "false");
		}
	
	//----
	
	
	
	//----
		/* 
		 * cfg::separator:
		 */
		
		void
		separator::write_to (std::ostream& strm, int indent)
		{
			
		}
		
	//----
	


	//----
		/* 
		 * cfg::array:
		 */
		 
		array::array (int elems_per_line, int word_wrap)
		{
			this->elems_per_line = elems_per_line;
			this->word_wrap = word_wrap;
		}
	
		array::~array ()
		{
			this->clear ();
		}
	
	
	
		/* 
		 * Adds a new element to the end of the array.
		 */
		void
		array::add (value *val)
		{
			if (this->vals.size () > 0)
				{
					if (val->type () != this->vals[0]->type ())
						throw cfg_type_error ("type mismatch in array");
				}
			this->vals.push_back (val);
		}
	
		void
		array::add_integer (long long val)
		{
			this->add (new cfg::integer (val));
		}
	
		void
		array::add_floating (double val)
		{
			this->add (new cfg::floating (val));
		}
	
		void
		array::add_string (const std::string& val)
		{
			this->add (new cfg::string (val));
		}
	
		void
		array::add_boolean (bool val)
		{
			this->add (new cfg::boolean (val));
		}
	
	
	
		value*
		array::at (int index)
		{
			if (index >= (int)this->vals.size ())
				return nullptr;
			return this->vals[index];
		}
	
		value*&
		array::operator[] (int index)
		{
			if (index >= (int)this->vals.size ())
				throw cfg_find_error ("index out of bounds in array");
			return this->vals[index];
		}
	
	
	
		long long
		array::get_integer (int index)
		{
			value *v = this->at (index);
			if (!v)
				throw cfg_find_error ("integer not found");
			if (v->type () == CFG_INTEGER)
				return ((integer *)v)->val ();
			else if (v->type () == CFG_FLOATING)
				return (long long)((floating *)v)->val ();
			throw cfg_type_error ("expected an integer");
		}
	
		double
		array::get_floating (int index)
		{
			value *v = this->at (index);
			if (!v)
				throw cfg_find_error ("float not found");
			if (v->type () == CFG_INTEGER)
				return (double)((integer *)v)->val ();
			else if (v->type () == CFG_FLOATING)
				return ((floating *)v)->val ();
			throw cfg_type_error ("expected a float");
		}
	
		std::string
		array::get_string (int index)
		{
			value *v = this->at (index);
			if (!v)
				throw cfg_find_error ("string not found");
			if (v->type () == CFG_STRING)
				return ((string *)v)->val ();
			throw cfg_type_error ("expected a string");
		}
	
	
	
		/* 
		 * Removes all elements from the array.
		 */
		void
		array::clear ()
		{
			for (value *val : this->vals)
				delete val;
			this->vals.clear ();
		}
	
	
	
		void
		array::condensed_write_to (std::ostream& strm, int indent)
		{
			if (this->vals.empty ())
				{
					strm << "[]";
				}
			else
				{
					strm << "[ ";
				
					for (auto itr = this->vals.begin (); itr != this->vals.end (); )
						{
							(*itr)->condensed_write_to (strm, indent + 2);
							++ itr;
							if (itr != this->vals.end ())
								strm << ", ";
						}
				
					strm << " ]";
				}
		}
	
		void
		array::write_to (std::ostream& strm, int indent)
		{
			if (this->vals.empty ())
				{
					strm << "[]";
					return;
				}
		
			strm << "[\n";

			std::ostringstream ss;
			indent_with_spaces (ss, indent + 2);
			std::string tmp;
		
			if (this->word_wrap == 0)
				indent_with_spaces (strm, indent + 2);
		
			int count = 0;
			for (auto itr = this->vals.begin (); itr != this->vals.end (); )
				{
					++ count;
				
					if (this->word_wrap > 0)
						{
							tmp.assign (ss.str ());
				
							(*itr)->write_to (ss, indent + 2);
							ss << ", ";
							if (((int)ss.tellp () > this->word_wrap) || ((this->elems_per_line > 0) && (count % (this->elems_per_line + 1) == 0)))
								{
									strm << tmp << "\n";
									ss.str (std::string ());
									indent_with_spaces (ss, indent + 2);
								}
							else
								{
									++ itr;
								}
						}
					else
						{
							(*itr)->write_to (strm, indent + 2);
							strm << ", ";
						
							++ itr;
							if ((this->elems_per_line > 0) && (count % this->elems_per_line == 0))
								{
									if (itr != this->vals.end ())
										strm << "\n";
									indent_with_spaces (strm, indent + 2);
								}
						}
				}
			
			if ((int)ss.tellp () > (indent + 2))
				strm << ss.str ();
			
			strm << "\n";
			indent_with_spaces (strm, indent);
			strm << "]";
		}
	
	//----



	//----
		/* 
		 * cfg::group:
		 */
	
		group::group (int lines_between)
		{
			this->lines_between = lines_between;
			this->sep_index = 0;
		}
	
		group::~group ()
		{
			this->clear ();
		}
	
	
	
		// subscript operator overload
	
		value*& 
		group::operator[] (const char *name)
		{
			for (setting& s : this->settings)
				if (s.name.compare (name) == 0)
					return s.val;
			throw cfg_find_error ("value not found");
		}
	
		value*&
		group::operator[] (const std::string& name)
		{
			for (setting& s : this->settings)
				if (s.name.compare (name) == 0)
					return s.val;
			throw cfg_find_error ("value not found");
		}
	
	
	
		/* 
		 * Inserts a new setting into the group.
		 */
		void
		group::add (const std::string& name, value *val)
		{
			this->settings.emplace_back (name, val);
		}
	
		void
		group::add_integer (const std::string& name, long long val)
		{
			this->add (name, new cfg::integer (val));
		}
	
		void
		group::add_floating (const std::string& name, double val)
		{
			this->add (name, new cfg::floating (val));
		}
	
		void
		group::add_string (const std::string& name, const std::string& val)
		{
			this->add (name, new cfg::string (val));
		}
	
		void
		group::add_boolean (const std::string& name, bool val)
		{
			this->add (name, new cfg::boolean (val));
		}
		
		void
		group::add_separator ()
		{
			std::ostringstream ss;
			ss << "sep$" << (this->sep_index++);
			this->settings.emplace_back (ss.str (), new cfg::separator ());
		}
		
		
		void
		group::attach_comment (const std::string& name, const std::string& comment)
		{
			for (setting& s : this->settings)
				if (s.name.compare (name) == 0)
					s.comment.assign (comment);
		}
	
	
	
		value*
		group::find (const std::string& name)
		{
			for (setting& s : this->settings)
				if (s.name.compare (name) == 0)
					return s.val;
			return nullptr;
		}
	
		integer*
		group::find_integer (const std::string& name)
		{
			value *val = this->find (name);
			if (!val) return nullptr;
			if (val->type () != CFG_INTEGER)
				throw cfg_type_error ("expected an integer");
			return (integer *)val;
		}
	
		floating*
		group::find_floating (const std::string& name)
		{
			value *val = this->find (name);
			if (!val) return nullptr;
			if (val->type () != CFG_FLOATING)
				throw cfg_type_error ("expected a float");
			return (floating *)val;
		}
	
		string*
		group::find_string (const std::string& name)
		{
			value *val = this->find (name);
			if (!val) return nullptr;
			if (val->type () != CFG_STRING)
				throw cfg_type_error ("expected a string");
			return (string *)val;
		}
		
		boolean*
		group::find_boolean (const std::string& name)
		{
			value *val = this->find (name);
			if (!val) return nullptr;
			if (val->type () != CFG_BOOLEAN)
				throw cfg_type_error ("expected a boolean");
			return (boolean *)val;
		}
		
		group*
		group::find_group (const std::string& name)
		{
			value *val = this->find (name);
			if (!val) return nullptr;
			if (val->type () != CFG_GROUP)
				throw cfg_type_error ("expected a group");
			return (group *)val;
		}
		
		array*
		group::find_array (const std::string& name)
		{
			value *val = this->find (name);
			if (!val) return nullptr;
			if (val->type () != CFG_ARRAY)
				throw cfg_type_error ("expected an array");
			return (array *)val;
		}
	
	
		long long
		group::get_integer (const std::string& name)
		{
			integer *val = this->find_integer (name);
			if (!val)
				throw cfg_find_error ("integer not found");
			return val->val ();
		}
	
		double
		group::get_floating (const std::string& name)
		{
			floating *val = this->find_floating (name);
			if (!val)
				throw cfg_find_error ("float not found");
			return val->val ();
		}
	
		std::string
		group::get_string (const std::string& name)
		{
			string *val = this->find_string (name);
			if (!val)
				throw cfg_find_error ("string not found");
			return val->val ();
		}
		
		bool
		group::get_boolean (const std::string& name)
		{
			boolean *val = this->find_boolean (name);
			if (!val)
				throw cfg_find_error ("boolean not found");
			return val->val ();
		}
		
		
		
		bool
		group::try_get_integer (const std::string& name, long long& out)
		{
			value *val = this->find (name);
			if (!val || (val->type () != CFG_INTEGER))
				return false;
			integer *cv = (integer *)val;
			out = cv->val ();
			return true;
		}
		
		bool
		group::try_get_floating (const std::string& name, double& out)
		{
			value *val = this->find (name);
			if (!val || (val->type () != CFG_FLOATING))
				return false;
			floating *cv = (floating *)val;
			out = cv->val ();
			return true;
		}
		
		bool
		group::try_get_string (const std::string& name, std::string& out)
		{
			value *val = this->find (name);
			if (!val || (val->type () != CFG_STRING))
				return false;
			string *cv = (string *)val;
			out.assign (cv->val ());
			return true;
		}
		
		bool
		group::try_get_boolean (const std::string& name, bool& out)
		{
			value *val = this->find (name);
			if (!val || (val->type () != CFG_BOOLEAN))
				return false;
			boolean *cv = (boolean *)val;
			out = cv->val ();
			return true;
		}
		
		
		bool
		group::exists (const std::string& name, value_type typ)
		{
			value *val = this->find (name);
			if (!val)
				return false;
			if ((typ != CFG_NONE) && (val->type () != typ))
				return false;
			return true;
		}
	
	
	
		/* 
		 * Removes the setting that has the specified name.
		 */
		void
		group::remove (const std::string& name)
		{
			for (auto itr = this->settings.begin (); itr != this->settings.end (); ++itr)
				if (itr->name.compare (name) == 0)
					{
						delete itr->val;
						this->settings.erase (itr);
						break;
					}
		}

		/* 
		 * Removes all settings from the group.
		 */
		void
		group::clear ()
		{
			for (auto itr = this->settings.begin (); itr != this->settings.end (); ++itr)
				delete itr->val;
			this->settings.clear ();
		}
	
	
	
		void
		group::condensed_write_to (std::ostream& strm, int indent)
		{
			if (this->settings.empty ())
				{
					strm << "{}";
				}
			else
				{
					strm << "{ ";
					
					for (auto itr = this->settings.begin (); itr != this->settings.end (); ++itr)
						{
							strm << itr->name << ": ";
							itr->val->condensed_write_to (strm, indent + 2);
							strm << "; ";
						}
				
					strm << "}";
				}
		}
	
		void
		group::write_to (std::ostream& strm, int indent)
		{
			if (this->settings.empty ())
				{
					strm << "{}";
				}
			else
				{
					strm << "{\n";
					bool is_first = true;
					for (auto itr = this->settings.begin (); itr != this->settings.end (); ++itr)
						{
							if (!is_first)
								{
									for (int i = 0; i < this->lines_between; ++i)
										strm << "\n";
								}
								
							// comment?
							if (!itr->comment.empty ())
								{
									std::string com = itr->comment;
									
									while (!com.empty ())
										{
											// remove whitespace
											{
												size_t i;
												for (i = 0; i < com.size (); ++i)
													if (com[i] != ' ')
														break;
												com.erase (0, i);
												if (com.empty ())
													break;
											}
											
											indent_with_spaces (strm, indent + 2);
											strm << "// ";
											
											int space = 80 - (indent + 5 + 1);
											if (space >= (int)com.size ())
												{
													strm << com;
													com.clear ();
												}
											else
												{
													strm << com.substr (0, space);
													com.erase (0, space);
													if (com[0] != ' ')
														strm << "-";
												}
											strm << "\n";
										}
								}
						
							indent_with_spaces (strm, indent + 2);
							if (itr->val->type () == CFG_SEPARATOR)
								{
									strm << "\n";
									continue;
								}
							
							strm << itr->name << ": ";
						
							if (itr->val->type () == CFG_ARRAY)
								{
									// try to fit the array in one line if possible.
									std::ostringstream ss;
									itr->val->condensed_write_to (ss, indent + 2);
									if (((int)((indent + itr->name.size () + 4) + ss.tellp ()) + 1) > 80)
										{
											// nope, too long
											itr->val->write_to (strm, indent + 2);
										}
									else
										strm << ss.str ();
								}
							else
								itr->val->write_to (strm, indent + 2);
						
							strm << ";\n";
						
							is_first = false;
						}
				
					indent_with_spaces (strm, indent);
					strm << "}";
				}
		}
	
	//----



	//----
		/* 
		 * READING
		 */
	
		namespace {
		
			static void
			_expect (std::istream& strm, int ch, const char *err = "encountered unexpected character")
			{
				if (strm.get () != ch)
					throw cfg_error (err);
			}
		
			static void
			_skip_whitespace (std::istream& strm)
			{
				int c;
				for (;;)
					{
						c = strm.peek ();
						if (c == std::istream::traits_type::eof ())
							break;
						if (!std::isspace (c))
							{
								// maybe it's a comment
								if (c == '/')
									{
										strm.get ();
										if (strm.peek () != '/')
											{ strm.unget (); break; }
										for (;;)
											{
												c = strm.get ();
												if (c == '\n' || c == std::istream::traits_type::eof ())
													break;
											}
									}
								else
									break;
							}
						strm.get ();
					}
			}
		
		
			static bool
			_is_name_char (char c, bool initial = false)
			{
				return ((initial ? std::isalpha (c) : std::isalnum (c)) || (c == '-') || (c == '_'));
			}
		
			static std::string
			_read_name (std::istream& strm)
			{
				int c;
				std::string str;
			
				bool initial = true;
				for (;;)
					{
						c = strm.peek ();
						if (_is_name_char (c, initial))
							{
								strm.get ();
								str.push_back (c);
								initial = false;
							}
						else
							break;
					}
			
				if (str.empty ())
					throw cfg_error ("expected a name");
				return str;
			}
		
		
			static value* _read_value (std::istream& strm); // forward dec
		
			static value*
			_read_number (std::istream& strm)
			{
				std::stringstream ss;
				int c;
			
				int dots = 0;
				for (;;)
					{
						c = strm.peek ();
						if (std::isdigit (c))
							{
								strm.get ();
								ss << (char)c;
							}
						else if (c == '.')
							{
								if (dots == 0)
									{
										strm.get ();
										ss << '.';
										++ dots;
									}
								else
									throw cfg_error ("invalid number (more than one decimal point)");
							}
						else
							break;
					}
			
				if ((int)ss.tellp () == 0)
					throw cfg_error ("expected a number");
			
				if (dots == 0)
					{
						// integer
						long long val;
						ss >> val;
						return new cfg::integer (val);
					}
			
				// floating
				double val;
				ss >> val;
				return new cfg::floating (val);
			}
		
			static string*
			_read_string (std::istream& strm)
			{
				_expect (strm, '"');
			
				std::string str;
				int c;
				for (;;)
					{
						c = strm.get ();
						if (c == std::istream::traits_type::eof ())
							throw cfg_error ("unexpected end of string");
						if (c == '"')
							break;
					
						if (c == '\\')
							{
								c = strm.get ();
								if (c == std::istream::traits_type::eof ())
									throw cfg_error ("unexpected end of string");
							
								switch (c)
									{
										case '\\': str.push_back ('\\'); break;
										case '"': str.push_back ('"'); break;
									
										default:
											throw cfg_error ("unknown escape sequence in string");
									}
							}
						else
							str.push_back (c);
					}
			
				return new string (str);
			}
		
			static group*
			_read_group (std::istream& strm)
			{
				std::unique_ptr<group> grp {new group ()};
			
				_skip_whitespace (strm);
				_expect (strm, '{');
			
				int c;
				for (;;)
					{
						_skip_whitespace (strm);
						c = strm.peek ();
						if (c == std::istream::traits_type::eof ())
							throw cfg_error ("unexpected eof");
						if (c == '}')
							{
								strm.get ();
								break;
							}
					
						std::string name = _read_name (strm);
						_skip_whitespace (strm);
						_expect (strm, ':', "expected :");
						_skip_whitespace (strm);
					
						value *val = _read_value (strm);
						if (!val)
							throw cfg_error ("expected a value after :");
					
						grp->add (name, val);
					
						_skip_whitespace (strm);
						_expect (strm, ';', "expected ;");
					}
			
				return grp.release ();
			}
		
			static array*
			_read_array (std::istream& strm)
			{
				std::unique_ptr<array> arr {new array ()};
			
				_skip_whitespace (strm);
				_expect (strm, '[');
			
				int c;
				for (;;)
					{
						_skip_whitespace (strm);
						c = strm.peek ();
						if (c == std::istream::traits_type::eof ())
							throw cfg_error ("unexpected eof");
						if (c == ']')
							{
								strm.get ();
								break;
							}
					
						value *val = _read_value (strm);
						if (!val)
							throw cfg_error ("expected a value inside array");
					
						arr->add (val);
					
						_skip_whitespace (strm);
						c = strm.peek ();
						if (c == ',')
							{
								strm.get ();
							}
						else if (c == ']')
							continue; // handled at the top of the loop
						else
							throw cfg_error ("expected , inside array");
					}
			
				return arr.release ();
			}
		
			static boolean*
			_read_boolean (std::istream& strm)
			{
				int c = strm.peek ();
				if (c == 't')
					{
						strm.get ();
						_expect (strm, 'r', "expected a boolean");
						_expect (strm, 'u', "expected a boolean");
						_expect (strm, 'e', "expected a boolean");
					
						return new boolean (true);
					}
				else if (c == 'f')
					{
						strm.get ();
						_expect (strm, 'a', "expected a boolean");
						_expect (strm, 'l', "expected a boolean");
						_expect (strm, 's', "expected a boolean");
						_expect (strm, 'e', "expected a boolean");
					
						return new boolean (false);
					}
			
				throw cfg_error ("expected a boolean");
			}
		
			static value*
			_read_value (std::istream& strm)
			{
				int c = strm.peek ();
				if (c == std::istream::traits_type::eof ())
					throw cfg_error ("expected a value");
			
				if (std::isdigit (c) || (c == '.'))
					return _read_number (strm);
			
				if (c == '"')
					return _read_string (strm);
				
				if (c == '{')
					return _read_group (strm);
			
				if (c == '[')
					return _read_array (strm);
			
				if (c == 't' || c == 'f')
					return _read_boolean (strm);
			
				throw cfg_error ("unknown type of value");
			}
		}
	
	
	
		group*
		group::read_from (std::istream& strm)
		{
			group *grp = _read_group (strm);
		
			// make sure there's nothing after this
			_skip_whitespace (strm);
			int c = strm.peek ();
			if (c != std::istream::traits_type::eof ())
				{
					delete grp;
					throw cfg_error ("unexpected characters after group");
				}
		
			return grp;
		}
	
		group*
		group::read_from (const std::string& str)
		{
			std::istringstream ss {str};
			return group::read_from (ss);
		}
	
	//----
	}
}

