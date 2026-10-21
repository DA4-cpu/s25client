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

/// Simple production building of the civilization mode as read from lua (rttr:AddWorkshop):
/// Takes input wares and produces an output ware
struct WorkshopDesc
{
    std::string name;
    std::string title;
    std::string description;
    /// "small", "medium" or "large"
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
    /// Number of wares stored per input
    unsigned storage = 6;
    /// Seconds of work per product and seconds of waiting before
    unsigned workTime = 0, waitTime = 0;

    WorkshopDesc() = default;
    explicit WorkshopDesc(const kaguya::LuaTable& luaData);
};

/// Read a tag query from lua: Either a single string or a list of strings
TagQuery parseTagQuery(const kaguya::LuaRef& value, const std::string& context);
