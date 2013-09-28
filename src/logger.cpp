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

#include "logger.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cctype>
#include <fstream>
#include <ctime>
#include <sstream>


namespace hCraft {
	
	/* 
	 * Class constructor.
	 */
	logger::logger_buf::logger_buf (logger& log)
		: log (log)
		{ }
	
	
	/* 
	 * Locks the buffer's underlying mutex and outputs whatever's in the
	 * internal string buffer out to the standard output stream.
	 */
	int
	logger::logger_buf::sync ()
	{
		std::lock_guard<std::mutex> guard {this->log.lock};
		
		struct winsize w;
		ioctl (STDOUT_FILENO, TIOCGWINSZ, &w);
		
		std::string output = this->str ();
		const char *str = output.c_str ();
		int c, col = 0, max_col = w.ws_col - 1;
		
		const char *lo = std::strrchr (str, '|');
		int col_start = 0;
		if (lo)
			col_start = lo - str;
		
		while ((c = (int)(*str++)))
			{
				std::cout << (char)c;
				++ col;
				if (col == max_col)
					{
						if (!std::isspace (c) && !std::isspace (*str))
							std::cout << '-';
						std::cout << "\n";
						
						if (col_start > 0)
							{
								for (int i = 0; i < col_start; ++i)
									std::cout << ' ';
								std::cout << "> ";
								col = col_start + 2;
							}
						else
							col = 0;
					}
			}
		
		// write to disk
		{
			std::time_t t = std::time (nullptr);
			struct tm lt;
			localtime_r (&t, &lt);
			
			if (lt.tm_mday != this->log.day)
				this->log.recalc_date ();
		}
		this->log.fs << output;
		if (this->log.fsc++ % 30 == 0)
			this->log.fs.flush ();
		
		this->str (std::string ());
		return 0;
	}
	
	
	
	/* 
	 * Class constructor.
	 */
	logger::logger_strm::logger_strm (logger& log)
		: std::ostream (&buf), buf (log)
		{ }
	
	
	
	static void
	_mark_session (std::ofstream& fs)
	{
		if (!fs) return;
		
		std::time_t t = std::time (nullptr);
		struct tm lt;
		localtime_r (&t, &lt);
		
		if (fs.tellp () > 0)
			fs << "\n\n" << std::flush;
		
		fs <<
"********************************************************************************\n"
"    New Session:\n"
"      @ " << std::setfill ('0')
 					 << std::setw (2) << lt.tm_hour << ":"
 					 << std::setw (2) << lt.tm_min << ":"
 					 << std::setw (2) << lt.tm_sec << "   "
 					 << std::setw (2) << lt.tm_mon << "/"
 					 << std::setw (2) << lt.tm_mday << "/"
 					 << (lt.tm_year + 1900) << std::setfill (' ') << "\n" <<
"********************************************************************************\n"
			 << std::endl;
	}
	
	/* 
	 * Class constructor.
	 * 
	 * Throws `std::runtime_error' on failure.
	 */
	logger::logger ()
	{
		// create log file
		this->recalc_date ();
		
		_mark_session (this->fs);
		
		if (pthread_key_create (&this->strm_key,
			[] (void *param)
				{
					delete static_cast<logger::logger_strm *> (param);
				}))
			throw std::runtime_error ("failed to create stream key");
	}
	
	
	
	/* 
	 * Reopens the internal file stream and sets its path to a file whose name
	 * contains the appropriate date.
	 */
	void
	logger::recalc_date ()
	{
		if (this->fs.is_open ())
			this->fs.close ();
		this->fsc = 0;
		
		std::time_t t = std::time (nullptr);
		struct tm lt;
		localtime_r (&t, &lt);
		this->day = lt.tm_mday;
		
		std::ostringstream ss;
		ss << "data/logs/log-" << std::setfill ('0')
			 << std::setw (2) << lt.tm_mon << "-"
			 << std::setw (2) << lt.tm_mday << "-"
			 << (lt.tm_year + 1900) << ".log";
		
		mkdir ("data/logs", 0744);
		fs.open (ss.str (), std::ios_base::out | std::ios_base::app);
		if (!fs)
			{
				std::cout << "[LOGGER ERROR] Failed to open logfile for writing!" << std::endl;
				return;
			}
	}
	
	
	
	static void
	write_logtype_and_time (logger::logger_strm& strm, logtype lt)
	{
		std::time_t t1;
		std::tm     t2;
		
		t1 = std::chrono::system_clock::to_time_t (
			std::chrono::system_clock::now ());
		localtime_r (&t1, &t2);
		
		static const char *logtype_names[] =
			{
				"debug  ",
				"system ",
				
				"chat   ",
				"draw   ",
				"console",
				
				"info   ",
				"warning",
				"error  ",
				"fatal  ",
			};
		
		strm << logtype_names[lt] << " | " << std::setfill ('0')
				 << std::setw (2) << t2.tm_hour << ':'
				 << std::setw (2) << t2.tm_min  << ':'
				 << std::setw (2) << t2.tm_sec  << " | " << std::setfill (' ');
	}
	
	/* 
	 * Returns a stream unique to the calling thread that it can use to log
	 * events. Returning a thread-local instance this way ensures
	 * thread-safety.
	 * 
	 * NOTE: After messages are written to the stream, the stream MUST be
	 * flushed (through any means, i.e: std::endl, std::flush, or just by
	 * directly calling flush () on it). Also note that by flushing the
	 * logger, the actual underlying stream is not necessarily flushed as
	 * well, this is just to let the logger know that the message has been
	 * fully written.
	 */
	logger::logger_strm&
	logger::operator() (logtype lt)
	{
		void *ptr;
		
		ptr = pthread_getspecific (this->strm_key);
		if (!ptr)
			{
				ptr = new logger_strm (*this);
				if (pthread_setspecific (this->strm_key, ptr))
					{
						delete static_cast<logger_strm *> (ptr);
						throw std::runtime_error ("failed to bind key with stream");
					}
			}
		
		logger_strm* strm = static_cast<logger_strm *> (ptr);
		write_logtype_and_time (*strm, lt);
		return *strm;
	}
}

