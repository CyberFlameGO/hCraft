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

#include "noise.hpp"
#include <cmath>

#include <iostream> // DEBUG
#include <random>   // DEBUG


namespace hCraft {
	
	namespace noise {
		
		/* 
		 * Linear interpolation.
		 */
		double
		lerp (double a, double b, double t)
		{
			return a + t * (b - a);
		}
		
		
		
		/* 
		 * Basic noise.
		 */
		
		double
		int_noise_1d (int seed, int n)
		{
			n = (n + seed * 4986329) ^ seed;
			n = (n << 13) ^ n;
		  return (1.0 - ((n * (n * n * 20011 + 999233) + 1876917607) & 0x7FFFFFFF) / 1073741824.0);   
		}
		
		double
		int_noise_2d (int seed, int x, int y)
		{
			return int_noise_1d (seed, x + y * 1698857);
		}
		
		double
		int_noise_3d (int seed, int x, int y, int z)
		{
			return int_noise_2d (seed, x + y * 16673, z);
		}
		
		
		
		/* 
		 * Perlin noise.
		 */
		 
		namespace {
		  
		  struct vec_2d { double x; double y; };
		  struct vec_3d { double x; double y; double z; };
		  
		  // 2d vector dot product
		  static inline double
		  dotp_2d (vec_2d a, vec_2d b)
		  {
	  	  return a.x * b.x + a.y * b.y; 
      }
      
      // 3d vector dot product
		  static inline double
		  dotp_3d (vec_3d a, vec_3d b)
		  {
	  	  return a.x * b.x + a.y * b.y + a.z * b.z; 
      }
      
      
      // two-dimensional gradient of length one.
		  static vec_2d
		  grad_2d (int seed, int x, int y)
		  {
		    vec_2d v { int_noise_2d (seed, x + 101, y + 101), int_noise_2d (seed, x + 303, y + 303) };
		    double m = std::sqrt (dotp_2d (v, v));
		    return { v.x / m, v.y / m }; 
		  }
		 	
		  // three-dimensional gradient of length one.
		  static vec_3d
		  grad_3d (int seed, int x, int y, int z)
		  {
		    vec_3d v {
		    	int_noise_3d (seed, x + 101, y + 101, z + 101),
		    	int_noise_3d (seed, x + 303, y + 303, z + 303),
		    	int_noise_3d (seed, x + 505, y + 505, z + 505) };
		    double m = std::sqrt (dotp_3d (v, v));
		    return { v.x / m, v.y / m, v.z / m }; 
		  }
		  
      
      // ease curve
      static inline double
      ecurve (double p)
      {
        return 3*p*p - 2*p*p*p;
      }
		}
		
		
		
		double
		perlin_noise_2d (int seed, double x, double y)
		{
		  // grid points
		  int x0 = std::floor (x);
		  int x1 = x0 + 1;
		  int y0 = std::floor (y);
		  int y1 = y0 + 1;
		  
		  // influences
		  double in00 = dotp_2d (grad_2d (seed, x0, y0), {x - x0, y - y0});
		  double in10 = dotp_2d (grad_2d (seed, x1, y0), {x - x1, y - y0});
		  double in01 = dotp_2d (grad_2d (seed, x0, y1), {x - x0, y - y1});
		  double in11 = dotp_2d (grad_2d (seed, x1, y1), {x - x1, y - y1});
		  
		  // weights
		  double sx = ecurve (x - x0);
		  double sy = ecurve (y - y0);
		  
		  double a = lerp (in00, in10, sx);
		  double b = lerp (in01, in11, sx);
		  return lerp (a, b, sy);
		}
		
		double
		perlin_noise_3d (int seed, double x, double y, double z)
		{
		  // grid points
		  int x0 = std::floor (x);
		  int x1 = x0 + 1;
		  int y0 = std::floor (y);
		  int y1 = y0 + 1;
		  int z0 = std::floor (z);
		  int z1 = z0 + 1;
		  
		  // influences
		  double in000 = dotp_3d (grad_3d (seed, x0, y0, z0), {x - x0, y - y0, z - z0});
		  double in100 = dotp_3d (grad_3d (seed, x1, y0, z0), {x - x1, y - y0, z - z0});
		  double in010 = dotp_3d (grad_3d (seed, x0, y1, z0), {x - x0, y - y1, z - z0});
		  double in110 = dotp_3d (grad_3d (seed, x1, y1, z0), {x - x1, y - y1, z - z0});
		  double in001 = dotp_3d (grad_3d (seed, x0, y0, z1), {x - x0, y - y0, z - z1});
		  double in101 = dotp_3d (grad_3d (seed, x1, y0, z1), {x - x1, y - y0, z - z1});
		  double in011 = dotp_3d (grad_3d (seed, x0, y1, z1), {x - x0, y - y1, z - z1});
		  double in111 = dotp_3d (grad_3d (seed, x1, y1, z1), {x - x1, y - y1, z - z1});
		  
		  // weights
		  double sx = ecurve (x - x0);
		  double sy = ecurve (y - y0);
		  double sz = ecurve (z - z0);
		  
		  double a = lerp (in000, in100, sx);
		  double b = lerp (in010, in110, sx);
		  double c = lerp (in001, in101, sx);
		  double d = lerp (in011, in111, sx);
		  
		  double e = lerp (a, b, sy);
		  double f = lerp (c, d, sy);
		  return lerp (e, f, sz);
		}
		
		
		
		/* 
		 * Fractal noise.
		 */
		
		double
		fractal_noise_2d (int seed, double x, double y, int oct, double persist)
		{
		  double total = 0.0;
		  double freq = 1.0, amp = 1.0;
		  
		  for (int i = 0; i < oct; ++i)
		    {
		      total += perlin_noise_2d (seed, x * freq, y * freq) * amp;
		      
		      freq *= 2;
		      amp  *= persist;
		    }
		  
		  return total;
		}
		
		double
		fractal_noise_3d (int seed, double x, double y, double z, int oct, double persist)
		{
		  double total = 0.0;
		  double freq = 1.0, amp = 1.0;
		  
		  for (int i = 0; i < oct; ++i)
		    {
		      total += perlin_noise_3d (seed, x * freq, y * freq, z * freq) * amp;
		      
		      freq *= 2;
		      amp  *= persist;
		    }
		  
		  return total;
		}
	}
}

