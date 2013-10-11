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

#ifndef _hCraft__IRC_H_
#define _hCraft__IRC_H_

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <queue>
#include <string>
#include <mutex>


namespace hCraft {
	
	class server;
	class logger;

#define IRC_MAXBUFSIZE		512
	
	struct irc_message {
		std::string prefix;
		std::string command;
		std::string rest;
	};
	
	/* 
	 * The IRC client.
	 */
	class irc_client
	{
		server& srv;
		logger& log;
		
		std::string nick;
		std::string chan;
		bool connected;
		
		struct event_base *evbase;
		struct bufferevent *bufev;
		evutil_socket_t sock;
		
		std::mutex out_lock;
		std::queue<std::string> out_queue;
		
		char rdbuf[IRC_MAXBUFSIZE];
		int total_read;
		bool fail;
		
	private:
		/* 
		 * libevent callback functions:
		 */
		static void handle_read (struct bufferevent *bufev, void *ctx);
		static void handle_write (struct bufferevent *bufev, void *ctx);
		static void handle_event (struct bufferevent *bufev, short events, void *ctx);
		
		
		/* 
		 * Attempts to extract a message from the internal buffer, if possible.
		 */
		void parse_message ();
		
		/* 
		 * Handles the specified (broken up) IRC message (called by parse_message ()).
		 */
		void handle_message (const irc_message& msg);
		void handle_reply_code (const irc_message& msg);
		
	public:
		server& get_server () const { return this->srv; }
		logger& get_logger () const { return this->log; }
		const std::string& get_nick () const { return this->nick; }
		const std::string& get_channel () const { return this->chan; }
		
	public:
		/* 
		 * Constructs a new IRC client on top of hte specified socket and event base.
		 */
		irc_client (server &srv, struct event_base *evbase, int sock);
		
		/* 
		 * Destructor - Resource cleanup.
		 */
		~irc_client ();
		
		
		
		/* 
		 * Sends the correct initial messages to the server (NICK/USER).
		 */
		void connect (const std::string& nick, const std::string& chan);
		
		/* 
		 * Closes the internal socket.
		 */
		void disconnect ();
		
		
		
		/* 
		 * Enqueues the specified message to be sent.
		 */
		void send (const std::string& str);
		
		/* 
		 * Joins the specified channel.
		 */
		void join (const std::string& chan);
		
		/* 
		 * Sends a message to the channel the bot's in.
		 */
		void chan_msg (const std::string& msg);
	};
}

#endif

