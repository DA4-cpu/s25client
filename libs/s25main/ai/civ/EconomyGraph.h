// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "helpers/EnumArray.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/Resource.h"
#include <string>
#include <vector>

class GameWorldBase;

namespace civai {

/// What a building takes from the map (besides its input wares)
enum class SourceKind : uint8_t
{
    /// Nothing from the map (only input wares)
    None,
    /// Grown trees (woodcutter)
    Trees,
    /// Stone piles (quarry)
    StonePiles,
    /// Ore in the ground: index = ResourceType (mines)
    Ore,
    /// Fish in the water
    Fish,
    /// Water in the ground (well)
    Water,
    /// Wild animals (hunter, skinner)
    Animals,
    /// Free buildable land for fields (farm, vineyard, charburner)
    Land,
    /// Map object defined in lua: index = map object (collectors)
    MapObject,
    /// Places where a map object defined in lua grows naturally: index = map object (plantations)
    Plantable
};

struct Source
{
    SourceKind kind = SourceKind::None;
    unsigned index = 0;
};

/// How a building makes wares, read from the game data (built-in buildings and workshops from lua)
struct Recipe
{
    BuildingType building;
    /// Input wares. consumeEach: one of each per product, otherwise one of any of them
    std::vector<GoodType> inputs;
    bool consumeEach = true;
    /// Products. Several: the building chooses one per product (e.g. the metalworks)
    std::vector<GoodType> outputs;
    /// Byproducts (byproducts.lua) and the technology each needs (-1: none)
    std::vector<GoodType> byproducts;
    std::vector<int> byproductTechs;
    Source source;
    /// Tool slots without which the building does not work (tool reform, base speed 0): any ware of each slot
    std::vector<std::vector<GoodType>> requiredTools;
    /// Not buildable at the start: one of these technologies unlocks it
    bool lockedAtStart = false;
    /// Wares per minute of one building at normal speed (estimate: work cycle and output ratio)
    double waresPerMinute = 0;
};

/// Production graph of the game: which buildings make which wares from what. Built once at the start of the game from
/// the game data (it does not change during the game), so new content from lua is part of it without code changes
class EconomyGraph
{
public:
    explicit EconomyGraph(const GameWorldBase& world);

    const std::vector<Recipe>& GetRecipes() const { return recipes_; }
    /// Indices of the recipes making the ware (as product or byproduct)
    const std::vector<unsigned>& GetProducers(GoodType good) const { return producers_[good]; }

    /// Text for a source, e.g. "Bäume" or "Erz Copper"
    std::string GetSourceName(const Source& source) const;

private:
    void AddBuiltinRecipes(const GameWorldBase& world);
    void AddCustomRecipes(const GameWorldBase& world);
    void AddRecipe(const GameWorldBase& world, Recipe recipe, unsigned cycleGFs);

    std::vector<Recipe> recipes_;
    helpers::EnumArray<std::vector<unsigned>, GoodType> producers_;
    std::vector<std::string> mapObjectNames_;
    std::vector<std::string> oreNames_;
};

} // namespace civai
