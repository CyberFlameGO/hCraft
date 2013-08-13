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

#include "authentication.hpp"
#include "player.hpp"
#include "server.hpp"
#include <functional>
#include <cryptopp/sha.h>
#include <curl/curl.h>


namespace hCraft {
	
	authenticator::authenticator ()
	{
		this->th = nullptr;
		this->running = false;
	}
	
	authenticator::~authenticator ()
	{
		this->stop ();
	}
	
	
	
	/* 
	 * Starts/stops the authenticator thread.
	 */
	void
	authenticator::start ()
	{
		if (this->running)
			return;
		
		this->running = true;
		this->th = new std::thread (
			std::bind (std::mem_fn (&hCraft::authenticator::worker), this));
	}
	
	void
	authenticator::stop ()
	{
		if (!this->running)
			return;
		
		this->running = false;
		if (this->th->joinable ())
			this->th->join ();
		delete this->th;
	}
	
	
	
	/* 
	 * Queues the specified player for authentication.
	 */
	void
	authenticator::enqueue (player *pl)
	{
		this->q.push (pl);
	}
	
	
	
//----
	
	static void
	twos_complement (unsigned char *digest)
	{
		int i;
		bool c = true;
		for (i = 19; i >= 0; --i)
			{
				digest[i] = ~digest[i];
				if (c)
					{
						c = (digest[i] == 0xFF);
						++ digest[i];
					}
			}
	}	
	
	static char
	hex_digit (int n)
		{ return (n >= 10) ? ('a' + (n - 10)) : ('0' + n); }
	
	static std::string
	java_digest (const unsigned char *in, int len)
	{
		CryptoPP::SHA1 sha;
		
		unsigned char digest[20];
		sha.CalculateDigest (digest, in, len);
		
		bool neg = digest[0] & 0x80;
		if (neg)
			twos_complement (digest);
		
		// convert to string
		std::string str;
		if (neg)
			str.push_back ('-');
		bool start = true;
		for (int i = 0; i < 20; ++i)
			{
				char a = (digest[i] & 0xF0) >> 4;
				char b = digest[i] & 0x0F;
				if (!start || a) { start = false; str.push_back (hex_digit (a)); }
				if (!start || b) { start = false; str.push_back (hex_digit (b)); }
			}
		
		return str;
	}
	
	
	
	static size_t
	write_func (void *ptr, size_t size, size_t nmemb, void *stream)
	{
		(static_cast<std::string *> (stream))
			->append ((char *)ptr, 0, size*nmemb);
		return size * nmemb;
	}
	
	bool
	authenticator::auth_player (player *pl)
	{
		// compute hash
		unsigned char hash_data[512];
		std::memcpy (hash_data, pl->srv.auth_id ().data (), 16);
		std::memcpy (hash_data + 16, pl->ssec, 16);
		CryptoPP::ByteQueue q;
		pl->srv.public_key ().Save (q);
		unsigned char key[384];
		size_t keylen = q.Get (key, sizeof key);
		std::memcpy (hash_data + 32, key, keylen);
		std::string hash = java_digest (hash_data, 32 + keylen);
		
		std::string url;
		url.append ("http://session.minecraft.net/game/checkserver.jsp?user=");
		url.append (pl->get_username ());
		url.append ("&serverId=");
		url.append (hash);
		
		std::string resp;
		CURL *curl_handle = curl_easy_init ();
		curl_easy_setopt (curl_handle, CURLOPT_URL, url.c_str ());
		curl_easy_setopt (curl_handle, CURLOPT_WRITEFUNCTION, write_func);
		curl_easy_setopt (curl_handle, CURLOPT_WRITEDATA, &resp);
		
		curl_easy_perform (curl_handle);
		curl_easy_cleanup (curl_handle);
		
		return (resp.compare ("YES") == 0);
	}
	
	/* 
	 * Where everything happens.
	 */
	void
	authenticator::worker ()
	{
		while (this->running)
			{
				std::this_thread::sleep_for (std::chrono::milliseconds (50));
				if (this->q.empty ())
					continue;
				
				// three tries
				player *pl = nullptr;
				for (int i = 0; i < 3; ++i)
					{
						if (!this->q.try_pop (pl))
							continue;
					}
				if (!pl)
					continue;
				
				if (this->auth_player (pl))
					pl->done_authenticating ();
				else
					pl->kick ("§cFailed to verify username", "Authentication failed");
			}
	}
}

