// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "CivContentDesc.h"
#include "DescriptionContainer.h"
#include "EdgeDesc.h"
#include "LandscapeDesc.h"
#include "MapGenDesc.h"
#include "TechnologyDesc.h"
#include "TerrainDesc.h"
#include <optional>
#include <stdexcept>

struct GameDataError : public std::runtime_error
{
    GameDataError(const std::string& desc) : std::runtime_error("Invalid game data: " + desc) {}
};

struct GameDataLoadError : public std::runtime_error
{
    GameDataLoadError(const std::string& desc) : std::runtime_error("Failed to load game data: " + desc) {}
};

struct WorldDescription
{
    WorldDescription();
    ~WorldDescription();
    DescriptionContainer<LandscapeDesc> landscapes;
    DescriptionContainer<EdgeDesc> edges;
    DescriptionContainer<TerrainDesc> terrain;
    /// Tech tree of the civilization mode
    DescriptionContainer<TechnologyDesc> technologies;
    /// Knowledge points of a finished building in the civilization mode: Building identifier -> points
    std::map<std::string, KnowledgeValues> buildingKnowledge;
    /// New wares of the civilization mode in order of definition
    std::vector<WareDesc> wares;
    /// Additional tags of built-in wares: Ware identifier -> tags
    std::map<std::string, WareTagsDesc> wareTags;
    /// Workshops (simple production buildings) of the civilization mode in order of definition
    std::vector<WorkshopDesc> workshops;
    /// Byproducts of buildings in the civilization mode in order of definition
    std::vector<ByproductDesc> byproducts;
    /// Default setting of the regulator "Wissen" (weights 0..10 for LWP, IWP, MWP). Not set: all equal
    std::optional<KnowledgeValues> knowledgeDistribution;
    /// Random map generator: climate, zones and resource rules (gamedata/world/mapgen.lua)
    MapGenDesc mapGen;
    // Convenience accessors
    template<class T>
    const T& get(DescIdx<T> idx) const
    {
        return getContainer<T>().get(idx);
    }
    template<class T>
    const DescriptionContainer<T>& getContainer() const;
};

template<>
inline const DescriptionContainer<LandscapeDesc>& WorldDescription::getContainer() const
{
    return landscapes;
}

template<>
inline const DescriptionContainer<EdgeDesc>& WorldDescription::getContainer() const
{
    return edges;
}

template<>
inline const DescriptionContainer<TerrainDesc>& WorldDescription::getContainer() const
{
    return terrain;
}

template<>
inline const DescriptionContainer<TechnologyDesc>& WorldDescription::getContainer() const
{
    return technologies;
}
