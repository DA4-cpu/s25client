// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "TechnologyDesc.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kaguya {
class LuaTable;
class LuaRef;
} // namespace kaguya

/// A list of tags. A ware matches if it has all of them
using TagQuery = std::vector<std::string>;

/// New ware of the civilization mode as read from lua (rttr:AddWare)
struct WareDesc
{
    /// Unique identifier (also the "name" tag), used in other lua definitions and savegames
    std::string name;
    /// Displayed name
    std::string title;
    /// Kind of ware ("Warenart"), also a tag. Optional
    std::string kind;
    /// Properties ("Eigenschaften"), also tags. Can be passed on to products of workshops
    std::vector<std::string> properties;
    /// Identifier of an existing ware whose graphics are used (placeholder)
    std::string look;
    /// Identifier of an existing ware with the same transport priority
    std::string transport;
    /// Knowledge points when stored as a knowledge medium
    KnowledgeValues knowledge{};

    WareDesc() = default;
    explicit WareDesc(const kaguya::LuaTable& luaData);
};

/// Additional tags of a built-in ware (rttr:SetWareTags)
struct WareTagsDesc
{
    std::string kind;
    std::vector<std::string> properties;
};

/// Worker of a workshop
struct WorkerDesc
{
    std::string title;
    /// Identifier of the tool required to recruit the worker or "none"
    std::string tool;
    /// Identifier of an existing job whose graphics are used (placeholder)
    std::string look;
};

/// Object on the map which a workshop can collect (rttr:AddMapObject), e.g. a loam deposit. It has several sizes and
/// gets smaller with every collected ware; the smallest one disappears after the last ware
struct MapObjectDesc
{
    /// Unique identifier, used in workshops, the map generator, map files and savegames
    std::string name;
    /// Displayed name
    std::string title;
    /// Number of sizes = wares in a full object (1..16)
    unsigned sizes = 6;
    /// Placeholder graphics: "Granite1" or "Granite2" (granite piles)
    std::string look;
    /// Placeholder graphics are tinted with this color (0xRRGGBB), 0xFFFFFF = unchanged
    unsigned tint = 0xFFFFFF;
    /// Seconds until a planted object is grown (plantations). 0: Can not be planted
    unsigned growTime = 0;
    /// Size of a grown planted object (1..sizes). 0: sizes
    unsigned plantSize = 0;

    MapObjectDesc() = default;
    explicit MapObjectDesc(const kaguya::LuaTable& luaData);
};

/// Ore in the mountains which mines can dig and the geologist finds (rttr:AddOre), like coal, iron, gold and granite
struct OreDesc
{
    /// Unique identifier, used in workshops (mine), the map generator, map files and savegames
    std::string name;
    /// Displayed name
    std::string title;
    /// Placeholder graphics (sign of the geologist): "Iron", "Gold", "Coal" or "Granite"
    std::string look;
    /// The sign is tinted with this color (0xRRGGBB), 0xFFFFFF = unchanged
    unsigned tint = 0xFFFFFF;

    OreDesc() = default;
    explicit OreDesc(const kaguya::LuaTable& luaData);
};

/// Simple production building of the civilization mode as read from lua (rttr:AddWorkshop):
/// Takes input wares and produces an output ware or collects map objects (like the quarry)
struct WorkshopDesc
{
    std::string name;
    std::string title;
    std::string description;
    /// "small", "medium", "large" or "mine"
    std::string size;
    /// Construction costs: ware identifier -> amount (currently only Boards and Stones)
    std::map<std::string, unsigned> cost;
    /// Identifier of an existing building whose graphics are used (placeholder)
    std::string look;
    WorkerDesc worker;
    /// Inputs, each a tag query
    std::vector<TagQuery> inputs;
    /// True: one ware of each input per product. False: one ware of the input with the most wares (like mines)
    bool consumeEach = true;
    /// Tag query of the product
    TagQuery output;
    /// True: the product gets the properties of the consumed input ware
    bool inheritProperties = false;
    /// How the product is chosen if the output matches several wares. Empty: the output must match exactly one
    /// ware. "knowledge": by knowledge type of the products (regulator "Wissen" in the distribution window)
    std::string outputChoice;
    /// Library: Orders and stores up to this many knowledge media (all wares with knowledge points) and produces
    /// nothing. 0: normal workshop
    unsigned libraryCapacity = 0;
    /// Number of wares stored per input
    unsigned storage = 6;
    /// Seconds of work per product and seconds of waiting before
    unsigned workTime = 0, waitTime = 0;
    /// Collector: Identifier of the map object the worker collects (rttr:AddMapObject) instead of consuming inputs.
    /// Empty: normal workshop
    std::string collectObject;
    /// Collector: Maximum distance of the map objects in nodes
    unsigned collectRadius = 0;
    /// Plantation: Identifier of the map object the worker plants (rttr:AddMapObject with growTime). With an output he
    /// also harvests grown objects of this kind. Empty: no plantation
    std::string plantObject;
    /// Plantation: Maximum distance of the planted objects in nodes
    unsigned plantRadius = 0;
    /// Mine: Identifier of the ore (rttr:AddOre) the worker digs. Empty: no mine
    std::string mineResource;
    /// Knowledge points of the finished building (like rttr:SetBuildingKnowledge)
    std::optional<KnowledgeValues> knowledge;
    /// True: The AI may build it near a land border (otherwise that space is kept free for military buildings)
    bool aiNearBorder = false;

    WorkshopDesc() = default;
    explicit WorkshopDesc(const kaguya::LuaTable& luaData);
};

/// Byproduct of a building as read from lua (rttr:AddByproduct):
/// Whenever the worker of the building carries out a product, he also brings the byproduct to the flag
struct ByproductDesc
{
    /// Identifier of the building (built-in or workshop)
    std::string building;
    /// Tag query of the byproduct, must match exactly one ware
    TagQuery ware;
    /// Number of byproducts per product
    unsigned amount = 1;
    /// Only for these products (tag query). Empty: for every product of the building
    TagQuery per;
    /// Name of the technology after which the byproduct is produced. Empty: always
    std::string technology;

    ByproductDesc() = default;
    explicit ByproductDesc(const kaguya::LuaTable& luaData);
};

/// Read a tag query from lua: Either a single string or a list of strings
TagQuery parseTagQuery(const kaguya::LuaRef& value, const std::string& context);
