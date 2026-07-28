-- Copyright (C) 2005 - 2026 Settlers Freaks <sf-team at siedler25.org>
--
-- SPDX-License-Identifier: GPL-2.0-or-later

-- A 4th landscape, "world", used exclusively by the map generator's climate system (see
-- mapGenerator/Climate.h/.cpp): rather than generating a map from a single up-front choice of
-- greenland/wasteland/winterworld, the generator now picks land textures by a (humidity,
-- temperature) "biome" reading, so a single generated map can contain meadows, savanna, desert
-- and tundra together, chosen by local climate. greenland.lua/wasteland.lua/winterworld.lua are
-- left completely untouched (including their original s2Id 0/1/2) so existing, already-saved
-- maps using them keep loading exactly as before; this file adds new, separate copies of a
-- subset of their terrains/edges (renamed with a "w_" prefix, e.g. "gl_meadow" -> "w_gl_meadow")
-- under this new landscape instead of modifying/reusing the originals, since terrain and edge
-- names must be globally unique and a terrain's s2Id only needs to be unique within its own
-- landscape.
--
-- IMPORTANT, and the reason this is a *subset*: the original Settlers II map file format only
-- has 6 bits (values 0-63) per tile to store which terrain it uses, *within* a landscape - see
-- MapLoader::InitNodes(), `t1 & 0x3F`. All three original landscapes stay within that limit
-- individually (~23-24 terrains each), but naively combining all of their terrains (70 total)
-- into one landscape does not fit, and any terrain assigned an id above 63 gets silently
-- corrupted (masked into a lower, wrong id) the moment a generated map is saved and reloaded -
-- which is exactly what happens when starting a round from a freshly generated map. This is why
-- only a curated 44-terrain subset is included here: it drops terrains that were already
-- unreachable duplicates (e.g. a second "buildable mountain" terrain that Find() never returns
-- because an earlier one already matches) or never referenced by any predicate the generator
-- actually uses (e.g. "reef"/"shallow" water variants, or extra lava/ice textures beyond the one
-- used for the mountain peak) - dropping them costs no generator behaviour. All 18 terrains that
-- the climate system actually chooses between (the "biome" pool) are kept in full.
--
-- `humidity`/`temperature` on those 18 buildable-land terrains have been retuned as one
-- coordinated set spanning the combined climate space, instead of three independent ones.

texFile = "<RTTR_GAME>/GFX/TEXTURES/TEX5.LBM"

rttr:AddLandscape{
	-- Name used to reference this
	name = __"world",
	-- File used for the objects on the map (trees, stones, etc.). A single landscape can only
	-- reference one such file, so this new landscape reuses greenland's set for all its terrains -
	-- decorative objects placed by the map generator won't visually adapt to hot/cold regions the
	-- way the terrain textures (chosen per-tile by humidity/temperature) do.
	mapGfx = "<RTTR_GAME>/DATA/MAP_0_Z.LST",
	-- Id used in the original S2. 0/1/2 are already taken by greenland/wasteland/winterworld
	-- (kept fully intact for backwards compatibility with existing maps), so this new, 4th
	-- landscape - used exclusively by the map generator from now on - takes the next free id.
	s2Id = 3,
	-- Not exclusively a winter world
	isWinter = false,
	-- Road textures used for this terrain. A landscape only has one road style, so - like mapGfx -
	-- this reuses greenland's roads for the whole world landscape.
	roads = {
		-- Regular road
		normal = {
			-- Filename of the texture image
			texture = texFile,
			-- Position and size {x, y, w, h} in the image if it contains multiple textures
			-- Can be left out. A size of 0 (w and/or h) is interpreted as the remaining image
			pos = {192, 0, 64, 16}
		},
		-- Upgraded road (with 2nd carrier)
		upgraded = {
			texture = texFile,
			pos = {192, 16, 64, 16}
		},
		-- Boat road
		boat = {
			texture = texFile,
			pos = {192, 32, 64, 16}
		},
		-- Road on a mountain or mountain side
		mountain = {
			texture = texFile,
			pos = {192, 160, 64, 16}
		}
	}
}

-- ===== Greenland-derived terrains (temperate grassland biomes) =====

rttr:AddTerrainEdge{
	-- Name used to reference this
	name = "w_gl_snow",
	-- Landscape to which this applies by default
	landscape = "world",
	-- Filename of the texture image
	texture = texFile,
	-- Position and size {x, y, w, h} in the image if it contains multiple textures
	-- Can be left out. A size of 0 (w and/or h) is interpreted as the remaining image
	pos = {192, 176, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_gl_mountain",
	landscape = "world",
	texture = texFile,
	pos = {192, 192, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_gl_desert",
	landscape = "world",
	texture = texFile,
	pos = {192, 208, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_gl_meadow",
	landscape = "world",
	texture = texFile,
	pos = {192, 224, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_gl_water",
	landscape = "world",
	texture = texFile,
	pos = {192, 240, 64, 16}
}

rttr:AddTerrain{
	-- Name used to reference this
	name = "w_gl_snow",
	-- Landscape to which this applies by default
	landscape = "world",
	-- Name of the edge drawn over neighbouring terrain or "none"
	edgeType = "w_gl_snow",
	-- Id used in the original S2. Defaults to 0xFF (not in S2)
	s2Id = 0,
	-- If this is higher than the neighbours edgePriority then it draws over the neighbour
	-- Valid = [-128, 127], defaults to 0
	edgePriority = 75,
	-- What kind of terrain is this? (Used for animals, ships, etc)
	-- Valid = land (default), water, lava, snow, mountain
	kind = "snow",
	-- Property for this terrain. 
	-- Valid = buildable   (allows buildings, includes walkable), default for land
	--	      mineable    (allows mines, includes walkable), default for mountain
	--	      walkable    (allows flags, people, animals)
	--        shippable   (allows ships only), default for water
	--        unwalkable  (can't walk on, but near)
	--        unreachable (dangerous, can't go near), default for snow, lava
	property = "unreachable",
	-- Humidity in percent (0..100) which determinate how much water can be on this terrain
	-- Defaults to 0 for lava, snow, mountain, 100 otherwise
	humidity = 0,
	-- Filename of the texture image
	texture = texFile,
	-- Position and size {x, y, w, h} in the image if it contains multiple textures
	-- Can be left out. A size of 0 (w and/or h) is interpreted as the remaining image
	pos = {0, 0, 32, 31},
	-- How are the texture triangles positioned? Default: overlapped
	-- overlapped: USD and RSU triangle overlap (like S2)
	-- stacked: RSU on top, USD below forming a rotated rectangle
	-- rotated: Similar to stacked but tips are left (RSU) and right (USD)
	texType = "overlapped",
	-- Index of the palette animation in the file, default -1 for no animation
	palAnimIdx = -1,
	-- Color used to display this on the minimap
	color = 0xFFFFFFFF
}
rttr:AddTerrain{
	name = "w_gl_desert1",
	landscape = "world",
	edgeType = "w_gl_desert",
	s2Id = 1,
	edgePriority = 65,
	kind = "land",
	property = "walkable",
	humidity = 0,
	temperature = 62,
	texture = texFile,
	pos = {48, 0, 32, 31},
	color = 0xFFc09c7c
}
rttr:AddTerrain{
	name = "w_gl_swamp",
	landscape = "world",
	edgeType = "w_gl_meadow",
	s2Id = 2,
	edgePriority = 10,
	kind = "land",
	humidity = 100,
	temperature = 55,
	property = "unwalkable",
	texture = texFile,
	pos = {96, 0, 32, 31},
	color = 0xFF649014
}
rttr:AddTerrain{
	name = "w_gl_meadowFlowers",
	landscape = "world",
	edgeType = "w_gl_meadow",
	s2Id = 3,
	edgePriority = 35,
	kind = "land",
	humidity = 75,
	temperature = 48,
	texture = texFile,
	pos = {144, 0, 32, 31},
	color = 0xFF48780c
}
rttr:AddTerrain{
	name = "w_gl_mountain1",
	landscape = "world",
	edgeType = "w_gl_mountain",
	s2Id = 4,
	edgePriority = 55,
	kind = "mountain",
	texture = texFile,
	pos = {0, 48, 32, 31},
	color = 0xFF9c8058
}
rttr:AddTerrain{
	name = "w_gl_mountain2",
	landscape = "world",
	edgeType = "w_gl_mountain",
	s2Id = 5,
	edgePriority = 50,
	kind = "mountain",
	texture = texFile,
	pos = {48, 48, 32, 31},
	color = 0xFF9c8058
}
rttr:AddTerrain{
	name = "w_gl_mountain3",
	landscape = "world",
	edgeType = "w_gl_mountain",
	s2Id = 6,
	edgePriority = 45,
	kind = "mountain",
	texture = texFile,
	pos = {96, 48, 32, 31},
	color = 0xFF9c8058
}
rttr:AddTerrain{
	name = "w_gl_mountain4",
	landscape = "world",
	edgeType = "w_gl_mountain",
	s2Id = 7,
	edgePriority = 40,
	kind = "mountain",
	texture = texFile,
	pos = {144, 48, 32, 31},
	color = 0xFF8c7048
}
rttr:AddTerrain{
	name = "w_gl_savannah",
	landscape = "world",
	edgeType = "w_gl_desert",
	s2Id = 8,
	edgePriority = 15,
	kind = "land",
	humidity = 45,
	temperature = 60,
	texture = texFile,
	pos = {0, 96, 32, 31},
	color = 0xFF649014
}
rttr:AddTerrain{
	name = "w_gl_meadow1",
	landscape = "world",
	edgeType = "w_gl_meadow",
	s2Id = 9,
	edgePriority = 30,
	kind = "land",
	humidity = 90,
	temperature = 45,
	texture = texFile,
	pos = {48, 96, 32, 31},
	color = 0xFF48780c
}
rttr:AddTerrain{
	name = "w_gl_meadow2",
	landscape = "world",
	edgeType = "w_gl_meadow",
	s2Id = 10,
	edgePriority = 25,
	kind = "land",
	humidity = 65,
	temperature = 50,
	texture = texFile,
	pos = {96, 96, 32, 31},
	color = 0xFF649014
}
rttr:AddTerrain{
	name = "w_gl_meadow3",
	landscape = "world",
	edgeType = "w_gl_meadow",
	s2Id = 11,
	edgePriority = 20,
	kind = "land",
	humidity = 55,
	temperature = 55,
	texture = texFile,
	pos = {144, 96, 32, 31},
	color = 0xFF407008
}
rttr:AddTerrain{
	name = "w_gl_steppe",
	landscape = "world",
	edgeType = "w_gl_desert",
	s2Id = 12,
	edgePriority = 60,
	kind = "land",
	humidity = 25,
	temperature = 63,
	texture = texFile,
	pos = {0, 144, 32, 31},
	color = 0xFF88b028
}
rttr:AddTerrain{
	name = "w_gl_mountainMeadow",
	landscape = "world",
	edgeType = "w_gl_mountain",
	s2Id = 13,
	edgePriority = 70,
	kind = "mountain",
	humidity = 100,
	property = "buildable",
	texture = texFile,
	pos = {48, 144, 32, 31},
	color = 0xFF9c8058
}
rttr:AddTerrain{
	name = "w_gl_water",
	landscape = "world",
	edgeType = "w_gl_water",
	s2Id = 14,
	edgePriority = 5,
	kind = "water",
	texture = texFile,
	pos = {193, 49, 53, 54},
	texType = "rotated",
	palAnimIdx = 10,
	color = 0xFF1038a4
}
rttr:AddTerrain{
	name = "w_gl_desert2",
	landscape = "world",
	edgeType = "w_gl_desert",
	s2Id = 15,
	edgePriority = 65,
	kind = "land",
	property = "walkable",
	humidity = 0,
	temperature = 62,
	texture = texFile,
	pos = {48, 0, 32, 31},
	color = 0xFFc09c7c
}

-- ===== Wasteland-derived terrains (hot/arid biomes) =====

texFile = "<RTTR_GAME>/GFX/TEXTURES/TEX6.LBM"

rttr:AddTerrainEdge{
	-- Name used to reference this
	name = "w_wl_stone",
	-- Landscape to which this applies by default
	landscape = "world",
	-- Filename of the texture image
	texture = texFile,
	-- Position and size {x, y, w, h} in the image if it contains multiple textures
	-- Can be left out. A size of 0 (w and/or h) is interpreted as the remaining image
	pos = {192, 176, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_wl_moor",
	landscape = "world",
	texture = texFile,
	pos = {192, 192, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_wl_wasteland",
	landscape = "world",
	texture = texFile,
	pos = {192, 208, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_wl_mountain",
	landscape = "world",
	texture = texFile,
	pos = {192, 224, 64, 16}
}

rttr:AddTerrain{
	name = "w_wl_wasteland1",
	landscape = "world",
	edgeType = "w_wl_wasteland",
	s2Id = 16,
	edgePriority = 50,
	kind = "land",
	property = "walkable",
	humidity = 0,
	temperature = 88,
	texture = texFile,
	pos = {48, 0, 32, 31},
	color = 0xFF9c7c64
}
rttr:AddTerrain{
	name = "w_wl_flowerPasture",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 17,
	edgePriority = 40,
	kind = "land",
	humidity = 70,
	temperature = 65,
	texture = texFile,
	pos = {144, 0, 32, 31},
	color = 0xFF444850
}
rttr:AddTerrain{
	name = "w_wl_mountain1",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 18,
	edgePriority = 30,
	kind = "mountain",
	texture = texFile,
	pos = {0, 48, 32, 31},
	color = 0xFF706c54
}
rttr:AddTerrain{
	name = "w_wl_mountain2",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 19,
	edgePriority = 30,
	kind = "mountain",
	texture = texFile,
	pos = {48, 48, 32, 31},
	color = 0xFF706454
}
rttr:AddTerrain{
	name = "w_wl_mountain3",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 20,
	edgePriority = 30,
	kind = "mountain",
	texture = texFile,
	pos = {96, 48, 32, 31},
	color = 0xFF684c24
}
rttr:AddTerrain{
	name = "w_wl_mountain4",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 21,
	edgePriority = 30,
	kind = "mountain",
	texture = texFile,
	pos = {144, 48, 32, 31},
	color = 0xFF684c24
}
rttr:AddTerrain{
	name = "w_wl_dSteppe",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 22,
	edgePriority = 40,
	kind = "land",
	humidity = 20,
	temperature = 85,
	texture = texFile,
	pos = {0, 96, 32, 31},
	color = 0xFF444850
}
rttr:AddTerrain{
	name = "w_wl_pasture1",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 23,
	edgePriority = 40,
	kind = "land",
	humidity = 80,
	temperature = 70,
	texture = texFile,
	pos = {48, 96, 32, 31},
	color = 0xFF5c5840
}
rttr:AddTerrain{
	name = "w_wl_pasture2",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 24,
	edgePriority = 40,
	kind = "land",
	humidity = 50,
	temperature = 75,
	texture = texFile,
	pos = {96, 96, 32, 31},
	color = 0xFF646048
}
rttr:AddTerrain{
	name = "w_wl_pasture3",
	landscape = "world",
	edgeType = "w_wl_mountain",
	s2Id = 25,
	edgePriority = 40,
	kind = "land",
	humidity = 35,
	temperature = 80,
	texture = texFile,
	pos = {144, 96, 32, 31},
	color = 0xFF646048
}
rttr:AddTerrain{
	name = "w_wl_lSteppe",
	landscape = "world",
	edgeType = "w_wl_wasteland",
	s2Id = 26,
	edgePriority = 60,
	kind = "land",
	humidity = 5,
	temperature = 95,
	texture = texFile,
	pos = {0, 144, 32, 31},
	color = 0xFF88b028
}
rttr:AddTerrain{
	name = "w_wl_alpPasture",
	landscape = "world",
	edgeType = "w_wl_stone",
	s2Id = 27,
	edgePriority = 90,
	kind = "mountain",
	property = "buildable",
	texture = texFile,
	pos = {48, 144, 32, 31},
	color = 0xFF001820
}
rttr:AddTerrain{
	name = "w_wl_moor",
	landscape = "world",
	edgeType = "w_wl_moor",
	s2Id = 28,
	edgePriority = 70,
	kind = "water",
	texture = texFile,
	pos = {193, 49, 53, 54},
	texType = "rotated",
	palAnimIdx = 10,
	color = 0xFF454520
}
rttr:AddTerrain{
	name = "w_wl_wasteland2",
	landscape = "world",
	edgeType = "w_wl_wasteland",
	s2Id = 29,
	edgePriority = 50,
	kind = "land",
	property = "walkable",
	humidity = 0,
	temperature = 88,
	texture = texFile,
	pos = {48, 0, 32, 31},
	color = 0xFF9c7c64
}

-- ===== Winterworld-derived terrains (cold/arctic biomes) =====

texFile = "<RTTR_GAME>/GFX/TEXTURES/TEX7.LBM"

rttr:AddTerrainEdge{
	-- Name used to reference this
	name = "w_ww_snow",
	-- Landscape to which this applies by default
	landscape = "world",
	-- Filename of the texture image
	texture = texFile,
	-- Position and size {x, y, w, h} in the image if it contains multiple textures
	-- Can be left out. A size of 0 (w and/or h) is interpreted as the remaining image
	pos = {192, 176, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_ww_mountain",
	landscape = "world",
	texture = texFile,
	pos = {192, 192, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_ww_ice",
	landscape = "world",
	texture = texFile,
	pos = {192, 208, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_ww_tundra",
	landscape = "world",
	texture = texFile,
	pos = {192, 224, 64, 16}
}
rttr:AddTerrainEdge{
	name = "w_ww_water",
	landscape = "world",
	texture = texFile,
	pos = {192, 240, 64, 16}
}

rttr:AddTerrain{
	name = "w_ww_ice1",
	landscape = "world",
	edgeType = "w_ww_ice",
	s2Id = 30,
	edgePriority = 43,
	kind = "land",
	property = "walkable",
	humidity = 0,
	temperature = 8,
	texture = texFile,
	pos = {48, 0, 32, 31},
	color = 0xFF0070b0
}
rttr:AddTerrain{
	name = "w_ww_tundraFlower",
	landscape = "world",
	edgeType = "w_ww_tundra",
	s2Id = 31,
	edgePriority = 13,
	kind = "land",
	humidity = 85,
	temperature = 32,
	texture = texFile,
	pos = {144, 0, 32, 31},
	color = 0xFF7c84ac
}
rttr:AddTerrain{
	name = "w_ww_mountain1",
	landscape = "world",
	edgeType = "w_ww_mountain",
	s2Id = 32,
	edgePriority = 48,
	kind = "mountain",
	texture = texFile,
	pos = {0, 48, 32, 31},
	color = 0xFF54586c
}
rttr:AddTerrain{
	name = "w_ww_mountain2",
	landscape = "world",
	edgeType = "w_ww_mountain",
	s2Id = 33,
	edgePriority = 63,
	kind = "mountain",
	texture = texFile,
	pos = {48, 48, 32, 31},
	color = 0xFF60607c
}
rttr:AddTerrain{
	name = "w_ww_mountain3",
	landscape = "world",
	edgeType = "w_ww_mountain",
	s2Id = 34,
	edgePriority = 58,
	kind = "mountain",
	texture = texFile,
	pos = {96, 48, 32, 31},
	color = 0xFF686c8c
}
rttr:AddTerrain{
	name = "w_ww_mountain4",
	landscape = "world",
	edgeType = "w_ww_mountain",
	s2Id = 35,
	edgePriority = 53,
	kind = "mountain",
	texture = texFile,
	pos = {144, 48, 32, 31},
	color = 0xFF686c8c
}
rttr:AddTerrain{
	name = "w_ww_taiga",
	landscape = "world",
	edgeType = "w_ww_tundra",
	s2Id = 36,
	edgePriority = 18,
	kind = "land",
	humidity = 60,
	temperature = 30,
	texture = texFile,
	pos = {0, 96, 32, 31},
	color = 0xFFa0accc
}
rttr:AddTerrain{
	name = "w_ww_tundra1",
	landscape = "world",
	edgeType = "w_ww_tundra",
	s2Id = 37,
	edgePriority = 23,
	kind = "land",
	humidity = 95,
	temperature = 10,
	texture = texFile,
	pos = {48, 96, 32, 31},
	color = 0xFFb0a494
}
rttr:AddTerrain{
	name = "w_ww_tundra2",
	landscape = "world",
	edgeType = "w_ww_tundra",
	s2Id = 38,
	edgePriority = 28,
	kind = "land",
	humidity = 55,
	temperature = 15,
	texture = texFile,
	pos = {96, 96, 32, 31},
	color = 0xFF88a874
}
rttr:AddTerrain{
	name = "w_ww_tundra3",
	landscape = "world",
	edgeType = "w_ww_tundra",
	s2Id = 39,
	edgePriority = 33,
	kind = "land",
	humidity = 75,
	temperature = 20,
	texture = texFile,
	pos = {144, 96, 32, 31},
	color = 0xFFa0accc
}
rttr:AddTerrain{
	name = "w_ww_tundra4",
	landscape = "world",
	edgeType = "w_ww_tundra",
	s2Id = 40,
	edgePriority = 8,
	kind = "land",
	humidity = 15,
	temperature = 5,
	texture = texFile,
	pos = {0, 144, 32, 31},
	color = 0xFF88b15e
}
rttr:AddTerrain{
	name = "w_ww_water",
	landscape = "world",
	edgeType = "w_ww_water",
	s2Id = 41,
	edgePriority = 78,
	kind = "water",
	texture = texFile,
	pos = {193, 49, 53, 54},
	texType = "rotated",
	palAnimIdx = 10,
	color = 0xFF1038a4
}
rttr:AddTerrain{
	name = "w_ww_flatMountain",
	landscape = "world",
	edgeType = "w_ww_mountain",
	s2Id = 42,
	edgePriority = 38,
	kind = "mountain",
	property = "buildable",
	texture = texFile,
	pos = {48, 48, 32, 31},
	color = 0xFF60607c
}
rttr:AddTerrain{
	name = "w_ww_ice2",
	landscape = "world",
	edgeType = "w_ww_ice",
	s2Id = 43,
	edgePriority = 43,
	kind = "land",
	property = "walkable",
	humidity = 0,
	temperature = 8,
	texture = texFile,
	pos = {48, 0, 32, 31},
	color = 0xFF0070b0
}
