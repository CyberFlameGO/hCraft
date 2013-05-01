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

#include "sql.hpp"
#include <sqlite3.h>
#include <chrono>
#include <thread>
#include <iostream> // DEBUG


namespace hCraft {
	
	namespace sql {
		
		void (*pass_static)(void *) = SQLITE_STATIC;
		void (*pass_transient)(void *) = SQLITE_TRANSIENT;
		
		
		
		/* 
		 * Constructs a new connection but does not associate it with any open
		 * database.
		 */
		connection::connection ()
		{
			this->_open = false;
			this->db = nullptr;
		}
	
		/* 
		 * Opens up a connection to the database file located at the path given
		 * by parameter @{db_name}.
		 * Throws sql_error on failure.
		 */
		connection::connection (const char *db_name)
		{
			this->_open = false;
			this->db = nullptr;
		
			this->open (db_name);
		}
		
		/* 
		 * Move constructor.
		 */
		connection::connection (connection&& conn)
		{
			this->db = conn.db;
			this->_open = conn._open;
			
			conn.db = nullptr;
			conn._open = false;
		}
	
		/* 
		 * Class destructor.
		 * Closes the underlying database if it is open.
		 */
		connection::~connection ()
		{
			this->close ();
		}
		
		
		
		/* 
		 * Attempts to open the database file located at path @{dn_name}.
		 * Throws sql_error on failure.
		 */
		void
		connection::open (const char *db_name)
		{
			if (this->_open)
				this->close ();
			
			int err;
			
			err = sqlite3_open (db_name, &this->db);
			if (err != SQLITE_OK)
				{
					std::string msg = sqlite3_errmsg (this->db);
					sqlite3_close (this->db);
					throw sql_error ("failed to open connection: " + msg);
				}
			
			this->_open = true;
		}
		
		/* 
		 * Closes the underlying database connection.
		 */
		void
		connection::close ()
		{
			if (!this->_open)
				return;
			
			sqlite3_close (this->db);
			
			this->_open = false;
			this->db = nullptr;
		}
		
		
		
		/* 
		 * Executes the specified SQL command(s) (seperated by semi-colons).
		 */
		
		void
		connection::execute (const char *cmd)
		{
			if (!this->_open)
				throw sql_error ("connection not open");
			if (!cmd) return;
			
			row r;
			
			const char *tail = cmd;
			while (*tail)
				{
					statement stmt (*this);
					stmt.prepare (tail, &tail);
					while (stmt.step (r))
						;
				}
		}
		
		void
		connection::execute (const std::string& cmd)
		{
			this->execute (cmd.c_str ());
		}
		
		
		
		/* 
		 * Creates and returns a new prepared statement from the given SQL query
		 * command.
		 */
		statement
		connection::query (const char *sql)
		{
			return statement (*this, sql);
		}
		
		statement
		connection::query (const std::string& sql)
		{
			return statement (*this, sql);
		}
		
		
		
	//----
		/* 
		 * Constructs an unprepared statement.
		 */
		statement::statement (connection& conn)
			: conn (conn)
		{
			this->prepared = false;
			this->col_count = 0;
		}
		
		/* 
		 * Constructs a new statement and prepares it with the given SQL command\
		 * query.
		 * Throws sql_error on failure.
		 */
		statement::statement (connection& conn, const char *query)
			: statement (conn)
		{
			this->prepare (query);
		}
		
		statement::statement (connection& conn, const std::string& query)
			: statement (conn, query.c_str ())
			{ }
		
		/* 
		 * Move constructor
		 */
		statement::statement (statement&& stmt)
			: conn (stmt.conn)
		{
			this->stmt = stmt.stmt;
			this->prepared = stmt.prepared;
			this->col_count = stmt.col_count;
			
			stmt.stmt = nullptr;
			stmt.prepared = false;
			stmt.col_count = 0;
		}
		
		/* 
		 * Class destructor.
		 * Frees all resources\memory used by the statement.
		 */
		statement::~statement ()
		{
			if (this->stmt)
				{
					this->finalize ();
				}
		}
		
		
		
		/* 
		 * Prepares the given SQL query\command to be executed.
		 * Throws sql_error on failure.
		 */
		
		void
		statement::prepare (const char *query, const char **tail)
		{
			if (this->prepared)
				this->finalize ();
			if (!this->conn.is_open ())
				throw sql_error ("cannot prepare statement on closed connection");
			
			int tries = 0;
			static const int max_tries = 10;
			bool success = false;
			
			do
				{
					int err;
					err = sqlite3_prepare_v2 (this->conn.handle (), query, -1, &this->stmt,
						tail);
					if (err != SQLITE_OK)
						{
							if (err == SQLITE_BUSY)
								{
									++ tries;
									std::this_thread::sleep_for (std::chrono::milliseconds (30));
								}
							else
								break;
						}
					else
						success = true;
				}
			while (!success && (tries < max_tries));
			if (!success)
				throw sql_error ("failed to prepare statement: "
					+ std::string (sqlite3_errmsg (this->conn.handle ())));
			
			this->col_count = sqlite3_column_count (this->stmt);
			this->prepared = true;
		}
		
		void
		statement::prepare (const std::string& query)
			{ this->prepare (query.c_str ()); }
		
		
		/* 
		 * Frees all memory\resources used by the constructed SQL query\command.
		 */
		void
		statement::finalize ()
		{
			if (!this->prepared)
				return;
			
			if (this->stmt)
				sqlite3_finalize (this->stmt);
			this->prepared = false;
		}
		
		
		
		/* 
		 * Evaluates the statement.
		 * Returns true if a row has been fetched and false otherwise.
		 */
		row&
		statement::step (row &r)
		{
			if (!this->prepared)
				throw sql_error ("cannot step through an un-prepared statement");
			
			r.items.clear ();
			if (!this->stmt) return r;
			
			int err;
			err = sqlite3_step (this->stmt);
			if (err == SQLITE_DONE) return r;
			if (err != SQLITE_ROW)
				throw sql_error ("failed to step through row: "
					+ std::string (sqlite3_errmsg (this->conn.handle ())));
			
			if (this->col_count)
				{
					r.items.reserve (this->col_count);
					for (int i = 0; i < this->col_count; ++i)
						{
							value val;
							
							switch (sqlite3_column_type (this->stmt, i))
								{
									case SQLITE_INTEGER:
										val._type = SQL_INTEGER;
										val._data.integer = sqlite3_column_int64 (this->stmt, i);
										break;
									case SQLITE_FLOAT:
										val._type = SQL_FLOAT;
										val._data.floating = sqlite3_column_double (this->stmt, i);
										break;
									case SQLITE_TEXT:
										val._type = SQL_TEXT;
										val._data.text = sqlite3_column_text (this->stmt, i);
										break;
									case SQLITE_BLOB:
										val._type = SQL_BLOB;
										val._data.blob = sqlite3_column_blob (this->stmt, i);
										break;
									default:
										val._type = SQL_NULL;
										val._data.blob = nullptr;
										break;
								}
							
							r.items.push_back (val);
						}
				}
			
			return r;
		}
		
		/* 
		 * Same as step (sql::row), but the row is created by the method.
		 * method.
		 */
		row
		statement::step ()
		{
			row r;
			this->step (r);
			return r;
		}
		
		/* 
		 * Resets the prepared statement to its initial state, allowing it to be
		 * stepped through again.
		 */
		void
		statement::reset ()
		{
			if (!this->prepared)
				throw sql_error ("statement is not prepared");
			if (!this->stmt) return;
			
			sqlite3_reset (this->stmt);
		}
		
		/* 
		 * Calls step () until no more rows are returned.
		 */
		void
		statement::execute ()
		{
			row r;
			while (this->step (r))
				;
		}
		
		
		
		/* 
		 * Parameter binding:
		 */
		
		void
		statement::bind (int index, int val)
		{
			int err;
			err = sqlite3_bind_int (this->stmt, index, val);
			if (err != SQLITE_OK)
				throw sql_error ("failed to bind parameter: "
					+ std::string (sqlite3_errmsg (this->conn.handle ())));
		}
		
		void
		statement::bind (const char *var, int val)
		{
			int index = sqlite3_bind_parameter_index (this->stmt, var);
			if (index == 0)
				throw sql_error ("invalid parameter: " + std::string (var));
			this->bind (index, val);
		}
		
		void
		statement::bind (int index, long long val)
		{
			int err;
			err = sqlite3_bind_int64 (this->stmt, index, val);
			if (err != SQLITE_OK)
				throw sql_error ("failed to bind parameter: "
					+ std::string (sqlite3_errmsg (this->conn.handle ())));
		}
		
		void
		statement::bind (const char *var, long long val)
		{
			int index = sqlite3_bind_parameter_index (this->stmt, var);
			if (index == 0)
				throw sql_error ("invalid parameter: " + std::string (var));
			this->bind (index, val);
		}
		
		void
		statement::bind (int index, double val)
		{
			int err;
			err = sqlite3_bind_double (this->stmt, index, val);
			if (err != SQLITE_OK)
				throw sql_error ("failed to bind parameter: "
					+ std::string (sqlite3_errmsg (this->conn.handle ())));
		}
		
		void
		statement::bind (const char *var, double val)
		{
			int index = sqlite3_bind_parameter_index (this->stmt, var);
			if (index == 0)
				throw sql_error ("invalid parameter: " + std::string (var));
			this->bind (index, val);
		}
		
		void
		statement::bind (int index, const char *val, void (*dctor)(void *))
		{
			int err;
			err = sqlite3_bind_text (this->stmt, index, val, -1, dctor);
			if (err != SQLITE_OK)
				throw sql_error ("failed to bind parameter: "
					+ std::string (sqlite3_errmsg (this->conn.handle ())));
		}
		
		void
		statement::bind (const char *var, const char *val,
			void (*dctor)(void *))
		{
			int index = sqlite3_bind_parameter_index (this->stmt, var);
			if (index == 0)
				throw sql_error ("invalid parameter: " + std::string (var));
			this->bind (index, val, dctor);
		}
		
		
		
	//----
		
		value::value (value_type t)
		{
			this->_type = t;
		}
		
		
		
	//----
		
		/* 
		 * Constructs a new row of {@column_count} columns.
		 */
		row::row ()
			{ }
		
		
		
	//----
		
		/* 
		 * Constructs a new connection pool, and creates @{min} connections
		 * on database @{db_name}.
		 */
		connection_pool::connection_pool (int count, const char *db_name)
			: db_name (db_name), lock ()
		{
			if (count <= 0) count = 1;
			this->conns.reserve (count);
			
			// create initial connections
			for (int i = 0; i < count; ++i)
				{
					this->conns.emplace_back (db_name);
					this->free_conns.push_back (&(this->conns.back ()));
				}
		}
		
		/* 
		 * Class destructor.
		 */
		connection_pool::~connection_pool ()
		{
			this->clear ();
		}
		
		
		
		/* 
		 * Pushes the specified connection back into the pool.
		 */
		void
		connection_pool::push (connection& conn)
		{
			std::lock_guard<std::mutex> guard {this->lock};
			this->free_conns.push_back (&conn);
			if (!this->free_conns.empty ())
				this->cv.notify_one ();
		}
		
		/* 
		 * Fetches an available connection from the pool.
		 * Will block if one is not available.
		 */
		connection&
		connection_pool::pop ()
		{
			std::unique_lock<std::mutex> guard {this->lock};
			std::vector<connection *>& free_list = this->free_conns;
			if (free_list.empty ())
				{
					this->cv.wait (guard, [&free_list] () { return !free_list.empty (); });
				}
			
			connection *conn = free_list.front ();
			free_list.pop_back ();
			return *conn;
		}
		
		/* 
		 * Removes all connection objects from this pool.
		 */
		void
		connection_pool::clear ()
		{
			std::lock_guard<std::mutex> guard {this->lock};
			this->free_conns.clear ();
			this->conns.clear ();
		}
	
	
	
	//----
		
		/* 
		 * Begins a transaction on the given connection.
		 */
		transaction::transaction (connection& conn)
			: conn (conn)
		{
			this->commited = false;
			this->rolled_back = false;
			conn.execute ("BEGIN TRANSACTION");
		}
		
		/* 
		 * Completely rolls-back the underlying transaction if it has not already
		 * been commited.
		 */
		transaction::~transaction ()
		{
			if (!this->commited && !this->rolled_back)
				this->rollback ();
		}
		
		
		void
		transaction::commit ()
		{
			if (!this->commited && !this->rolled_back)
				{
					conn.execute ("COMMIT TRANSACTION");
					this->commited = true;
				}
		}
		
		void
		transaction::rollback ()
		{
			if (!this->commited && !this->rolled_back)
				{
					conn.execute ("ROLLBACK TRANSACTION");
					this->rolled_back = true;
				}
		}
	}
}

