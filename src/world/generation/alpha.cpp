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

#include "world/generation/alpha.hpp"
#include "world/generation/detail/trees.hpp"
#include <noise/noise.h>


namespace hCraft {
  
  namespace {
    
    class ocean_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1;
			
		public:
			ocean_biome (long s)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.03);
				this->pn1.SetPersistence (0.4);
				this->pn1.SetLacunarity (1.6);
				this->pn1.SetOctaveCount (2);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return 1.0 - y/(48.0 + 8.0*this->pn1.GetValue (x, z, 0));
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  int depth = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ depth;
			          if (depth < 6)
			            {
			              if (y >= 64)
			                ch->set_id (bx, y, bz, BT_SAND);
			              else
			                ch->set_id (bx, y, bz, BT_DIRT);
			            }
			        }
			      else
			        depth = 0;
			    }
			}
		};
		
		
		
		class desert_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1;
			
		public:
			desert_biome (long s)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.016);
				this->pn1.SetPersistence (0.3);
				this->pn1.SetLacunarity (1.2);
				this->pn1.SetOctaveCount (2);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return 1.0 - y/(70.0 + 4.0*this->pn1.GetValue (x, z, 0));
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  int counter = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          if (counter++ < 6)
			            {
			              ch->set_id (bx, y, bz, BT_SAND);
			              if (y > 64 && counter == 1)
			                {
			                  // plant a cactus
			                  std::uniform_int_distribution<> dis (0, 240);
			                  if (dis (rnd) == 0)
			                    {
			                      // plant a cactus
			                      std::uniform_int_distribution<> hdis (0, 2);
			                      int h = 1 + hdis (rnd);
			                      for (int i = 0; i < h; ++i)
			                        ch->set_id (bx, y + i + 1, bz, BT_CACTUS);
			                    }
			                  else if (dis (rnd) == 1)
			                    {
			                      ch->set_id (bx, y + 1, bz, BT_DEAD_BUSH);
			                    }
			                }
			            }
			        }
			    }
			}
		};
		
		/* 
		 * A mountainous variant of the desert biome. 
		 */
		class desert_m_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
		public:
			desert_m_biome (long s)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.023);
				this->pn1.SetPersistence (0.67);
				this->pn1.SetLacunarity (1.9);
				this->pn1.SetOctaveCount (2);
		
				this->co1.SetConstValue (5.6);
				this->mu1.SetSourceModule (0, this->pn1);
				this->mu1.SetSourceModule (1, this->co1);
				
				this->pn2.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn2.SetFrequency (0.03);
				this->pn2.SetPersistence (0.18);
				this->pn2.SetLacunarity (0.5);
				this->pn2.SetOctaveCount (2);
				
				this->pn3.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn3.SetFrequency (0.05);
				this->pn3.SetPersistence (0.8);
				this->pn3.SetLacunarity (1.5);
				this->pn3.SetOctaveCount (2);
		
				this->se1.SetSourceModule (1, this->mu1);
				this->se1.SetSourceModule (0, this->pn2);
				this->se1.SetControlModule (this->pn3);
				this->se1.SetBounds (0.45, 1.0);
				this->se1.SetEdgeFalloff (0.1);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
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
			  return
					this->se1.GetValue (x * 0.5, y, z * 0.7) + ((78 - y) * 0.09);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  int depth = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ depth;
			          if (depth < 3)
			            ch->set_id (bx, y, bz, BT_SAND);
			        }
			      else
			        depth = 0;
			    }
			}
		};
		
		
		
		class extreme_hills_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
			dgen::generic_trees oak_trees;
			
		public:
			extreme_hills_biome (long s)
			  : oak_trees (5)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.023);
				this->pn1.SetPersistence (0.67);
				this->pn1.SetLacunarity (1.9);
				this->pn1.SetOctaveCount (2);
		
				this->co1.SetConstValue (12.6);
				this->mu1.SetSourceModule (0, this->pn1);
				this->mu1.SetSourceModule (1, this->co1);
				
				this->pn2.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn2.SetFrequency (0.021);
				this->pn2.SetPersistence (0.34);
				this->pn2.SetLacunarity (0.8);
				this->pn2.SetOctaveCount (2);
				
				this->pn3.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn3.SetFrequency (0.05);
				this->pn3.SetPersistence (0.8);
				this->pn3.SetLacunarity (1.5);
				this->pn3.SetOctaveCount (2);
		
				this->se1.SetSourceModule (1, this->mu1);
				this->se1.SetSourceModule (0, this->pn2);
				this->se1.SetControlModule (this->pn3);
				this->se1.SetBounds (0.0, 1.0);
				this->se1.SetEdgeFalloff (0.1);
		    
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
				this->oak_trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return
					this->se1.GetValue (x * 0.5, y, z * 0.7) + ((70 - y) * 0.09);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_EXTREME_HILLS);
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 300);
			  
			  int depth = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ depth;
			          if (depth == 1)
			            {
			              ch->set_id (bx, y, bz, BT_GRASS);
			              if (y > 64)
			                {
			                  if (dis1 (rnd) < 5)
			                    ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
			                  else if (dis2 (rnd) == 0)
			                    this->oak_trees.generate (w, x, y + 1, z);
			                }
			            }
			          else if (depth < 5)
			            ch->set_id (bx, y, bz, BT_DIRT);
			        }
			      else
			        depth = 0;
			    }
			}
		};
		
		
		
		class taiga_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1;
			
			dgen::pine_trees trees;
			
		public:
			taiga_biome (long s)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.02);
				this->pn1.SetPersistence (0.7);
				this->pn1.SetLacunarity (2.0);
				this->pn1.SetOctaveCount (2);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return 1.0 - y/(70.0 + 8.0*this->pn1.GetValue (x, z, 0));
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_TAIGA);
			  
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 120);
			  
			  int counter = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ counter;
			          if (counter == 1)
			            {
			              ch->set_id (bx, y, bz, BT_GRASS);
			              if (y >= 64)
			                {
			                  if (dis1 (rnd) < 10)
			                    ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 2);
			                  else if (dis2 (rnd) < 2)
			                    this->trees.generate (w, x, y + 1, z);
			                }
			            }
			          else if (counter < 6)
			            ch->set_id (bx, y, bz, BT_DIRT);
			        }
			    }
			}
		};
		
		
		
		class taiga_m_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
			dgen::pine_trees trees;
			
		public:
			taiga_m_biome (long s)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
		  	this->pn1.SetFrequency (0.03);
			  this->pn1.SetPersistence (0.67);
			  this->pn1.SetLacunarity (2.9);
			  this->pn1.SetOctaveCount (2);
	
			  this->co1.SetConstValue (4.0);
			  this->mu1.SetSourceModule (0, this->pn1);
			  this->mu1.SetSourceModule (1, this->co1);
			
			  this->pn2.SetNoiseQuality (noise::QUALITY_FAST);
			  this->pn2.SetFrequency (0.04);
			  this->pn2.SetPersistence (0.68);
			  this->pn2.SetLacunarity (1.3);
			  this->pn2.SetOctaveCount (2);
			
			  this->pn3.SetNoiseQuality (noise::QUALITY_FAST);
			  this->pn3.SetFrequency (0.02);
			  this->pn3.SetPersistence (0.6);
			  this->pn3.SetLacunarity (1.9);
			  this->pn3.SetOctaveCount (2);
	
			  this->se1.SetSourceModule (1, this->mu1);
			  this->se1.SetSourceModule (0, this->pn2);
			  this->se1.SetControlModule (this->pn3);
			  this->se1.SetBounds (0.3, 1.0);
			  this->se1.SetEdgeFalloff (0.05);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->pn2.SetSeed ((s + 1) & 0x7FFFFFFF);
				this->pn3.SetSeed ((s + 2) & 0x7FFFFFFF);
				this->trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return
					this->se1.GetValue (x * 0.8, y, z * 0.8) + ((80 - y) * 0.15);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_TAIGA);
			  
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 120);
			  
			  int counter = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ counter;
			          
			          if (counter == 1)
			            {
			              ch->set_id (bx, y, bz, BT_GRASS);
			              if (y > 64)
			                {
			                  if (dis1 (rnd) < 10)
			                    ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 2);
			                  else if (dis2 (rnd) < 2)
			                    this->trees.generate (w, x, y + 1, z);
			                }
			            }
			          else if (counter < 6)
			            ch->set_id (bx, y, bz, BT_DIRT);
			        }
			      else
			        counter = 0;
			    }
			}
		};
		
		
		
		class plains_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1;
			
			dgen::generic_trees trees;
			
		public:
			plains_biome (long s)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.012);
				this->pn1.SetPersistence (0.7);
				this->pn1.SetLacunarity (2.0);
				this->pn1.SetOctaveCount (2);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return 1.0 - y/(70.0 + 3.0*this->pn1.GetValue (x, z, 0));
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_PLAINS);
			  
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 800);
			  
			  int counter = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ counter;
			          if (counter == 1)
			            {
			              ch->set_id (bx, y, bz, BT_GRASS);
			              if (y > 64)
			                {
			                  if (dis1 (rnd) < 30)
			                    ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
			                  else if (dis2 (rnd) == 0)
			                    this->trees.generate (w, x, y + 1, z);
			                }
			            }
			          else if (counter < 6)
			            ch->set_id (bx, y, bz, BT_DIRT);
			        }
			      else
			        counter = 0;
			    }
			}
		};
		
		
		
		class forest_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1;
			
			dgen::generic_trees oak_trees, birch_trees;
			
		public:
			forest_biome (long s)
			  : oak_trees (4, {BT_TRUNK, 0}, {BT_LEAVES, 0}),
			    birch_trees (4, {BT_TRUNK, 2}, {BT_LEAVES, 2})
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.012);
				this->pn1.SetPersistence (0.7);
				this->pn1.SetLacunarity (2.0);
				this->pn1.SetOctaveCount (2);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->oak_trees.seed (s);
				this->birch_trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return this->pn1.GetValue (x, y, z) + ((78 - y) * 0.06);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_FOREST);
			  
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 45);
			  std::uniform_int_distribution<> dis3 (1, 5);
			  
			  int counter = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ counter;
			          if (counter == 1)
			            {
			              ch->set_id (bx, y, bz, BT_GRASS);
			              if (y > 64)
			                {
			                  if (dis1 (rnd) < 7)
			                    ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
			                  else if (dis2 (rnd) == 0)
			                    {
			                      if (dis3 (rnd) == 1)
			                        this->birch_trees.generate (w, x, y + 1, z);
			                      else
			                        this->oak_trees.generate (w, x, y + 1, z);
			                    }
			                }
			            }
			        }
			      else
			        counter = 0;
			    }
			}
		};
		
		
		
		class ice_plains_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1;
			
			dgen::pine_trees trees;
			
		public:
			ice_plains_biome (long s)
			  : trees (7, 100)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.017);
				this->pn1.SetPersistence (0.7);
				this->pn1.SetLacunarity (2.5);
				this->pn1.SetOctaveCount (2);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return 1.0 - y/(66.0 + 5.0*this->pn1.GetValue (x * 0.6, z * 0.6, 0));
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_ICE_PLAINS);
			 
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 1200);
			  
			  unsigned short id;
			  int depth = 0;
			  enum
			    {
			      ST_STONE,
			      ST_WATER,
			    } state = ST_STONE;
			  
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      id = ch->get_id (bx, y, bz);
			      if (id == BT_STONE)
			        {
			          if (state != ST_STONE)
			            {
			              depth = 0;
			              state = ST_STONE;
			            }
			          
			          ++ depth;
			          if (depth == 1)
			            {
			              ch->set_id (bx, y, bz, BT_GRASS);
			              if (y >= 64)
			                {
			                  if (dis2 (rnd) == 0)
			                    this->trees.generate (w, x, y + 1, z);
			                  else
			                    ch->set_id (bx, y + 1, bz, BT_SNOW_COVER);
			                }
			            }
			          else if (depth < 6)
			            ch->set_id (bx, y, bz, BT_DIRT);
			        }
			      else if (id == BT_WATER)
			        {
			          if (state != ST_WATER)
			            {
			              depth = 0;
			              state = ST_WATER;
			            }
			          
			          ++ depth;
			          if (depth == 1)
			            ch->set_id (bx, y, bz, BT_ICE);
			        }
			    }
			}
		};
		
		
		
		class awesome1_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
			dgen::generic_trees trees1, trees2;
			dgen::pine_trees trees3;
			
		public:
			awesome1_biome (long s)
			  : trees2 (4, {BT_TRUNK, 2}, {BT_LEAVES, 2})
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
		  	this->pn1.SetFrequency (0.02);
			  this->pn1.SetPersistence (0.61);
			  this->pn1.SetLacunarity (2.9);
			  this->pn1.SetOctaveCount (2);
	
			  this->co1.SetConstValue (5.7);
			  this->mu1.SetSourceModule (0, this->pn1);
			  this->mu1.SetSourceModule (1, this->co1);
			
			  this->pn2.SetNoiseQuality (noise::QUALITY_FAST);
			  this->pn2.SetFrequency (0.04);
			  this->pn2.SetPersistence (0.68);
			  this->pn2.SetLacunarity (1.3);
			  this->pn2.SetOctaveCount (2);
			
			  this->pn3.SetNoiseQuality (noise::QUALITY_FAST);
			  this->pn3.SetFrequency (0.02);
			  this->pn3.SetPersistence (0.6);
			  this->pn3.SetLacunarity (1.9);
			  this->pn3.SetOctaveCount (2);
	
			  this->se1.SetSourceModule (1, this->mu1);
			  this->se1.SetSourceModule (0, this->pn2);
			  this->se1.SetControlModule (this->pn3);
			  this->se1.SetBounds (0.0, 1.0);
			  this->se1.SetEdgeFalloff (0.05);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->pn2.SetSeed ((s + 1) & 0x7FFFFFFF);
				this->pn3.SetSeed ((s + 2) & 0x7FFFFFFF);
				this->trees1.seed (s);
				this->trees2.seed (s);
				this->trees3.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return
					this->se1.GetValue (x * 0.5, y, z * 0.5) + ((70 - y) * 0.08);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_FOREST);
			  
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 120);
			  
			  int counter = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ counter;
			          
			          if (counter == 1)
			            {
			              if (y < 63)
			                ch->set_id (bx, y, bz, BT_DIRT);
			              else if (y < 66)
			                ch->set_id (bx, y, bz, BT_SAND);
			              else
			                ch->set_id (bx, y, bz, BT_GRASS);
			              
			              if (y >= 66)
			                {
			                  if (dis1 (rnd) < 45)
			                    {
			                      if (dis1 (rnd) < 80)
			                        ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 2);
			                      else
			                        ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
			                    }
			                  else if (dis2 (rnd) < 2)
			                    {
			                      int t = dis2 (rnd);
			                      if (t < 100)
			                        this->trees1.generate (w, x, y + 1, z);
			                      else if (t < 115)
			                        this->trees2.generate (w, x, y + 1, z);
			                      else
			                        this->trees3.generate (w, x, y + 1, z);
			                    }
			                }
			            }
			          else if (counter < 6)
			            ch->set_id (bx, y, bz, BT_DIRT);
			        }
			      else
			        counter = 0;
			    }
			}
		};
		
		
		
		class awesome2_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
			dgen::generic_trees trees1, trees2;
			
		public:
			awesome2_biome (long s)
			  : trees1 (5),
			    trees2 (5, {BT_TRUNK, 2}, {BT_LEAVES, 2})
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
		  	this->pn1.SetFrequency (0.02);
			  this->pn1.SetPersistence (0.61);
			  this->pn1.SetLacunarity (2.9);
			  this->pn1.SetOctaveCount (2);
	
			  this->co1.SetConstValue (8.3);
			  this->mu1.SetSourceModule (0, this->pn1);
			  this->mu1.SetSourceModule (1, this->co1);
			
			  this->pn2.SetNoiseQuality (noise::QUALITY_FAST);
			  this->pn2.SetFrequency (0.035);
			  this->pn2.SetPersistence (0.78);
			  this->pn2.SetLacunarity (1.5);
			  this->pn2.SetOctaveCount (2);
			
			  this->pn3.SetNoiseQuality (noise::QUALITY_FAST);
			  this->pn3.SetFrequency (0.018);
			  this->pn3.SetPersistence (0.7);
			  this->pn3.SetLacunarity (1.4);
			  this->pn3.SetOctaveCount (2);
	
			  this->se1.SetSourceModule (1, this->mu1);
			  this->se1.SetSourceModule (0, this->pn2);
			  this->se1.SetControlModule (this->pn3);
			  this->se1.SetBounds (-0.1, 1.0);
			  this->se1.SetEdgeFalloff (0.05);
		    
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
				this->trees1.seed (s);
				this->trees2.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return
					this->se1.GetValue (x * 0.35, y, z * 0.35) + ((70 - y) * 0.068);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_JUNGLE);
			  
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 120);
			  
			  int counter = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ counter;
			          
			          if (counter == 1)
			            {
			              if (y < 63)
			                ch->set_id (bx, y, bz, BT_DIRT);
			              else if (y < 66)
			                ch->set_id (bx, y, bz, BT_GRAVEL);
			              else
			                ch->set_id (bx, y, bz, BT_GRASS);
			              
			              if (y >= 66)
			                {
			                  if (dis1 (rnd) < 60)
			                    ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
			                  else if (dis2 (rnd) < 2)
			                    {
			                      int t = dis2 (rnd);
			                      if (t < 80)
			                        this->trees1.generate (w, x, y + 1, z);
			                      else
			                        this->trees2.generate (w, x, y + 1, z);
			                    }
			                }
			            }
			          else if (counter < 6)
			            ch->set_id (bx, y, bz, BT_DIRT);
			        }
			      else
			        counter = 0;
			    }
			}
		};
		
		
		
		class swamp_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1;
			
			dgen::generic_trees trees;
			
		public:
			swamp_biome (long s)
			  : trees (5)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.042);
				this->pn1.SetPersistence (0.6);
				this->pn1.SetLacunarity (2.0);
				this->pn1.SetOctaveCount (2);
		    
				this->seed (s);
			}
			
			virtual bool is_3d () override { return true; }
			virtual int min_y () { return 40; }
			virtual int max_y () { return 100; }
		
			virtual void
			seed (long s)
			{
				this->gen_seed = s;
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->trees.seed (s);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return 1.0 - y/(64.0 + 2.0*this->pn1.GetValue (x, z, 0));
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_SWAMPLAND);
			  
			  std::uniform_int_distribution<> dis1 (0, 120);
			  std::uniform_int_distribution<> dis2 (0, 120);
			  
			  int counter = 0;
			  for (int y = this->max_y (); y >= 1; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE)
			        {
			          ++ counter;
			          if (counter == 1)
			            {
			              if (y >= 64)
			                {
			                  ch->set_id (bx, y, bz, BT_GRASS);
			                  if (dis1 (rnd) < 7)
			                    {
			                      int t = dis1 (rnd) % 10;
			                      if (t == 0)
			                        ch->set_id (bx, y + 1, bz, BT_RED_MUSHROOM);
			                      else if (t == 1)
			                        ch->set_id (bx, y + 1, bz, BT_BROWN_MUSHROOM);
			                      else
			                        ch->set_block (bx, y + 1, bz, BT_TALL_GRASS, 1);
			                    }
			                  else if (dis2 (rnd) == 0)
			                    this->trees.generate (w, x, y + 1, z);
			                }
			              else
			                ch->set_id (bx, y, bz, BT_DIRT);
			            }
			          else if (counter < 6)
			            ch->set_id (bx, y, bz, BT_DIRT);
			        }
			      else
			        counter = 0;
			    }
			}
		};
  }
  
  
  
  /* 
	 * Constructs a new "alpha" world generator.
	 */
	alpha_world_generator::alpha_world_generator (long seed)
		: biome_gen (64, true, 0.25)
	{
		this->seed (seed);
		
		this->biome_gen.add (new ocean_biome (seed), 20.0);
		this->biome_gen.add (new desert_biome (seed), 7.2);
		this->biome_gen.add (new desert_m_biome (seed), 7.2);
		this->biome_gen.add (new extreme_hills_biome (seed), 7.2);
		this->biome_gen.add (new taiga_biome (seed), 7.2);
		this->biome_gen.add (new taiga_m_biome (seed), 7.2);
		this->biome_gen.add (new plains_biome (seed), 7.2);
		this->biome_gen.add (new forest_biome (seed), 7.2);
		this->biome_gen.add (new ice_plains_biome (seed), 7.2);
		this->biome_gen.add (new awesome1_biome (seed), 7.2);
		this->biome_gen.add (new awesome2_biome (seed), 7.2);
		this->biome_gen.add (new swamp_biome (seed), 8.0);
	}
	
	
	
	void
	alpha_world_generator::seed (long seed)
	{
		this->gen_seed = seed;
		this->biome_gen.seed (seed);
	}
	
	
	
	/* 
	 * Generates terrain on the specified chunk.
	 */
	void
	alpha_world_generator::generate (world& wr, chunk *out, int cx, int cz)
	{
		this->biome_gen.generate (wr, out, cx, cz);
	} 
}

