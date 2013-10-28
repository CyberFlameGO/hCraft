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

#ifndef _hCraft__JSON_H_
#define _hCraft__JSON_H_

#include <unordered_map>
#include <string>
#include <ostream>
#include <vector>


namespace hCraft {
	
	namespace json {
		
		enum value_type
		{
			JSON_OBJECT,
			JSON_STRING,
			JSON_NUMBER,
			JSON_ARRAY,
			JSON_BOOL,
			JSON_NULL,
		};
		
		class value
		{
		public:
			virtual ~value () { }
			virtual value_type type () const = 0;
			
			
			/* 
			 * Serializes the value into the specified character stream.
			 */
			virtual void write (std::ostream& strm) = 0;
		};
		
		
		
		/* 
		 * Unordered set of name/value pairs.
		 *   { "name1" : value1, "name2" : value2, ... }
		 */
		class object: public value
		{
			std::unordered_map<std::string, value *> data;
			
		public:
			virtual value_type type () const override { return JSON_OBJECT; }
			
		public:
			object () { }
			~object ();
			
			
			
			/* 
			 * Searches the object for the name/value pair that has the specified name
			 * and returns the corresponding value if found, or null otherwise.
			 */
			value* find (const std::string& name);
			
			/* 
			 * Inserts the specified name/value pair into the object.
			 * If a pair with the same name already exists, it will be replaced.
			 */
			void insert (const std::string& name, value *val);
			void insert_string (const std::string& name, const std::string& val);
			void insert_number (const std::string& name, double val);
			
			
			
			/* 
			 * Returns a reference to the specified name's corresponding value.
			 * NOTE: If it does not exist, it will be created.
			 */
			value*& operator[] (const std::string& name);
			
			
			
			/* 
			 * Serializes the value into the specified character stream.
			 */
			virtual void write (std::ostream& strm) override;
		};
		
		
		
		/* 
		 * An ordered collection of values.
		 */
		class array: public value
		{
			std::vector<value *> data;
			
		public:
			virtual value_type type () const override { return JSON_ARRAY; }
			
		public:
			array () { }
			~array ();
			
			
			
			/* 
			 * Inserts the specifie value into the array.
			 */
			void add (value *val);
			void add_string (const std::string& val);
			void add_number (double val);
			
			/* 
			 * Returns the i'th element in the array.
			 */
			value*& get (int i);
			
			/* 
			 * Returns the number of elements in the array.
			 */
			int size () const;
			
			
			
			/* 
			 * Returns a reference to the i'th element in the array.
			 */
			value*& operator[] (int i);
			
			
			
			/* 
			 * Serializes the value into the specified character stream.
			 */
			virtual void write (std::ostream& strm) override;
		};
		
		
		
		/* 
		 * Double quoted string.
		 */
		class string: public value
		{
			std::string val;
			
		public:
			inline const std::string& get () const { return this->val; }
			virtual value_type type () const override { return JSON_STRING; }
			
		public:
			string (const std::string& str)
				: val (str)
				{ }
			
			
			inline void
			set (const std::string& str)
				{ this->val = str; }
			
			
			
			/* 
			 * Serializes the value into the specified character stream.
			 */
			virtual void write (std::ostream& strm) override;
		};
		
		
		
		/* 
		 * Double-precision floating point number.
		 */
		class number: public value
		{
			double val;
			
		public:
			inline double get () const { return this->val; }
			virtual value_type type () const override { return JSON_NUMBER; }
			
		public:
			number (double val)
				{ this->val = val; }
			
			
			inline void
			set (double val)
				{ this->val = val; }
			
			
			
			/* 
			 * Serializes the value into the specified character stream.
			 */
			virtual void write (std::ostream& strm) override;
		};
	}
}

#endif

