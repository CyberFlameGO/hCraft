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

#include "world/generation/super_overhang.hpp"
#include "slot/blocks.hpp"
#include <random>


namespace hCraft {
	
	/* 
	 * Constructs a new super overhang world generator.
	 */
	super_overhang_world_generator::super_overhang_world_generator (long seed)
		: gen_trees (9, 0, {BT_TRUNK, 2}, {BT_LEAVES, 2})
	{
		this->gen_seed = seed; 
		
		this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
		this->pn1.SetFrequency (0.03);
		this->pn1.SetPersistence (0.38);
		this->pn1.SetLacunarity (2.5);
		this->pn1.SetOctaveCount (2);

		this->co1.SetConstValue (14.8);
		this->mu1.SetSourceModule (0, this->pn1);
		this->mu1.SetSourceModule (1, this->co1);
		
		this->pn2.SetNoiseQuality (noise::QUALITY_FAST);
		this->pn2.SetFrequency (0.03);
		this->pn2.SetPersistence (0.3);
		this->pn2.SetLacunarity (1.3);
		this->pn2.SetOctaveCount (2);
		
		this->pn3.SetNoiseQuality (noise::QUALITY_FAST);
		this->pn3.SetFrequency (0.021);
		this->pn3.SetPersistence (0.8);
		this->pn3.SetLacunarity (3.0);
		this->pn3.SetOctaveCount (2);

		this->se1.SetSourceModule (1, this->mu1);
		this->se1.SetSourceModule (0, this->pn2);
		this->se1.SetControlModule (this->pn3);
		this->se1.SetBounds (-1.0, 2.5);
		this->se1.SetEdgeFalloff (0.34);
		
		this->seed (seed);
	}
	
	
	
	void
	super_overhang_world_generator::seed (long s)
	{
		this->pn1.SetSeed (s & 0x7FFFFFFF);
		this->pn2.SetSeed ((s + 1) & 0x7FFFFFFF);
		this->pn3.SetSeed ((s + 2) & 0x7FFFFFFF);
		this->gen_trees.seed (s);
	}
	
	
	
	void
	super_overhang_world_generator::terrain (world& wr, chunk *out, int cx, int cz)
	{
		std::minstd_rand rnd (this->gen_seed + cx * 1917 + cz * 3947);
		
		int y;
		double v;
		for (int x = 0; x < 16; ++x)
			for (int z = 0; z < 16; ++z)
				{
					y = 0;
					out->set_id (x, 0, z, BT_BEDROCK);
					for (y = 1; y < 40; ++y)
						{
							out->set_id (x, y, z, BT_STONE);
						}
					
					for (; y < 150; ++y)
						{
							v = this->se1.GetValue (((cx << 4) | x) * 0.32, y, ((cz << 4) | z) * 0.52) + ((60 - y) * 0.04);
							if (v > 0.0)
								out->set_id (x, y, z, BT_STONE);
							else if (y < 50)
								out->set_id (x, y, z, BT_WATER);
						}
				}
	}
	
	void
	super_overhang_world_generator::decorate (world& wr, chunk *out, int cx, int cz)
	{
		std::minstd_rand rnd (this->gen_seed + cx * 1917 + cz * 3947);
		std::uniform_int_distribution<> dis (0, 512);
		std::uniform_int_distribution<> grass_dis (0, 20);
		std::uniform_int_distribution<> tree_dis (0, 256);
		std::uniform_int_distribution<> waterfall_dis (0, 1000);
		
		int id, xx, zz;
		for (int x = 0; x < 16; ++x)
			for (int z = 0; z < 16; ++z)
				{
					out->set_biome (x, z, BI_EXTREME_HILLS);
					
					int depth = 0;
					int y = 150;
					for (; y > 30; --y)
						{
							id = out->get_id (x, y, z);
							if (id == BT_STONE)
								{
									if (depth == 0)
										{
											if (y == 50)
												out->set_id (x, y, z, BT_SAND);
											else
												out->set_id (x, y, z, BT_GRASS);
											if (y > 50)
												{
													if (y >= 64 && tree_dis (rnd) > 253)
														this->gen_trees.generate (wr, (cx << 4) | x, y + 1, (cz << 4) | z);
													else if (tree_dis (rnd) > 240)
														out->set_id (x, y + 1, z, BT_DANDELION);
													else if (grass_dis (rnd) < 8)
														out->set_block (x, y + 1, z, BT_TALL_GRASS, 1);
												}
										}
									else if (depth < 5)
										{
											out->set_id (x, y, z, BT_DIRT);
										}
									
									++ depth;
								}
							else
								{
									if (depth > 5 && out->get_id (x, y + 1, z) == BT_STONE)
										{
											if (dis (rnd) == 0)
												out->set_id (x, y + 1, z, BT_GLOWSTONE_BLOCK);
											else
												{
													// try for a waterfall
													bool waterfall = false;
													int h = 0;
													for (int yy = y - 1; yy >= 0; --yy)
														{
															int id = out->get_id (x, yy, z);
															if (id != BT_AIR)
																{
																	if (id == BT_WATER)
																		{
																			waterfall = true;
																			h = y - yy;
																		}
																	break;
																}
														}
										
													if (waterfall && h > 10 && waterfall_dis (rnd) < 3)
														{
															// spawn waterfall
															for (int i = -2; i < h; ++i)
																out->set_block (x, y - i, z, BT_WATER, 8);
														}
												}
										}
									
									depth = 0;
								}
						}
				}
	}
	
	
	
	
	/* 
	 * Generates on the specified chunk.
	 */
	void
	super_overhang_world_generator::generate (world& wr, chunk *out, int cx, int cz)
	{
		this->terrain (wr, out, cx, cz);
		this->decorate (wr, out, cx, cz);
	}
}

