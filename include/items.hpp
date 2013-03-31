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

#ifndef _hCraft__ITEMS_H_
#define _hCraft__ITEMS_H_


namespace hCraft {
	
	/* 
	 * Item IDs.
	 */
	enum item_type: unsigned short
	{
		IT_UNKNOWN = 0xFFFF,
		
		IT_IRON_SHOVEL = 0x100,
		IT_IRON_PICKAXE,
		IT_IRON_AXE,
		IT_FLINT_AND_STEEL,
		IT_APPLE,
		IT_BOW,
		IT_ARROW,
		IT_COAL,
		IT_DIAMOND,
		IT_IRON_INGOT,
		IT_GOLD_INGOT,
		IT_IRON_SWORD,
		IT_WOODEN_SWORD,
		IT_WOODEN_SHOVEL,
		IT_WOODEN_PICKAXE,
		IT_WOODEN_AXE,
		IT_STONE_SWORD,
		IT_STONE_SHOVEL,
		IT_STONE_PICKAXE,
		IT_STONE_AXE,
		IT_DIAMOND_SWORD,
		IT_DIAMOND_SHOVEL,
		IT_DIAMOND_PICKAXE,
		IT_DIAMOND_AXE,
		IT_STICK,
		IT_BOWL,
		IT_MUSHROOM_STEW,
		IT_GOLDEN_SWORD,
		IT_GOLDEN_SHOVEL,
		IT_GOLDEN_PICKAXE,
		IT_GOLDEN_AXE,
		IT_STRING,
		IT_FEATHER,
		IT_GUNPOWDER,
		IT_WOODEN_HOE,
		IT_STONE_HOE,
		IT_IRON_HOE,
		IT_DIAMOND_HOE,
		IT_GOLD_HOE,
		IT_SEEDS,
		IT_WHEAT,
		IT_BREAD,
		IT_LEATHER_CAP,
		IT_LEATHER_TUNIC,
		IT_LEATHER_PANTS,
		IT_LEATHER_BOOTS,
		IT_CHAIN_HELMET,
		IT_CHAIN_CHESTPLATE,
		IT_CHAIN_LEGGINGS,
		IT_CHAIN_BOOTS,
		IT_IRON_HELMET,
		IT_IRON_CHESTPLATE,
		IT_IRON_LEGGINGS,
		IT_IRON_BOOTS,
		IT_DIAMOND_HELMET,
		IT_DIAMOND_CHESTPLATE,
		IT_DIAMOND_LEGGINGS,
		IT_DIAMOND_BOOTS,
		IT_GOLDEN_HELMET,
		IT_GOLDEN_CHESTPLATE,
		IT_GOLDEN_LEGGINGS,
		IT_GOLDEN_BOOTS,
		IT_FLINT,
		IT_RAW_PORKCHOP,
		IT_COOKED_PORKCHOP,
		IT_PAINTING,
		IT_GOLDEN_APPLE,
		IT_SIGN,
		IT_WOODEN_DOOR,
		IT_BUCKET,
		IT_WATER_BUCKET,
		IT_LAVA_BUCKET,
		IT_MINECART,
		IT_SADDLE,
		IT_IRON_DOOR,
		IT_REDSTONE_DUST,
		IT_SNOWBALL,
		IT_BOAT,
		IT_LEATHER,
		IT_MILK,
		IT_CLAY_BRICK,
		IT_CLAY,
		IT_SUGAR_CANE,
		IT_PAPER,
		IT_BOOK,
		IT_SLIMEBALL,
		IT_CHEST_MINECART,
		IT_FURNACE_MINECART,
		IT_CHICKEN_EGG,
		IT_COMPASS,
		IT_FISHING_ROD,
		IT_CLOCK,
		IT_GLOWSTONE_DUST,
		IT_RAW_FISH,
		IT_COOKED_FISH,
		IT_DYE,
		IT_BONE,
		IT_SUGAR,
		IT_CAKE,
		IT_BED,
		IT_READSTONE_REPEATER,
		IT_COOKIE,
		IT_MAP,
		IT_SHEARS,
		IT_MELON_SLICE,
		IT_PUMPKIN_SEEDS,
		IT_MELON_SEEDS,
		IT_RAW_BEEF,
		IT_STEAK,
		IT_RAW_CHICKEN,
		IT_COOKED_CHICKEN,
		IT_ROTTEN_FLESH,
		IT_ENDER_PEARL,
		IT_BLAZE_ROD,
		IT_GHAST_TEAR,
		IT_GOLD_NUGGET,
		IT_NETHER_WART,
		IT_POTION,
		IT_GLASS_BOTTLE,
		IT_SPIDER_EYE,
		IT_FERMENTED_SPIDER_EYE,
		IT_BLAZE_POWDER,
		IT_MAGMA_CREAM,
		IT_BREWING_STAND,
		IT_CAULDRON,
		IT_EYE_OF_ENDER,
		IT_GLISTERING_MELON,
		IT_SPAWN_EGG,
		IT_BOTTLE_O_ENCHANTING,
		IT_FIRE_CHARGE,
		IT_BOOK_AND_QUILL,
		IT_WRITTEN_BOOK,
		IT_EMERALD,
		IT_ITEM_FRAME,
		IT_FLOWER_POT,
		IT_CARROT,
		IT_POTATO,
		IT_BAKED_POTATO,
		IT_POISONOUS_POTATO,
		IT_EMPTY_MAP,
		IT_GOLDEN_CARROT,
		IT_HEAD,
		IT_CARROT_ON_A_STICK,
		IT_NETHER_STAR,
		IT_PUMPKIN_PIE,
		
		IT_DISC_13 = 0x8D0,
		IT_DISC_CAT,
		IT_DISC_BLOCKS,
		IT_DISC_CHIRP,
		IT_DISC_FAR,
		IT_DISC_MALL,
		IT_DISC_MELLOHI,
		IT_DISC_STAL,
		IT_DISC_STRAD,
		IT_DISC_WARD,
		IT_DISC_11,
	};
	
	
	
//----
	
	struct food_info
	{
		int hunger;
		float saturation;
	};
	
	
	/* 
	 * Item information.
	 */
	struct item_info
	{
		unsigned short id;
		char name[33];
		unsigned short durability;
		char max_stack;
		
	//----
		
		item_info (unsigned short id, const char *name, unsigned short durability,
			char max_stack);
		
		inline bool stackable () { return max_stack > 1; }
			
	//----
		
		/* 
		 * Returns the item_info structure describing the item associated with the
		 * specified ID number.
		 */
		static item_info* from_id (unsigned short id);
		
		
		/* 
		 * Returns an appropriate food_info structure which information about food
		 * aspects of the item with the specified ID.
		 */
		static food_info get_food_info (unsigned short id);
		
		
		/* 
		 * Checks whether the item associated with the given ID is a tool.
		 * (Anything that has a durability bar is considered a tool (except armor))
		 */
		static bool is_tool (unsigned short id);
	};
}

#endif

