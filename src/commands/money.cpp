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

#include "commands/infoc.hpp"
#include "server.hpp"
#include "player.hpp"
#include "stringutils.hpp"
#include "utils.hpp"
#include "sqlops.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
		
		enum _m_action
		{
			M_GIVE,
			M_TAKE,
			M_PAY,
		};
		
		static void
		_pay_give_take (player *pl, command_reader& reader, _m_action act)
		{
			if ( ((act == M_PAY) && !pl->has ("command.info.money.pay")) ||
					 ((act == M_TAKE || act == M_GIVE) && !pl->has ("command.info.money.give")) )
				{
					pl->message ("§c * §7You are not allowed to do that§c.");
					return;
				}
			else if (reader.arg_count () != 3)
				{
					if (act == M_PAY)
						pl->message ("§c * §7Usage§c: §e/money pay §cplayer amount");
					else if (act == M_GIVE)
						pl->message ("§c * §7Usage§c: §e/money give §cplayer amount");
					else
						pl->message ("§c * §7Usage§c: §e/money take §cplayer amount");
					return;
				}
				
			std::string target_name = reader.next ();
			double amount = reader.next ().as_float ();
			
			if (act == M_PAY)
				{
					if (sutils::iequals (target_name, pl->get_username ()))
						{
							pl->message ("§c * §7You cannot send yourself money§c.");
							return;
						}
					
					if (amount < 0.01)
						{
							pl->message ("§c * §7Invalid amount§c.");
							return;
						}
					else if (amount > pl->bal)
						{
							double left = amount - pl->bal;
							if (left < 10000000.0)
								pl->message ("§c * §7You are §c$" + utils::format_number (left, 2) + " §7short§c.");
							else
								pl->message ("§c * §7Not enough money§c.");
							return;
						}
				}
			else if (act == M_TAKE)
				{
					amount = -amount;
					if (amount >= 0.0)
						{
							pl->message ("§c * §7Invalid amount§c.");
							return;
						}
				}
			else if (amount <= 0.0)
				{
					pl->message ("§c * §7Invalid amount§c.");
					return;
				}
		
			std::string colored_target_name;
			
			// the easy way
			player *target = pl->get_server ().get_players ().find (target_name.c_str ());
			if (target)
				{
					if (act == M_PAY)
						pl->bal -= amount;
					target->bal += amount;
					target_name.assign (target->get_username ());
					colored_target_name.assign (target->get_colored_username ());
				}
			else
				{
					// the hard way
					auto& conn = pl->get_server ().sql ().pop ();
					try
						{
							if (!sqlops::player_exists (conn, target_name.c_str ()))
								{
									pl->message ("§c * §7No such player§f: §c" + target_name);
									return;
								}
							if (act == M_PAY)
								pl->bal -= amount;
							sqlops::add_money (conn, target_name.c_str (), amount);
							colored_target_name.assign (sqlops::player_colored_name (conn,
								target_name.c_str (), pl->get_server ()));
							target_name.assign (sqlops::player_name (conn, target_name.c_str ()));
						}
					catch (const std::exception& ex)
						{
							pl->message ("§4 * §cAn error has occurred§4.");
							pl->get_server ().sql ().push (conn);
							return;
						}
					pl->get_server ().sql ().push (conn);
				}
			
			std::ostringstream ss;
			if (act == M_PAY)
				ss << "§7$" << utils::format_number (amount, 2)
					 << " §ehas been sent to " << colored_target_name;
			else if (act == M_GIVE)
				ss << "§7$" << utils::format_number (amount, 2)
					 << " §ehas been added to " << colored_target_name << "§e's account";
			else
				ss << "§7$" << utils::format_number (-amount, 2)
					 << " §ehas been taken from " << colored_target_name << "§e's account";
			pl->message (ss.str ());
			if ((act != M_TAKE) && (target && target != pl))
				{
					ss.clear (); ss.str (std::string ());
					ss << pl->get_colored_username () << " §ehas sent you §7$"
						 << utils::format_number (amount, 2);
				 	target->message (ss.str ());
				}
			
			if (act == M_TAKE)
				amount = -amount;
			pl->get_logger () (LT_SYSTEM) << pl->get_username () << " has " <<
				((act == M_PAY) ? "sent" : ((act == M_GIVE) ? "given" : "taken"))
				<< " $" << utils::format_number (amount, 2) <<
					((act == M_TAKE) ? " from " : " to ") << target_name << std::endl;
			return;
		}
		
		static void
		do_pay (player *pl, command_reader& reader)
			{ _pay_give_take (pl, reader, M_PAY); }
		
		static void
		do_give (player *pl, command_reader& reader)
			{ _pay_give_take (pl, reader, M_GIVE); }
		
		static void
		do_take (player *pl, command_reader& reader)
			{ _pay_give_take (pl, reader, M_TAKE); }
			
		
		
		static void
		do_set (player *pl, command_reader& reader)
		{
			if (!pl->has ("command.info.money.set"))
				{
					pl->message ("§c * §7You are not allowed to do that§c.");
					return;
				}
			else if (reader.arg_count () != 3)
				{
					pl->message ("§c * §7Usage§c: §e/money set §cplayer amount");
					return;
				}
				
			std::string target_name = reader.next ();
			double amount = reader.next ().as_float ();
			
			std::string colored_target_name;
			
			// the easy way
			player *target = pl->get_server ().get_players ().find (target_name.c_str ());
			if (target)
				{
					target->bal = amount;
					colored_target_name.assign (target->get_colored_username ());
				}
			else
				{
					// the hard way
					auto& conn = pl->get_server ().sql ().pop ();
					try
						{
							if (!sqlops::player_exists (conn, target_name.c_str ()))
								{
									pl->message ("§c * §7No such player§f: §c" + target_name);
									return;
								}
							sqlops::set_money (conn, target_name.c_str (), amount);
							colored_target_name.assign (sqlops::player_colored_name (conn,
								target_name.c_str (), pl->get_server ()));
						}
					catch (const std::exception& ex)
						{
							pl->message ("§4 * §cAn error has occurred§4.");
							pl->get_server ().sql ().push (conn);
							return;
						}
					pl->get_server ().sql ().push (conn);
				}
			
			std::ostringstream ss;
			ss << colored_target_name << "§e's balance has been set to §7$"
				 << utils::format_number (amount, 2);
			pl->message (ss.str ());
		}
		
		
		/* 
		 * /money -
		 * 
		 * Displays the amount of money a player has.
		 * 
		 * Permissions:
		 *   - command.info.money
		 *       Needed to execute the command.
		 *   - command.info.money.others
		 *       Required to display the amount of money _other_ players have.
		 *   - command.info.money.pay
		 *       Whether the player is allowed to send money out of their own account.
		 *   - command.info.money.give
		 *       Whether the player is allowed to give money to players _wihout_
		 *       taking it out of their own account.
		 *   - command.info.money.set
		 *       Required to modify a player's balance directly.
		 */
		void
		c_money::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
				return;
			
			if (!reader.parse (this, pl))
				return;
			
			bool someone_else = false;
			std::string target_name;
			if (!reader.has_next ())
				{
					target_name.assign (pl->get_username ());
				}
			else
				{
					std::string& a = reader.next ();
					if (sutils::iequals (a, "pay"))
						{
							do_pay (pl, reader);
							return;
						}
					else if (sutils::iequals (a, "give"))
						{
							do_give (pl, reader);
							return;
						}
					else if (sutils::iequals (a, "take"))
						{
							do_take (pl, reader);
							return;
						}
					else if (sutils::iequals (a, "set"))
						{
							do_set (pl, reader);
							return;
						}
					else
						{
							target_name = a;
							someone_else = true;
						}
				}
			
			// 
			// /money [player]
			// 
			std::ostringstream ss;
			if (someone_else)
				{
					if (!pl->has ("command.info.money.others"))
						{
							pl->message ("§c * §7You are not allowed to do that§c.");
							return;
						}
					
					if (sutils::iequals (target_name, pl->get_username ()))
						ss << "§2Balance§8: §a$§f" << utils::format_number (pl->bal, 2);
					else
						{
							player *target = pl->get_server ().get_players ().find (target_name.c_str ());
							if (target)
								ss << target->get_colored_username () << "§e's balance§f: §a$§f" << utils::format_number (target->bal, 2);
							else
								{
									auto& conn = pl->get_server ().sql ().pop ();
									
									try
										{
											if (!sqlops::player_exists (conn, target_name.c_str ()))
												{
													pl->message ("§c * §7No such player§f: §c" + target_name);
													return;
												}
											
											ss << sqlops::player_colored_name (conn, target_name.c_str (), pl->get_server ())
												 << "§2's balance§f: §a$§f" << utils::format_number (sqlops::get_money (conn, target_name.c_str ()), 2);
										}
									catch (const std::exception& ex)
										{
											pl->message ("§4 * §cAn error has occurred§4.");
											pl->get_server ().sql ().push (conn);
											
											return;
										}
									
									pl->get_server ().sql ().push (conn);
								}
						}
				}
			else
				ss << "§2Balance§f: §a$§f" << utils::format_number (pl->bal, 2);
			
			pl->message (ss.str ());
		}
	}
}

