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

#include "generation/experiment.hpp"
#include "generation/detail/trees.hpp"
#include "noise.hpp"
#include "utils.hpp"
#include <random>
#include <cmath>

#include <noise/noise.h>


namespace hCraft {
	
	namespace {
		
		/* 
		 * A highly mountainous biome.
		 */
		class overhang_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
			dgen::generic_trees gen_oak_trees, gen_birch_trees;
			
		public:
			overhang_biome (long s)
				: gen_birch_trees (5, {BT_TRUNK, 2}, {BT_LEAVES, 2})
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.016);
				this->pn1.SetPersistence (0.32);
				this->pn1.SetLacunarity (1.5);
				this->pn1.SetOctaveCount (2);
		
				this->co1.SetConstValue (7.8);
				this->mu1.SetSourceModule (0, this->pn1);
				this->mu1.SetSourceModule (1, this->co1);
				
				this->pn2.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn2.SetFrequency (0.03);
				this->pn2.SetPersistence (0.18);
				this->pn2.SetLacunarity (0.5);
				this->pn2.SetOctaveCount (2);
				
				this->pn3.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn3.SetFrequency (0.05);
				this->pn3.SetPersistence (0.7);
				this->pn3.SetLacunarity (0.55);
				this->pn3.SetOctaveCount (2);
		
				this->se1.SetSourceModule (1, this->mu1);
				this->se1.SetSourceModule (0, this->pn2);
				this->se1.SetControlModule (this->pn3);
				this->se1.SetBounds (-1.0, -0.1);
				this->se1.SetEdgeFalloff (0.34);
				
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				
				this->gen_birch_trees.seed (s);
				this->gen_oak_trees.seed (s);
				
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->pn2.SetSeed ((s + 1) & 0x7FFFFFFF);
				this->pn3.SetSeed ((s + 2) & 0x7FFFFFFF);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					this->se1.GetValue (x * 0.5, y, z * 0.7) + ((60 - y) * 0.06);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, ymin = this->min_y (), depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> tall_grass_dis (0, 256);
				std::uniform_int_distribution<> tree_dis (0, 256);
				std::uniform_int_distribution<> waterfall_dis (0, 1000);
				
				ch->set_biome (bx, bz, BI_FOREST);
				for (y = this->max_y (); y >= ymin; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								if (depth == 0)
									{
										if (y > 55)
											{
												ch->set_id (bx, y, bz, BT_GRASS);
												if (tall_grass_dis (rnd) > 160)
													ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
												else if (y >= 60 && tree_dis (rnd) > 253)
													{
														// spawn tree
														if (tree_dis (rnd) > 200)
															this->gen_birch_trees.generate (w, x, y + 1, z);
														else
															this->gen_oak_trees.generate (w, x, y + 1, z);
													}
											}
										else if (y == 55)
											ch->set_id (bx, y, bz, BT_SAND);
										else
											ch->set_id (bx, y, bz, BT_DIRT);
									}
								else if (depth < 5)
									ch->set_id (bx, y, bz, BT_DIRT);
								
								++ depth;
							}
						else
							{
								if (depth > 0 && ch->get_id (bx, y + 1, bz) == BT_STONE)
									{
										// overhang
										bool waterfall = false;
										int h = 0;
										for (int yy = y - 1; yy >= 0; --yy)
											{
												int id = ch->get_id (bx, yy, bz);
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
													ch->set_block (bx, y - i, bz, BT_WATER, 8);
											}
									}
								
								depth = 0;
							}
					}
			}
		};
		
		
		class wasteland_biome: public biome_generator
		{
			long gen_seed;
			
		public:
			wasteland_biome (long s)
				{ this->seed (s); }
			
			virtual bool is_3d () override { return false; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					h_noise::fractal_noise_2d (this->gen_seed, x / 26.0 + 0.5, z / 26.0 + 0.5, 4, 0.48) * 8.0 + 70.0;
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, rn, depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> tall_grass_dis (0, 256);
				std::uniform_int_distribution<> block_dis (0, 256);
				std::uniform_int_distribution<> cactus_dis (0, 256);
				
				ch->set_biome (bx, bz, BI_DESERT);
				for (y = 78; y >= 0; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								rn = block_dis (rnd);
								if (rn > 243)
									{
										ch->set_id (bx, y, bz, BT_SAND);
										if (y > 55 && depth == 0 && cactus_dis (rnd) < 5)
											{
												ch->set_id (bx, y + 1, bz, BT_CACTUS);
												if (cactus_dis (rnd) > 128)
													ch->set_id (bx, y + 2, bz, BT_CACTUS);
											}
									}
								else if (rn > 235)
									ch->set_id (bx, y, bz, BT_STONE);
								else
									{
										if (rn < 5)
											ch->set_id (bx, y, bz, BT_GRASS);
										else
											ch->set_id (bx, y, bz, BT_DIRT);
										if (y > 55 && depth == 0 && tall_grass_dis (rnd) > 250)
											ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
									}
								
								if (++ depth > 5)
									break;
							}
					}
			}
		};
		
		
		
		class flatlands_biome: public biome_generator
		{
			long gen_seed;
			dgen::round_trees gen_trees;
			
		public:
			flatlands_biome (long s)
				: gen_trees (6)
				{ this->seed (s); }
			
			virtual bool is_3d () override { return false; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->gen_trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					h_noise::fractal_noise_2d (this->gen_seed, x / 80.0 + 0.5, z / 80.0 + 0.5, 4, 0.48) + 56.0;
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, depth = 0;
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> tree_dis (0, 256);
				
				for (y = 80; y >= 0; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								if (depth == 0)
									{
										if (h_noise::perlin_noise_2d (this->gen_seed, x / 40.0 + 0.5, z / 40.0 + 0.5) > 0.0)
											ch->set_id (bx, y, bz, BT_SAND);
										else
											{
												if (y < 55)
													ch->set_id (bx, y, bz, BT_DIRT);
												else
													{
														ch->set_id (bx, y, bz, BT_GRASS);
														if (tree_dis (rnd) > 253)
															this->gen_trees.generate (w, x, y + 1, z);
													}
											}
									}
								else
									ch->set_id (bx, y, bz, BT_DIRT);
								
								if (++ depth == 5)
									break;
							}
					}
			}
		};
		
		
		
		class ocean_biome: public biome_generator
		{
			long gen_seed;
			
		public:
			ocean_biome (long s)
				{ this->seed (s); }
			
			virtual bool is_3d () override { return false; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					h_noise::fractal_noise_2d (this->gen_seed, x / 25.0 + 0.5, z / 25.0 + 0.5, 4, 0.45) * 2.0 + 48.0;
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				ch->set_biome (bx, bz, BI_PLAINS);
				for (y = 80; y >= 0; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								if (y >= 55)
									ch->set_id (bx, y, bz, BT_SAND);
								else
									ch->set_id (bx, y, bz, BT_DIRT);
								if (++ depth > 5)
									break;
							}
					}
			}
		};
		
		
		
		class desert_biome: public biome_generator
		{
			long gen_seed;
			
		public:
			desert_biome (long s)
				{ this->seed (s); }
			
			virtual bool is_3d () override { return false; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					h_noise::perlin_noise_2d (this->gen_seed, x / 40.0 + 0.5, z / 40.0 + 0.5) * 6.0 + 68.0;
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> tall_grass_dis (0, 256);
				std::uniform_int_distribution<> cactus_dis (0, 512);
				
				ch->set_biome (bx, bz, BI_DESERT);
				for (y = 80; y >= 0; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								ch->set_id (bx, y, bz, BT_SAND);
								if (depth == 0)
									{
										if (cactus_dis (rnd) > 510)
											{
												int h = cactus_dis (rnd) % 3 + 1;
												for (int i = 1; i <= h; ++i)
													ch->set_id (bx, y + i, bz, BT_CACTUS);
											}
										else if (tall_grass_dis (rnd) > 255)
											{
												ch->set_id (bx, y + 1, bz, BT_TALL_GRASS);
											}
									}
								
								if (++ depth > 5)
									break;
							}
					}
			}
		};
		
		
		
		class taiga_biome: public biome_generator
		{
			long gen_seed;
			dgen::pine_trees gen_trees;
			
		public:
			taiga_biome (long s)
				: gen_trees (8, 95)
				{ this->seed (s); }
			
			virtual bool is_3d () override { return false; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->gen_trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					h_noise::fractal_noise_2d (this->gen_seed, x / 75.0 + 0.5, z / 75.0 + 0.5, 4, 0.35) * 24.0 + 60.0;
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> snow_dis (0, 10);
				std::uniform_int_distribution<> tree_dis (0, 300);
				
				ch->set_biome (bx, bz, BI_TAIGA);
				for (y = 90; y >= 0; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								if (depth == 0 && y >= 55)
									{
										ch->set_id (bx, y, bz, BT_GRASS);
										if (snow_dis (rnd) < 4)
											ch->set_id (bx, y + 1, bz, BT_SNOW_COVER);
										else if (tree_dis (rnd) < 7)
											{
												this->gen_trees.generate (w, x, y + 1, z);
											}
										else if (snow_dis (rnd) > 0)
											{
												if (tree_dis (rnd) > 293)
													ch->set_id (bx, y + 1, bz, BT_ROSE);
												else
													ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
											}
									}
								else
									ch->set_id (bx, y, bz, BT_DIRT);
								if (++ depth > 5)
									break;
							}
						else if (ch->get_id (bx, y, bz) == BT_WATER)
							{
								if (y == 55)
									{
										if (snow_dis (rnd) > 3)
											ch->set_id (bx, y, bz, BT_ICE);
									}
							}
					}
			}
		};
		
		
		
		class steppe_biome: public biome_generator
		{
			long gen_seed;
			dgen::round_trees gen_trees;
			
		public:
			steppe_biome (long s)
				{ this->seed (s); }
			
			virtual bool is_3d () override { return false; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->gen_trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					h_noise::fractal_noise_2d (this->gen_seed, x / 35.0 + 0.5, z / 35.0 + 0.5, 4, 0.3) * 4.0 + 64.0;
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> tree_dis (0, 300);
				std::uniform_int_distribution<> grass_dis (0, 10);
				
				ch->set_biome (bx, bz, BI_FOREST);
				for (y = 90; y >= 0; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								if (depth == 0)
									{
										ch->set_id (bx, y, bz, BT_GRASS);
										if (grass_dis (rnd) >= 2)
											ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
										else if (tree_dis (rnd) == 300)
											{
												// spawn tree
												this->gen_trees.generate (w, x, y + 1, z);
											}
											
										if (h_noise::perlin_noise_2d (this->gen_seed, x / 10.0 + 0.5, z / 10.0 + 0.5) > 0.2)
											ch->set_biome (bx, bz, BI_PLAINS);
									}
								else
									ch->set_id (bx, y, bz, BT_DIRT);
								if (++ depth > 5)
									break;
							}
					}
			}
		};
		
		
		
		class alps_biome: public biome_generator
		{
			long gen_seed;
			noise::module::RidgedMulti ridg;
			dgen::pine_trees gen_trees;
			
		public:
			alps_biome (long s)
				: gen_trees (6, 0, {BT_TRUNK, 0}, {BT_LEAVES, 0})
			{
				this->ridg.SetNoiseQuality (noise::QUALITY_FAST);
				this->ridg.SetFrequency (0.01);
				this->ridg.SetLacunarity (2.5);
				this->ridg.SetOctaveCount (3);
				
				this->seed (s);
			}
			
			virtual bool is_3d () override { return false; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->ridg.SetSeed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					/*
					std::max (
						this->ridg.GetValue (x, 0, z) * 40.0 + 80.0,
						h_noise::fractal_noise_2d (this->gen_seed, x / 48.0 + 0.5, z / 48.0 + 0.5, 4, 0.58) * 6.0 + 70.0);*/
					this->ridg.GetValue (x, 0, z) * 40.0 + 80.0;
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> snow_dis (0, 8);
				std::uniform_int_distribution<> tree_dis (0, 300);
				
				int snow_start = 98 - snow_dis (rnd);
				
				ch->set_biome (bx, bz, BI_FOREST);
				for (y = 120; y >= 0; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								if (depth == 0)
									{
										if (y >= snow_start)
											ch->set_id (bx, y, bz, BT_SNOW_BLOCK);
										else if (y >= 80)
											{
												;
											}
										else if (y >= 55)
											{
												ch->set_id (bx, y, bz, BT_GRASS);
												if (y < 80)
													{
														if (tree_dis (rnd) > 285)
															this->gen_trees.generate (w, x, y + 1, z);
														else if (tree_dis (rnd) > 100)
															ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
													}
											}
									}
								else
									{
										//if (y < 70)
											ch->set_id (bx, y, bz, BT_DIRT);
									}
								
								if (++ depth > 6)
									break;
							}
					}
			}
		};
		
		
		
		class forest_biome: public biome_generator
		{
			long gen_seed;
			
			dgen::round_trees gen_oak_trees;
			dgen::pine_trees gen_pine_trees;
			
		public:
			forest_biome (long s)
				{ this->seed (s); }
			
			virtual bool is_3d () override { return false; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->gen_oak_trees.seed (s);
				this->gen_pine_trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				return
					h_noise::fractal_noise_2d (this->gen_seed, x / 72.0 + 0.5, z / 72.0 + 0.5, 4, 0.42) * 14.0 + 65.0;
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> tree_dis (0, 300);
				std::uniform_int_distribution<> grass_dis (0, 50);
				
				ch->set_biome (bx, bz, BI_FOREST);
				for (y = 90; y >= 0; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								if (depth == 0)
									{
										if (y >= 55)
											{
												ch->set_id (bx, y, bz, BT_GRASS);
										
												if (tree_dis (rnd) > 290)
													{
														// spawn tree
														if (tree_dis (rnd) < 6)
															this->gen_pine_trees.generate (w, x, y + 1, z);
														else
															this->gen_oak_trees.generate (w, x, y + 1, z);
													}
												else if (grass_dis (rnd) > 18)
													{
														if (grass_dis (rnd) == 0)
															ch->set_id (bx, y + 1, bz, BT_DANDELION);
														else
															ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
													}
											}
										else
											ch->set_id (bx, y, bz, BT_DIRT);
									}
								else
									ch->set_id (bx, y, bz, BT_DIRT);
								if (++ depth > 5)
									break;
							}
					}
			}
		};
		
		
		
		class super_overhang_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
			dgen::pine_trees gen_trees;
			
		public:
			super_overhang_biome (long s)
				: gen_trees (9, 0, {BT_TRUNK, 2}, {BT_LEAVES, 2})
			{
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
				
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 120; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->pn2.SetSeed ((s + 1) & 0x7FFFFFFF);
				this->pn3.SetSeed ((s + 2) & 0x7FFFFFFF);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
				if (y == 57)
					{
						double h = h_noise::fractal_noise_2d (this->gen_seed, x / 32.0 + 0.5, z / 32.0 + 0.5, 3, 0.56);
						if (h > 0.0 || this->generate (x, y + 1, z) > 0.0)
							return 0.2;
						return 0.0;
					}
				else if (y <= 56)
					return 0.2;
				
				return
					this->se1.GetValue (x * 0.32, y, z * 0.52) + ((60 - y) * 0.05);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
				int y, ymin = this->min_y (), depth = 0;
				
				int bx = x & 0xF, bz = z & 0xF;
				
				std::uniform_int_distribution<> tall_grass_dis (0, 256);
				std::uniform_int_distribution<> tree_dis (0, 256);
				std::uniform_int_distribution<> waterfall_dis (0, 1000);
				
				ch->set_biome (bx, bz, BI_EXTREME_HILLS);
				for (y = this->max_y (); y >= ymin; --y)
					{
						if (ch->get_id (bx, y, bz) == BT_STONE)
							{
								if (depth == 0)
									{
										if (y > 55)
											{
												ch->set_id (bx, y, bz, BT_GRASS);
												if (tall_grass_dis (rnd) > 160)
													{
														if (tall_grass_dis (rnd) < 5)
															ch->set_id (bx, y + 1, bz, BT_DANDELION);
														else
															ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
													}
												else if (y >= 83 && tree_dis (rnd) > 253)
													{
														// spawn tree
														this->gen_trees.generate (w, x, y + 1, z);
													}
											}
										else if (y == 55)
											ch->set_id (bx, y, bz, BT_SAND);
										else
											ch->set_id (bx, y, bz, BT_DIRT);
									}
								else if (depth < 5)
									ch->set_id (bx, y, bz, BT_DIRT);
								
								++ depth;
							}
						else
							depth = 0;
					}
			}
		};
	}
	
	
	
	/* 
	 * Constructs a new "experiment" world generator.
	 */
	experiment_world_generator::experiment_world_generator (long seed)
		: biome_gen (55, true, 0.1)
	{
		this->seed (seed);
		
		this->biome_gen.add (new overhang_biome (seed), 17.0);
		this->biome_gen.add (new flatlands_biome (seed), 4.0);
		this->biome_gen.add (new ocean_biome (seed), 44.0);
		this->biome_gen.add (new wasteland_biome (seed), 1.0);
		this->biome_gen.add (new desert_biome (seed), 7.0);
		this->biome_gen.add (new taiga_biome (seed), 7.0);
		this->biome_gen.add (new steppe_biome (seed), 4.0);
		this->biome_gen.add (new alps_biome (seed), 2.0);
		this->biome_gen.add (new forest_biome (seed), 7.0);
		this->biome_gen.add (new super_overhang_biome (seed), 7.0);
	}
	
	
	
	void
	experiment_world_generator::seed (long seed)
	{
		this->gen_seed = seed;
		this->biome_gen.seed (seed);
	}
	
	
	
	/* 
	 * Generates terrain on the specified chunk.
	 */
	void
	experiment_world_generator::generate (world& wr, chunk *out, int cx, int cz)
	{
		this->biome_gen.generate (wr, out, cx, cz);
	}
}

