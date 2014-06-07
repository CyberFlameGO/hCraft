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

#include "world/generation/test.hpp"
#include <noise/noise.h>


namespace hCraft {
  
  namespace {
		
		class A_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
		public:
			A_biome (long s)
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
				
				this->pn1.SetSeed (s & 0x7FFFFFFF);
				this->pn2.SetSeed ((s + 1) & 0x7FFFFFFF);
				this->pn3.SetSeed ((s + 2) & 0x7FFFFFFF);
			}
			
			virtual double
			generate (int x, int y, int z)
			{
			  return 1.0 - 2.0*(y / 128.0);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_JUNGLE);
			  for (int y = 150; y >= 0; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE &&
                ch->get_id (bx, y + 1, bz) == BT_AIR)
			        ch->set_id (bx, y, bz, BT_GRASS);
			    }
			}
		};
    
    
    
    class B_biome: public biome_generator
		{
			long gen_seed;
			
			noise::module::Perlin pn1, pn2, pn3;
			noise::module::Const co1;
			noise::module::Multiply mu1;
			noise::module::Select se1;
			
		public:
			B_biome (long s)
			{
				this->pn1.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn1.SetFrequency (0.024);
				this->pn1.SetPersistence (0.45);
				this->pn1.SetLacunarity (1.7);
				this->pn1.SetOctaveCount (2);
		
				this->co1.SetConstValue (16.2);
				this->mu1.SetSourceModule (0, this->pn1);
				this->mu1.SetSourceModule (1, this->co1);
				
				this->pn2.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn2.SetFrequency (0.05);
				this->pn2.SetPersistence (0.11);
				this->pn2.SetLacunarity (0.43);
				this->pn2.SetOctaveCount (2);
				
				this->pn3.SetNoiseQuality (noise::QUALITY_FAST);
				this->pn3.SetFrequency (0.08);
				this->pn3.SetPersistence (0.9);
				this->pn3.SetLacunarity (0.66);
				this->pn3.SetOctaveCount (2);
		
				this->se1.SetSourceModule (1, this->mu1);
				this->se1.SetSourceModule (0, this->pn2);
				this->se1.SetControlModule (this->pn3);
				this->se1.SetBounds (-1.0, -0.1);
				this->se1.SetEdgeFalloff (0.3);
				
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
			  //return (y < 64) ? 1.0 : -1.0;
				return
					this->se1.GetValue (x * 0.5, y, z * 0.7) + ((75 - y) * 0.07);
			}
			
			virtual void
			decorate (world &w, chunk *ch, int x, int z, std::minstd_rand& rnd)
			{
			  int bx = x & 0xF;
			  int bz = z & 0xF;
			  
			  ch->set_biome (bx, bz, BI_EXTREME_HILLS);
			  for (int y = 150; y >= 0; --y)
			    {
			      if (ch->get_id (bx, y, bz) == BT_STONE &&
                ch->get_id (bx, y + 1, bz) == BT_AIR)
			        ch->set_id (bx, y, bz, BT_GRASS);
			    }
			}
		};
  }
  
  
  /* 
	 * Constructs a new "test" world generator.
	 */
	test_world_generator::test_world_generator (long seed)
		: biome_gen (1, true, 0.25)
	{
		this->seed (seed);
		
		this->biome_gen.add (new A_biome (seed), 50.0);
		this->biome_gen.add (new B_biome (seed), 50.0);
	}
	
	
	
	void
	test_world_generator::seed (long seed)
	{
		this->gen_seed = seed;
		this->biome_gen.seed (seed);
	}
	
	
	
	/* 
	 * Generates terrain on the specified chunk.
	 */
	void
	test_world_generator::generate (world& wr, chunk *out, int cx, int cz)
	{
		this->biome_gen.generate (wr, out, cx, cz);
	}
}

