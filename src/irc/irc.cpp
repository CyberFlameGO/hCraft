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

#include "irc/irc.hpp"
#include "system/server.hpp"
#include "system/logger.hpp"
#include "util/stringutils.hpp"
#include "util/wordwrap.hpp"
#include <event2/buffer.h>
#include <cstdlib>


namespace hCraft {
	
	/* 
	 * Constructs a new IRC client on top of hte specified socket and event base.
	 */
	irc_client::irc_client (server &srv, struct event_base *evbase, int sock)
		: srv (srv), log (srv.get_logger ())
	{
		this->evbase = evbase;
		this->sock = sock;
		
		this->bufev = bufferevent_socket_new (evbase, sock, BEV_OPT_CLOSE_ON_FREE);
		
		bufferevent_setcb (this->bufev, &hCraft::irc_client::handle_read,
			&hCraft::irc_client::handle_write, &hCraft::irc_client::handle_event, this);
		bufferevent_enable (this->bufev, EV_READ | EV_WRITE);
		
		this->total_read = 0;
		this->fail = false;
		this->connected = false;
	}
	
	/* 
	 * Destructor - Resource cleanup.
	 */
	irc_client::~irc_client ()
	{
		this->disconnect ();
		
		bufferevent_free (this->bufev);
	}
	
	
	
//----
	/* 
	 * libevent callback functions:
	 */
	
	void
	irc_client::handle_read (struct bufferevent *bufev, void *ctx)
	{
		irc_client *ircc = static_cast<irc_client *> (ctx);
		if (ircc->fail || ircc->srv.is_shutting_down ())
			return;
		
		struct evbuffer *buf = bufferevent_get_input (bufev);
		size_t buf_size;
		int n;
		
		while ((buf_size = evbuffer_get_length (buf)) > 0)
			{
				int take = buf_size;
				if (take + ircc->total_read > IRC_MAXBUFSIZE)
					take = IRC_MAXBUFSIZE - ircc->total_read;
				
				n = evbuffer_remove (buf, ircc->rdbuf + ircc->total_read, take);
				if (n <= 0)
					{
						ircc->disconnect ();
						return;
					}
				
				ircc->total_read += n;
				ircc->parse_message ();
				if (ircc->total_read == 512)
					{
						// shouldn't happen
						ircc->disconnect ();
						return;
					}
			}
	}
	
	void
	irc_client::handle_write (struct bufferevent *bufev, void *ctx)
	{
		irc_client *ircc = static_cast<irc_client *> (ctx);
		if (ircc->fail || ircc->srv.is_shutting_down ())
			return;
		
		std::lock_guard<std::mutex> guard {ircc->out_lock};
		if (!ircc->out_queue.empty ())
			{
				ircc->out_queue.pop ();
				if (!ircc->out_queue.empty ())
					{
						std::string &s = ircc->out_queue.front ();
						bufferevent_write (bufev, s.data (), s.length ());
					}
			}
	}
	
	void
	irc_client::handle_event (struct bufferevent *bufev, short events, void *ctx)
	{
		irc_client *ircc = static_cast<irc_client *> (ctx);
		if (ircc->fail || ircc->srv.is_shutting_down ())
			return;
		
		ircc->disconnect ();
	}
	
	
	
//----
	
	/* 
	 * Sends the correct initial messages to the server (NICK/USER).
	 */
	void
	irc_client::connect (const std::string& nick, const std::string& chan)
	{
		this->nick = nick;
		this->chan = chan;
		
		this->send ("NICK " + nick);
		this->send ("USER " + nick + " 0 * :" + nick);
	}
	
	/* 
	 * Closes the internal socket.
	 */
	void
	irc_client::disconnect ()
	{
		if (this->fail)
			return;
		
		this->log (LT_SYSTEM) << "IRC bot disconnecting" << std::endl;
		bufferevent_disable (this->bufev, EV_READ | EV_WRITE);
		bufferevent_setcb (this->bufev, nullptr, nullptr, nullptr, nullptr);
		this->fail = true;
	}
	
	
	
	/* 
	 * Attempts to extract a message from the internal buffer, if possible.
	 */
	void
	irc_client::parse_message ()
	{
		char msg[IRC_MAXBUFSIZE + 1];
		int msg_len = -1;
		
		char *buf = this->rdbuf;
		for (int i = 0; i < this->total_read; ++i)
			if (buf[i] == '\n')
				{
					msg_len = i;
					if (i > 0 && buf[i - 1] == '\r')
						-- msg_len;
					std::memcpy (msg, buf, msg_len);
					msg[msg_len] = '\0';
					
					int end = i + 1;
					if (this->total_read - end > 0)
						std::memmove (buf, buf + end, this->total_read - end);
					this->total_read -= end;
				}
		
		if (msg_len == -1)
			return;
		
		irc_message imsg {};
		const char *ptr = msg;
		
		// prefix
		if (*ptr == ':')
			{
				++ ptr;
				while (*ptr && (*ptr != ' '))
					imsg.prefix.push_back (*ptr++);
				if (*ptr == ' ')
					++ ptr;
			}
		
		// command
		while (*ptr && (*ptr != ' '))
			imsg.command.push_back (*ptr++);
		if (*ptr == ' ')
			++ ptr;
		
		// and the rest
		while (*ptr)
			imsg.rest.push_back (*ptr++);
		
		this->handle_message (imsg);
	}
	
	
	static bool
	_is_reply_code (const std::string& str)
	{
		if (str.length () != 3)
			return false;
		
		for (char c : str)
			if (!(c >= '0' && c <= '9'))
				return false;
		return true;
	}
	

	
	static void
	_handle_ping (irc_client *ircc, const irc_message& msg)
	{
		ircc->send ("PONG " + msg.rest);
	}
	
	static void
	_handle_privmsg (irc_client *ircc, const irc_message& msg)
	{
		auto sp = msg.rest.find_first_of (' ');
		if (sp == std::string::npos)
			return;
		
		std::string chan = msg.rest.substr (0, sp);
		if (!sutils::iequals (chan, ircc->get_channel ()))
			return;
		
		std::string text = msg.rest.substr (sp + 1);
		if (text.empty () || text[0] != ':')
			return;
		text.erase (0, 1);
		
		std::string name = msg.prefix;
		auto np = name.find_first_of ('!');
		if (np == std::string::npos)
			return;
		name.erase (np);
		
		ircc->get_logger () (LT_IRC) << name << ": " << text << std::endl;
		ircc->get_server ().get_players ().message (
			"§c# §8§o(IRC) §r§5" + name + ": §f" + text); 
	}
	
	/* 
	 * Handles the specified (broken up) IRC message (called by parse_message ()).
	 */
	void
	irc_client::handle_message (const irc_message& msg)
	{
		if (_is_reply_code (msg.command))
			this->handle_reply_code (msg);
		
		if (msg.command == "PING")
			_handle_ping (this, msg);
		else if (msg.command == "PRIVMSG")
			_handle_privmsg (this, msg);
	}
	
	void
	irc_client::handle_reply_code (const irc_message& msg)
	{
		int code = std::strtol (msg.command.c_str (), nullptr, 10);
		
		switch (code)
			{
				case 1:		// welcome
				case 2:		// your host
				case 3:		// created
				case 4:		// my info
				case 5:		// bouncer
					if (!this->connected)
						{
							this->join (this->chan);
							this->connected = true;
						}
					break;
			}
	}
	
	
	
	/* 
	 * Enqueues the specified message to be sent.
	 */
	void
	irc_client::send (const std::string& str)
	{
		std::lock_guard<std::mutex> guard {this->out_lock};
		
		this->out_queue.push (str);
		std::string& s = this->out_queue.back ();
		s.append ("\r\n");
		
		if (this->out_queue.size () == 1)
			{
				// initiate write
				bufferevent_write (this->bufev, s.data (), s.length ());
			}
	}
	
	/* 
	 * Joins the specified channel.
	 */
	void
	irc_client::join (const std::string& chan)
	{
		this->send ("JOIN " + chan);
	}
	
	
	
	static inline int
	_hex_to_int (char c)
	{
		if (c >= '0' && c <= '9')
			return c - '0';
		else if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			return c - 'F' + 10;
		return 0; 
	}
	
	// MC colors -> IRC colors
	static std::string
	_translate_colors (const std::string& in)
	{
		std::string out;
		
		for (size_t i = 0; i < in.length (); ++i)
			{
				if ((in[i] & 0xFF) == 0xC2 && ((i + 2) < in.length ()) && (in[i + 1] & 0xFF) == 0xA7)
					{
						if (is_style_code (in[i + 2]))
							i += 2;
						else
							{
								int c = _hex_to_int (in[i + 2]);
								const static int color_table[] = {
									1, 2, 3, 10, 4, 6, 7, 15, 14, 12, 9, 11, 4, 13, 8, 1
								};
						
								if (c >= 0 && c <= 15)
									{
										int irc_col = color_table[c];
										out.push_back (0x03); // ^C
										if (irc_col >= 10)
											out.push_back ('1');
										out.push_back ((irc_col % 10) + '0');
										i += 2;
									}
								else
									out.push_back (in[i]);
							}
					}
				else
					out.push_back (in[i]);
			}
		
		return out;
	}
	
	/* 
	 * Sends a message to the channel the bot's in.
	 */
	void
	irc_client::chan_msg (const std::string& msg)
	{
		this->send ("PRIVMSG " + this->chan + " :" + _translate_colors (msg));
	}
}

