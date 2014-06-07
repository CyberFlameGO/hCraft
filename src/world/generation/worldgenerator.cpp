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

#include "world/generation/worldgenerator.hpp"
#include "util/noise.hpp"
#include "util/utils.hpp"
#include <unordered_map>
#include <string>

#include <iostream> // DEBUG

// generators:
#include "world/generation/flatgrass.hpp"
#include "world/generation/plains.hpp"
#include "world/generation/overhang.hpp"
#include "world/generation/flatplains.hpp"
#include "world/generation/experiment.hpp"
#include "world/generation/islands.hpp"
#include "world/generation/empty.hpp"
#include "world/generation/sandbox.hpp"
#include "world/generation/super_overhang.hpp"
#include "world/generation/alpha.hpp"
#include "world/generation/test.hpp"


namespace hCraft {
	
	void
	world_generator::generate_edge (world& wr, chunk *out)
	{
		int x, y, z;
		int height = 64;
		
		for (x = 0; x < 16; ++x)
			for (z = 0; z < 16; ++z)
				{
					for (y = 0; y < height; ++y)
						out->set_id (x, y, z, BT_BEDROCK);
					out->set_id (x, y, z, BT_STILL_WATER);
					
					out->set_biome (x, z, BI_HELL);
				}
	}
	
	
	
//----
	
	static world_generator*
	create_flatgrass (long seed)
		{ return new flatgrass_world_generator (seed); }
	
	static world_generator*
	create_plains (long seed)
		{ return new plains_world_generator (seed); }
	
	static world_generator*
	create_overhang (long seed)
		{ return new overhang_world_generator (seed); }
	
	static world_generator*
	create_flatplains (long seed)
		{ return new flatplains_world_generator (seed); }
	
	static world_generator*
	create_experiment (long seed)
		{ return new experiment_world_generator (seed); }
	
	static world_generator*
	create_islands (long seed)
		{ return new islands_world_generator (seed); }
	
	static world_generator*
	create_empty (long seed)
		{ return new empty_world_generator (seed); }
		
	static world_generator*
	create_sandbox (long seed)
		{ return new sandbox_world_generator (seed); }
	
	static world_generator*
	create_super_overhang (long seed)
		{ return new super_overhang_world_generator (seed); }
  
  static world_generator*
	create_alpha (long seed)
		{ return new alpha_world_generator (seed); }
  
  static world_generator*
	create_test (long seed)
		{ return new test_world_generator (seed); }
	
	
	/* 
	 * Finds and instantiates a new world generator from the given name.
	 */
	world_generator*
	world_generator::create (const char *name, long seed)
	{
		static std::unordered_map<std::string, world_generator* (*) (long)> generators {
				{ "flatgrass", create_flatgrass },
				{ "plains", create_plains },
				{ "overhang", create_overhang },
				{ "flatplains", create_flatplains },
				{ "experiment", create_experiment },
				{ "islands", create_islands },
				{ "empty", create_empty },
				{ "sandbox", create_sandbox },
				{ "super-overhang", create_super_overhang },
				{ "test", create_test },
				{ "alpha", create_alpha },
			};
		
		auto itr = generators.find (name);
		if (itr != generators.end ())
			return itr->second (seed);
		return nullptr;
	}
	
	world_generator*
	world_generator::create (const char *name)
	{
		return world_generator::create (name,
			std::chrono::system_clock::to_time_t (
				std::chrono::system_clock::now ()) & 0xFFFFFFFF);
	}
	
	
	
//------------------------------------------------------------------------------

// the smaller this number is, the bigger biomes will get.
#define BIOME_SIZE 0.005
	
	/* 
	 * Constructs a new biome selector with the specified default water level.
	 */
	biome_selector::biome_selector (int water_level, bool bedrock, double edge_falloff)
	{
		this->water_level = water_level;
		this->edge_falloff = edge_falloff;
		this->next_start = 0.0;
		this->gen_seed = 0;
		this->bedrock = bedrock;
		
		// initialize interpolation cache
		for (int i = 0; i < 24; ++i)
			this->interp_cache[i].x = this->interp_cache[i].z = 0x7FFFFFFE;
	}
	
	/* 
	 * Destroys all biome generators held by the selector.
	 */
	biome_selector::~biome_selector ()
	{
		for (biome_info bi : this->biomes)
			delete bi.bgen;
	}
	
	
	
	/* 
	 * Inserts a new biome with the specified occurrence range.
	 */
	void
	biome_selector::add (biome_generator *bgen, double min, double max)
	{
		this->biomes.push_back ({bgen, min, max});
	}
	
	/* 
	 * Inserts a new biome with the specified occurence rate (percentage).
	 */
	void
	biome_selector::add (biome_generator *bgen, double prob)
	{
	  double start = this->next_start;
		double end   = start + (prob / 100.0);
		this->next_start = end;
		this->biomes.push_back ({bgen, start, end});
	}
	
	
	
	namespace {
		
#define RECORD_COUNT  6
		struct seed_record { double dist, x, z, val; };
		struct seed_record_list {
		  seed_record recs[RECORD_COUNT];
		  int count;
		};
		
		/* 
		 * Computes the four closest voronoi seed points.
		 */
		static seed_record_list
		_closest_voronoi_seeds (double x, double z, long seed, double freq)
		{
			x *= freq;
			z *= freq;
			
			int xi = (x > 0.0) ? (int)x : ((int)x - 1);
			int zi = (z > 0.0) ? (int)z : ((int)z - 1);

			seed_record recs[RECORD_COUNT];
			for (int i = 0; i < RECORD_COUNT; ++i)
			  recs[i] = { 2147483648.0, 0.0, 0.0, 0.0 };
		  int count = 0;
		  
			for (int zcur = zi - 2; zcur <= zi + 2; ++zcur)
				for (int xcur = xi - 2; xcur <= xi + 2; ++xcur)
					{
						double xp = xcur + h_noise::int_noise_2d (xcur, zcur, seed);
				    double zp = zcur + h_noise::int_noise_2d (xcur, zcur, seed + 1);
				    double xd = xp - x;
				    double zd = zp - z;
				    double dist = xd * xd + zd * zd;
				    
				    for (int i = 0; i < RECORD_COUNT; ++i)
				      if (dist < recs[i].dist)
				        {
				          for (int j = RECORD_COUNT - 1; j > i; --j)
				            recs[j] = recs[j - 1];
				          
				          recs[i].dist = dist;
				          recs[i].x    = xp;
				          recs[i].z    = zp;
				          
				          if (count < RECORD_COUNT)
				            ++ count;
				          break;
				        }
					}
		  
		  seed_record_list lst;
		  lst.count = count;
		  for (int i = 0; i < RECORD_COUNT; ++i)
		    lst.recs[i] = { recs[i].dist, recs[i].x, recs[i].z,
		      (h_noise::int_noise_2d (seed, utils::floor (recs[i].x), utils::floor (recs[i].z)) + 1.0) / 2.0 };
		  return lst;
		}
	}
	
	biome_generator*
	biome_selector::find_biome (double t)
	{
		for (biome_info bi : this->biomes)
			if (t < bi.end)
				return bi.bgen;
		return nullptr;
	}
	
	
	
	double
	biome_selector::get_value_2d (double x, double z)
	{
	  return 0.0;
	  /*
		auto closest = _closest_voronoi_seeds (x, z, this->gen_seed, BIOME_SIZE);
		double mdist = closest.first.dist - closest.second.dist;
		if (std::abs (mdist) < this->edge_falloff)
			{
				biome_generator *b1 = this->find_biome (closest.first.val);
				biome_generator *b2 = this->find_biome (closest.second.val);
				if (b1 == b2)
					return b1->generate (x, 0, z); // no need to interpolate between identical biomes
				
				double b1_v = b1->generate (x, 0, z);
				double b2_v = b2->generate (x, 0, z);
				
				if (b2->is_3d ())
					{
						return b1_v;
					}
				
				// interpolate
				double t = 0.5 + mdist * (0.5 / this->edge_falloff);
				return h_noise::lerp (b1_v, b2_v, t);
			}
		
		biome_generator *b = this->find_biome (closest.first.val);
		return b->generate (x, 0, z);
		*/
	}
	
	static int
	_interp_cache_index (int x, int z)
	{
		return ((unsigned int)(((2166136261 ^ (x & 0xFF) * 16777619) ^ (z & 0xFF)) * 16777619)) % 24;
	}
	
	
	double
	_sample (biome_generator *bg, double x, double y, double z)
	{
	  /*
	  double v = 0.0;
	  
	  static struct v3 { int x, y, z; }
	  _offs[] = {
	    { 0, 0, 0 },
	    { 1, 0, 0 },
	    { 0, 1, 0 },
	    { 0, 0, 1 },
	    { -1, 0, 0 },
	    { 0, -1, 0 },
	    { 0, 0, -1 },
	    { 1, 1, 1 },
	    { -1, 1, 1 },
	    { 1, -1, 1 },
	    { 1, 1, -1 },
	  };
    
    const double m = 3.0;
    for (auto vec : _offs)
      {
        v += bg->generate (x + vec.x*m, y + vec.y*m, z + vec.z*m);
      }
    
    return v / 7.0;
    */
    
    return bg->generate (x, y, z);
	}
	
	double
	biome_selector::get_value_3d (double x, double y, double z)
	{
	  auto closest = _closest_voronoi_seeds (x, z, this->gen_seed, BIOME_SIZE);
	  
	  biome_generator *b1 = this->find_biome (closest.recs[0].val);
	  
	  struct
	    {
	      double val;
	      double dist;
	    } irecs[RECORD_COUNT];
	  
	  double v = 0.0;
	  int count = 0;
	  for (int i = 1; i < closest.count; ++i)
	    {
	      seed_record rec = closest.recs[i];
	      double mdist = closest.recs[0].dist - rec.dist;
	      if (std::abs (mdist) < this->edge_falloff)
	        {
				    biome_generator *b2 = this->find_biome (rec.val);
				    
				    double b1_v = _sample (b1, x, y, z);
				    double b2_v = _sample (b2, x, y, z);
				    double t = 0.5 + mdist * (0.5 / this->edge_falloff);
				    
				    auto& irec = irecs[count++];
				    irec.val = h_noise::lerp (b1_v, b2_v, t);
				    irec.dist = std::abs (mdist);
	        }
	    }
	  
	  if (count == 0)
	    {
		    return b1->generate (x, y, z);
	    }
	  
	  // plain average
	  for (int i = 0; i < count; ++i)
	    v += irecs[i].val;
	  v /= (double)count;
	  return v;
	  
	  /*
	  // compute distance sum
	  double ds = 0.0;
	  for (int i = 0; i < count; ++i)
	    ds += irecs[i].dist;
	  
	  // interpolate
	  for (int i = 0; i < count; ++i)
	    v += (irecs[i].dist / ds) * irecs[i].val;
	  return v;
	  */
	  
	  /*
	  // TWO VERSION
	  
	  double mdist = closest.recs[0].dist - closest.recs[1].dist;
	  if (std::abs (mdist) < this->edge_falloff)
			{
				biome_generator *b1 = this->find_biome (closest.recs[0].val);
				biome_generator *b2 = this->find_biome (closest.recs[1].val);
				if (b1 == b2)
					return b1->generate (x, y, z); // no need to interpolate between identical biomes
				
				double b1_v = _sample (b1, x, y, z);
				double b2_v = _sample (b2, x, y, z);
				double t = 0.5 + mdist * (0.5 / this->edge_falloff);
				
				return h_noise::lerp (b1_v, b2_v, t);
	    }
	  
	  biome_generator *b = this->find_biome (closest.recs[0].val);
		return b->generate (x, y, z);
		*/
	  
	  /*
	  //GENERAL: BAD
	  double v = 0.0;
	  int count = 0;
	  for (int i = 0; i < closest.count; ++i)
	    {
	      seed_record rec_a = closest.recs[i];
	      for (int j = i + 1; j < closest.count; ++j)
          {
            seed_record rec_b = closest.recs[j];
            double mdist = rec_a.dist - rec_b.dist;
            if (std::abs (mdist) < this->edge_falloff)
              {
                biome_generator *b1 = this->find_biome (rec_a.val);
			          biome_generator *b2 = this->find_biome (rec_b.val);
			          if (b1 && b2)
			            {
			              double b1_v = _sample (b1, x, y, z);
			              double b2_v = _sample (b2, x, y, z);
			              double t = 0.5 + mdist * (0.5 / this->edge_falloff);
			              
			              v += h_noise::lerp (b1_v, b2_v, t);
			              ++ count;
			            }
              }
          }
	    }
	  
	  if (count == 0)
	    {
	      biome_generator *b = this->find_biome (closest.recs[0].val);
		    return b->generate (x, y, z);
	    }
	  
	  // average
	  v /= (double)count;
	  return v;
	  */
	  
	  /*
		auto closest = _closest_voronoi_seeds (x, z, this->gen_seed, BIOME_SIZE);
		double mdist = closest.first.dist - closest.second.dist;
		if (std::abs (mdist) < this->edge_falloff)
			{
				biome_generator *b1 = this->find_biome (closest.first.val);
				biome_generator *b2 = this->find_biome (closest.second.val);
				if (b1 == b2)
					return b1->generate (x, y, z); // no need to interpolate between identical biomes
				
				double b1_v = _sample (b1, x, y, z);
				double b2_v = _sample (b2, x, y, z);
				double t = 0.5 + mdist * (0.5 / this->edge_falloff);
				
				// interpolating between 2d and 3d biomes is a bit more involved...
				if (!b2->is_3d ())
					{
						double t2 = std::abs (mdist) * (1.0 / this->edge_falloff);
				
						// get height
						double h = y;
						
						// try to retrieve the height from our cache
						unsigned int index = _interp_cache_index (x, z);
						internal::interp_entry ent = this->interp_cache[index];
						if (ent.x == x && ent.z == z)
							h = ent.h;
						else
							{
								// calculate it.. :X
								// this is a very expensive operation
								for (int i = y; i <= 120; ++i)
									if (b1->generate (x, i, z) > 0.0)
										h = i;
								
								this->interp_cache[index] = {(int)x, (int)z, h};
							}
						
						double h2 = h_noise::lerp (b2_v, h, t2);
						if (y > h2)
							return 0.0;
						else if (y < h2)
							return 1.0;
						
						return b1_v;
					}
				
				// interpolate
				return h_noise::lerp (b1_v, b2_v, t);
			}
		
		biome_generator *b = this->find_biome (closest.first.val);
		return b->generate (x, y, z);
		*/
	}
	
	
	static void
	_empty_gen (world &w, chunk *ch, int cx, int cz, int water_level, bool bedrock)
	{
		int x, y, z;
		for (x = 0; x < 16; ++x)
			for (z = 0; z < 16; ++z)
				{
					y = 0;
					if (bedrock)
						ch->set_id (x, y++, z, BT_BEDROCK);
					for (; y <= water_level; ++y)
						ch->set_id (x, y, z, BT_WATER);
				}
	}
	
	/* 
	 * Uses the selector's biomes to generate terrain on the given chunk.
	 */
	void
	biome_selector::generate (world &w, chunk *ch, int cx, int cz)
	{
		const int water_level = this->water_level;
		const bool bedrock    = this->bedrock;
		int x, z, xx, zz, h, y, ymin, ymax;
		biome_generator *b;
		
		std::minstd_rand rnd ((this->gen_seed + (cx * 21149) + (cz * 63761)));
		
		if (this->biomes.empty ())
			{
				_empty_gen (w, ch, cx, cz, water_level, bedrock);
				return;
			}
		
		for (x = 0; x < 16; ++x)
			for (z = 0; z < 16; ++z)
				{
					// world coordinates
					xx = (cx << 4) | x;
					zz = (cz << 4) | z;
					
					b = this->find_biome (
						_closest_voronoi_seeds (xx, zz, this->gen_seed, BIOME_SIZE).recs[0].val);
					if (b->is_3d ())
						{
							ymin = b->min_y ();
							ymax = b->max_y ();
							y    = 0;
							
							if (bedrock)
								ch->set_id (x, y++, z, BT_BEDROCK);
							for (; y < ymin; ++y)
								ch->set_id (x, y, z, BT_STONE);
							
							for (; y < ymax; ++y)
								{
									if (this->get_value_3d (xx, y, zz) > 0.0)
										ch->set_id (x, y, z, BT_STONE);
									else if (y <= water_level)
										ch->set_id (x, y, z, BT_WATER);
								}
							
							b->decorate (w, ch, xx, zz, rnd);
						}
					else
						{
							h = this->get_value_2d (xx, zz);
							y = 0;
							
							if (bedrock)
								ch->set_id (x, y++, z, BT_BEDROCK);
							
							for (; y < h; ++y)
								ch->set_id (x, y, z, BT_STONE);
							for (; y <= water_level; ++y)
								ch->set_id (x, y, z, BT_WATER);
							
							b->decorate (w, ch, xx, zz, rnd);
						}
				}
	}
	
	
	
	/* 
	 * Seeds the internal generators used by the biome selector.
	 */
	void
	biome_selector::seed (long seed)
	{
		this->gen_seed = seed;
		
		for (biome_info bi : this->biomes)
		  {
		    bi.bgen->seed (seed);
		  }
	}
}

