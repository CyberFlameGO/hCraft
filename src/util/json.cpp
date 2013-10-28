
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

#include "util/json.hpp"


namespace hCraft {
	
	namespace json {
		
		static void
		_write_string (std::ostream& strm, const std::string& str)
		{
			strm << '"';
			for (size_t i = 0; i < str.length (); ++i)
				switch (str[i])
					{
						case '"': strm << "\\\""; break;
						case '\\': strm << "\\\\"; break;
						case '\t': strm << "\\t"; break;
						case '\n': strm << "\\n"; break;
							
						default:
							strm << str[i];
							break;
					}
			
			strm << '"';
		}
		
		
//-----------------
		
		object::~object ()
		{
			for (auto& p : this->data)
				delete p.second;
		}
		
		
		
		/* 
		 * Searches the object for the name/value pair that has the specified name
		 * and returns the corresponding value if found, or null otherwise.
		 */
		value*
		object::find (const std::string& name)
		{
			auto itr = this->data.find (name);
			if (itr == this->data.end ())
				return nullptr;
			
			return itr->second;
		}
		
		/* 
		 * Inserts the specified name/value pair into the object.
		 * If a pair with the same name already exists, it will be replaced.
		 */
		void
		object::insert (const std::string& name, value *val)
		{
			auto itr = this->data.find (name);
			if (itr == this->data.end ())
				this->data[name] = val;
			else
				{
					if (itr->second != val)
						delete itr->second;
					itr->second = val;
				}
		}
		
		void
		object::insert_string (const std::string& name, const std::string& val)
		{
			this->insert (name, new string (val));
		}
		
		void
		object::insert_number (const std::string& name, double val)
		{
			this->insert (name, new number (val));
		}
		
		
		
		/* 
		 * Returns a reference to the specified name's corresponding value.
		 * NOTE: If it does not exist, it will be created.
		 */
		value*&
		object::operator[] (const std::string& name)
		{
			return this->data[name];
		}
		
		
		
		/* 
		 * Serializes the value into the specified character stream.
		 */
		void
		object::write (std::ostream& strm)
		{
			strm << '{';
			if (!this->data.empty ())
				{
					auto itr = this->data.begin ();
					_write_string (strm, itr->first);
					strm << ':';
					itr->second->write (strm);
					
					for (++itr; itr != this->data.end (); ++itr)
						{
							strm << ',';
							_write_string (strm, itr->first);
							strm << ':';
							itr->second->write (strm);
						}
				}
			strm << '}';
		}
		
		
		
//-----------------
		
		array::~array ()
		{
			for (value *v : this->data)
				delete v;
		}
		
		
		
		/* 
		 * Inserts the specifie value into the array.
		 */
		void
		array::add (value *val)
		{
			this->data.push_back (val);
		}
		
		void
		array::add_string (const std::string& val)
		{
			this->data.push_back (new string (val));
		}
		
		void
		array::add_number (double val)
		{
			this->data.push_back (new number (val));
		}
		
		
		/* 
		 * Returns the i'th element in the array.
		 */
		value*&
		array::get (int i)
		{
			return this->data[i];
		}
		
		/* 
		 * Returns the number of elements in the array.
		 */
		int
		array::size () const
		{
			return this->data.size ();
		}
		
		
		
		/* 
		 * Returns a reference to the i'th element in the array.
		 */
		value*&
		array::operator[] (int i)
		{
			return this->data[i];
		}
		
		
		
		/* 
		 * Serializes the value into the specified character stream.
		 */
		void
		array::write (std::ostream& strm)
		{
			strm << '[';
			if (!this->data.empty ())
				{
					auto itr = this->data.begin ();
					(*itr)->write (strm);
					
					for (++itr; itr != this->data.end (); ++itr)
						{
							strm << ',';
							(*itr)->write (strm);
						}
				}
			strm << ']';
		}
		
		
		
//-----------------
		
		/* 
		 * Serializes the value into the specified character stream.
		 */
		void
		string::write (std::ostream& strm)
		{
			_write_string (strm, this->val);
		}
		
		
		
//-----------------
		
		/* 
		 * Serializes the value into the specified character stream.
		 */
		void
		number::write (std::ostream& strm)
		{
			strm << this->val;
		}
	}
}

