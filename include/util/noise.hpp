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

#ifndef _hCraft__NOISE_H_
#define _hCraft__NOISE_H_


namespace hCraft {
	
	/* 
	 * Our own noise functions.
	 */
	namespace h_noise {
		
		/* 
		 * Linear interpolation.
		 */
		double lerp (double a, double b, double t);
		
		
		/* 
		 * Basic noise.
		 */
		double int_noise_1d (int seed, int n);
		double int_noise_2d (int seed, int x, int y);
		double int_noise_3d (int seed, int x, int y, int z);
		
		
		
		/* 
		 * Perlin noise.
		 */
		double perlin_noise_2d (int seed, double x, double y);
		double perlin_noise_3d (int seed, double x, double y, double z);
		
		
		
		/* 
		 * Fractal noise.
		 */
		double fractal_noise_2d (int seed, double x, double y, int oct, double persist);
		double fractal_noise_3d (int seed, double x, double y, double z, int oct, double persist);
	}
}

#endif

