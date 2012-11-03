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

#ifndef _hCraft__SQL_H_
#define _hCraft__SQL_H_

#include <stdexcept>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>


// forward decs:
struct sqlite3;
struct sqlite3_stmt;

namespace hCraft {
	
	/* 
	 * Base class for all errors thrown from the methods declared in this file.
	 */
	class sql_error: std::runtime_error
	{
	public:
		sql_error (const char *str)
			: std::runtime_error (str)
			{ }
		sql_error (const std::string& str)
			: std::runtime_error (str)
			{ }
	};
	
	
	/* 
	 * Classes that wrap around sqlite3 objects and provide an cleaner and easier
	 * to use interface (RAII, etc...)
	 */
	namespace sql {
		
		class connection;
		class connection_pool;
		
		extern void (*pass_static)(void *);
		extern void (*pass_transient)(void *);
		
		
		enum value_type
		{
			SQL_NULL,
			SQL_INTEGER,
			SQL_FLOAT,
			SQL_TEXT,
			SQL_BLOB,
		};
		
		/* 
		 * A value type that can be fetched from returned rows.
		 */
		class value
		{
			friend class statement;
			
			value_type _type;
			union
				{
					const void *blob;
					double floating;
					long long integer;
					const unsigned char *text;
				} _data;
				
		public:
			inline value_type type () { return this->_type; }
			
		public:
			value (value_type t = SQL_NULL);
			
			long long as_int ()
				{ return this->_data.integer; }
			
			double as_double ()
				{ return this->_data.floating; }
			
			const char* as_cstr ()
				{ return (const char *)this->_data.text; }
			
			std::string as_str ()
				{ return std::string (this->as_cstr ()); }
			
			const void* as_blob ()
				{ return this->_data.blob; }
		};
		
		
		/* 
		 * A table row that is filled by stepping through a prepared statement.
		 */
		class row
		{
			friend class statement;
			std::vector<value> items;
			
		public:
			inline int size () { return this->items.size (); }
			inline bool empty () { return this->items.empty (); }
		
		public:
			/* 
			 * Constructs a new empty row.
			 */
			row ();
			
			
			/* 
			 * Returns the value of the column at the given index.
			 */
			value at (unsigned int index) { return items.at (index); }
			
			
			/* 
			 * Conversion to boolean values.
			 * Returns true if this row has columns in it.
			 */
			operator bool () const
				{ return !items.empty (); }
		};
		
		
		/* 
		 * Represents an executable SQL statement.
		 */
		class statement
		{
			connection& conn;
			sqlite3_stmt *stmt;
			bool prepared;
			
			int col_count;
			
		public:
			inline bool is_prepared () { return this->prepared; }
			inline int  column_count () { return this->col_count; }
			
		public:
			/* 
			 * Constructs an unprepared statement.
			 */
			statement (connection& conn);
			
			/* 
			 * Constructs a new statement and prepares it with the given SQL command\
			 * query.
			 * Throws sql_error on failure.
			 */
			statement (connection& conn, const char *query);
			statement (connection& conn, const std::string& query);
			
			/* 
			 * Class destructor.
			 * Frees all resources\memory used by the statement.
			 */
			~statement ();
			
			
			
			/* 
			 * Prepares the given SQL query\command to be executed.
			 * Throws sql_error on failure.
			 */
			void prepare (const char *query, const char **tail = nullptr);
			void prepare (const std::string& query);
			
			/* 
			 * Frees all memory\resources used by the constructed SQL query\command.
			 */
			void finalize ();
			
			
			
			/* 
			 * Parameter binding:
			 */
			
			void bind (int index, int val);
			void bind (const char *var, int val);
			void bind (int index, double val);
			void bind (const char *var, double val);
			void bind (int index, const char *val, void (*dctor)(void *) = nullptr);
			void bind (const char *var, const char *val,
				void (*dctor)(void *) = nullptr);
			
			
			
			/* 
			 * Evaluates the statement.
			 */
			row& step (row &r);
			
			/* 
			 * Same as step (sql::row), but the row is created by the method.
			 * method.
			 */
			row step ();
			
			/* 
			 * Resets the prepared statement to its initial state, allowing it to be
			 * stepped through again.
			 */
			void reset ();
			
			/* 
			 * Calls step () until no more rows are returned.
			 */
			void execute ();
		};
		
		
		
		/* 
		 * A connection to a database object.
		 */
		class connection
		{
			sqlite3 *db;
			bool _open;
		
		public:
			inline bool is_open () { return this->_open; }
			inline sqlite3* handle () { return this->db; }
			
		public:
			/* 
			 * Constructs a new connection but does not associate it with any open
			 * database.
			 */
			connection ();
			
			/* 
			 * Opens up a connection to the database file located at the path given
			 * by parameter @{db_name}.
			 * Throws sql_error on failure.
			 */
			connection (const char *db_name);
			
			/* 
			 * Class destructor.
			 * Closes the underlying database if it is open.
			 */
			~connection ();
			
			// connections are not copyable.
			connection (const connection& conn) = delete;
			
			/* 
			 * Move constructor.
			 */
			connection (connection&& conn);
			
			
			
			/* 
			 * Attempts to open the database file located at path @{dn_name}.
			 * Throws sql_error on failure.
			 */
			void open (const char *db_name);
			
			/* 
			 * Closes the underlying database connection.
			 */
			void close ();
			
			
			
			/* 
			 * Executes the specified SQL command(s) (seperated by semi-colons).
			 */
			void execute (const char *cmd);
			void execute (const std::string& cmd);
			
			/* 
			 * Creates and returns a new prepared statement from the given SQL query
			 * command.
			 */
			statement query (const char *sql);
			statement query (const std::string& sql);
		};
		
		
		
		/* 
		 * A thread-safe pool of reusable connection objects.
		 */
		class connection_pool
		{
			std::vector<connection> conns;
			std::vector<connection *> free_conns;
			std::string db_name;
			
			std::mutex lock;
			std::condition_variable cv;
			
			int min_conns;
			int max_conns;
			
		public:
			/* 
			 * Constructs a new connection pool, and creates @{min} connections
			 * on database @{db_name}.
			 */
			connection_pool (int min, int max, const char *db_name);
			
			/* 
			 * Class destructor.
			 */
			~connection_pool ();
			
			
			
			/* 
			 * Pushes the specified connection back into the pool.
			 */
			void push (connection& conn);
			
			/* 
			 * Fetches an available connection from the pool.
			 * Will block if one is not available.
			 */
			connection& pop ();
			
			/* 
			 * Removes all connection objects from this pool.
			 */
			void clear ();
		};
	}
}

#endif

