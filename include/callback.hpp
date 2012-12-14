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

#ifndef _hCraft__CALLBACK_H_
#define _hCraft__CALLBACK_H_

#include <vector>
#include <functional>


namespace hCraft {
	
	template <typename TSig>
	class callback;
	
	/* 
	 * Manages a list of functors.
	 */
	template<typename... TArgs>
	class callback<bool (TArgs...)>
	{
		std::vector<std::function<bool (TArgs...)> > cbs;
		
	public:
		callback () = default;
		
		callback (const callback& cb)
			: cbs (cb.cbs) { }
		
		callback (callback&& cb)
			: cbs (std::move (cb.cbs)) { }
		
		
		// number of callbacks registered
		bool size () { return this->cbs.size (); }
		
		
		/* 
		 * Adds a callback function to the list managed in this object.
		 */
		template <typename F> callback&
		operator+= (F func)
		{
			cbs.push_back (func);
			return *this;
		}
		
		void
		operator() (TArgs... args)
		{
			for (auto itr = this->cbs.begin (); itr != this->cbs.end (); )
				{
					auto& cb = *itr;
					if (cb (args...))
						{
							// callback returned true, remove it from the list.
							itr = this->cbs.erase (itr);
						}
					else
						++ itr;
				}
		}
	};
}

#endif

