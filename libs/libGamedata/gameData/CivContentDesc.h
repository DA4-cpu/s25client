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
    /// Kinds of ware ("Warenarten"), each also a tag. Optional, any number
    std::vector<std::string> kind;
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

/// Changes of a built-in ware (rttr:SetWare): new identifier and additional tags
struct BuiltinWareDesc
{
    /// New identifier, also the displayed name and the name tag. Empty: unchanged
    std::string name;
    /// Kinds of ware ("Warenarten"), each also a tag
    std::vector<std::string> kind;
    /// Properties ("Eigenschaften"), also tags
    std::vector<std::string> properties;
};

/// Read { name = "PineLog", kind = { ... }, properties = { ... } } of rttr:SetWare (all optional) for the built-in ware
/// with the given identifier
BuiltinWareDesc parseBuiltinWare(const kaguya::LuaTable& luaData, const std::string& ware);

/// Material of tools (rttr:AddToolMaterial), e.g. iron
struct ToolMaterialDesc
{
    /// Unique identifier
    std::string name;
    /// Displayed name
    std::string title;

    ToolMaterialDesc() = default;
    explicit ToolMaterialDesc(const kaguya::LuaTable& luaData);
};

/// Makes a ware a tool used in buildings (rttr:AddTool)
struct ToolDesc
{
    /// Identifier of the ware (built-in or rttr:AddWare)
    std::string ware;
    /// Identifier of the material (rttr:AddToolMaterial)
    std::string material;
    /// Number of work steps until the tool is broken
    unsigned durability = 0;
    /// Bonus to the work speed of the building in percent of the normal speed
    unsigned speed = 0;
    /// Bonus to the output ratio of the building in hundredths (wares per work step), e.g. 50 = +0.5, for every
    /// product. May be negative
    int outputRatio = 0;
    /// Bonus to the output ratio in hundredths only for the products with the tag (lua: outputRatio = { Sword = 0.5 })
    std::vector<std::pair<std::string, int>> outputRatioByTag;
    /// Change of the wares used per work step of every input in hundredths, e.g. -20 = 0.2 less. May be positive
    int inputRatio = 0;
    /// The same only for the inputs whose wares all have the tag (lua: inputRatio = { Coal = -0.25 })
    std::vector<std::pair<std::string, int>> inputRatioByTag;

    ToolDesc() = default;
    explicit ToolDesc(const kaguya::LuaTable& luaData);
};

/// Tools used in a building (rttr:SetBuildingTools or in rttr:AddWorkshop)
struct BuildingToolsDesc
{
    /// Work speed without any tool in percent of the normal speed. 0: does not work without tools
    unsigned baseSpeed = 0;
    /// One slot per entry: Tag query of the tools that fit (e.g. "Scythe" for all scythes)
    std::vector<TagQuery> tools;
};

/// Construction material of a building: `amount` wares matching the tags
struct CostDesc
{
    TagQuery tags;
    unsigned amount = 0;
};

/// Worker of a workshop
struct WorkerDesc
{
    std::string title;
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

/// An input of a workshop (rttr:AddWorkshop, inputs): a place for every ware matching the tag query, also mixed
struct InputDesc
{
    TagQuery tags;
    /// Wares used per work step in hundredths (100 = 1 per step, 50 = 1 every 2nd step)
    unsigned amount = 100;
    /// Places of the input. 0: storage of the workshop
    unsigned storage = 0;
};

/// Values of a military building as read from lua (rttr:AddWorkshop, military = { ... })
struct MilitaryDesc
{
    /// Most soldiers in the building
    unsigned soldiers = 0;
    /// Land radius in nodes
    unsigned radius = 0;
    /// Sight in nodes from the building. Not set: radius + 5 (like the built-in military buildings)
    std::optional<unsigned> vision;
    /// Places for gold coins (promotion of the soldiers)
    unsigned coins = 0;
    /// Places for armor (leather addon). Not set: same as coins (like the built-in military buildings)
    std::optional<unsigned> armor;
};
/// Highest value of soldiers, radius, vision, coins and armor of a military building (stored with 1 byte)
constexpr unsigned MAX_MILITARY_VALUE = 255;
/// Read military = { soldiers = 2, radius = 10, coins = 1 } (vision and armor optional)
MilitaryDesc parseMilitary(const kaguya::LuaTable& luaData, const std::string& context);

/// Frames a worker waits after the work before carrying out the product, unless set (waitAfterTime)
constexpr unsigned DEFAULT_WAIT_AFTER_TIME = 5;
/// Highest workTime, waitTime and waitAfterTime in frames
constexpr unsigned MAX_WORK_FRAMES = 65535;
/// Highest radius of a collector or a plantation in nodes
constexpr unsigned MAX_WORK_RADIUS = 20;

/// Simple production building of the civilization mode as read from lua (rttr:AddWorkshop):
/// Takes input wares and produces an output ware or collects map objects (like the quarry)
struct WorkshopDesc
{
    std::string name;
    std::string title;
    std::string description;
    /// "small", "medium", "large" or "mine"
    std::string size;
    /// Construction costs in the order the builder uses them
    std::vector<CostDesc> cost;
    /// Identifier of an existing building whose graphics are used (placeholder)
    std::string look;
    WorkerDesc worker;
    /// Inputs. A work step needs and uses the amount of every input
    std::vector<InputDesc> inputs;
    /// Products: each entry a tag query, every ware it matches is a product (e.g. "GeritzterKnochen" for all carved
    /// bones). Empty: no product (library, plantation that only plants)
    std::vector<TagQuery> outputs;
    /// Work steps each product of outputs costs in hundredths (same order as outputs, 100 = 1 step)
    std::vector<unsigned> outputSteps;
    /// True: the product gets the properties of the consumed input ware
    bool inheritProperties = false;
    /// "knowledge": the products (several) are chosen by their knowledge type (regulator "Wissen"). Empty: one product,
    /// or several chosen by the distribution of all wares (Gesamtverteilung)
    std::string outputChoice;
    /// Library: Orders and stores up to this many knowledge media (all wares with knowledge points) and produces
    /// nothing. 0: normal workshop
    unsigned libraryCapacity = 0;
    /// Warehouse: holds up to this many wares of the kinds in warehouseWares (tag queries) and nothing else, no
    /// people. It has no worker and produces nothing. 0: no warehouse
    unsigned warehouseCapacity = 0;
    std::vector<TagQuery> warehouseWares;
    /// Military building: soldiers occupy it and hold the land around it. It has no worker and produces nothing.
    /// Not set: no military building
    std::optional<MilitaryDesc> military;
    /// Number of wares stored per input
    unsigned storage = 6;
    /// Frames (game steps, 50 ms at normal speed) of work per work step, of waiting before the work and of waiting
    /// after the work before the product is carried out (like the built-in buildings)
    unsigned workTime = 0, waitTime = 0, waitAfterTime = DEFAULT_WAIT_AFTER_TIME;
    /// Collector: Identifier of the map object the worker collects (rttr:AddMapObject) instead of consuming inputs.
    /// Empty: normal workshop
    std::string collectObject;
    /// Collector: Maximum distance of the map objects in nodes
    unsigned collectRadius = 0;
    /// Plantation: Identifier of the map object the worker plants (rttr:AddMapObject with growTime). With outputs he
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
    /// Tools used in the workshop. Not set: no tools, full speed (100 %)
    std::optional<BuildingToolsDesc> tools;
    /// Wares per work step in hundredths (100 = 1 ware, 50 = 1 ware every 2nd step). Tools can add to it
    unsigned outputRatio = 100;

    WorkshopDesc() = default;
    explicit WorkshopDesc(const kaguya::LuaTable& luaData);
};

/// Built-in building changed in lua (rttr:SetBuilding{ name = "Sawmill", ... }). Every value not set keeps the one of
/// the original game. Construction costs, knowledge points, tools, output ratio and the capacity of a warehouse are
/// also put into the tables of WorldDescription (buildingCosts, buildingKnowledge, ...) by the loader
struct BuiltinBuildingDesc
{
    /// Identifier of the built-in building, e.g. "Sawmill"
    std::string name;
    /// Names of all fields given in lua (the game checks which ones the building may have)
    std::vector<std::string> fields;
    std::optional<std::string> title, description;
    /// "small", "medium", "large" or "mine"
    std::optional<std::string> size;
    std::optional<std::vector<CostDesc>> cost;
    /// Identifier of a built-in building whose graphics are used
    std::optional<std::string> look;
    /// Name and graphics of the job of the worker (worker = { title = "...", look = "Carpenter" })
    std::optional<std::string> workerTitle, workerLook;
    std::optional<KnowledgeValues> knowledge;
    std::optional<BuildingToolsDesc> tools;
    /// Wares per work step in hundredths
    std::optional<unsigned> outputRatio;
    /// Wares stored per input
    std::optional<unsigned> storage;
    /// Frames of work, of waiting before the work and of waiting after the work
    std::optional<unsigned> workTime, waitTime, waitAfterTime;
    std::optional<bool> aiNearBorder;
    /// Radius of a collector (collect = { radius = 8 }) or a plantation (plant = { radius = 6 }) in nodes
    std::optional<unsigned> collectRadius, plantRadius;
    /// Ore the mine digs (mine = { resource = "Coal" }): a built-in ore (Granite, Coal, Iron, Gold) or one of
    /// rttr:AddOre
    std::optional<std::string> mineResource;
    std::optional<MilitaryDesc> military;
    /// Warehouse: wares it holds at most (warehouse = { capacity = 400 })
    std::optional<unsigned> warehouseCapacity;

    BuiltinBuildingDesc() = default;
    explicit BuiltinBuildingDesc(const kaguya::LuaTable& luaData);
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

/// Read an output ratio (wares per work step, e.g. 1.5) from lua and return it in hundredths. Throws if it is not in
/// [0, MAX_OUTPUT_RATIO]
unsigned parseOutputRatio(double value, const std::string& context);

/// Read construction costs from lua: { Boards = 2, Stones = 1 } (one tag each, used in alphabetical order) or
/// { { tags = { "Bretter", "Nadelholz" }, amount = 2 }, ... } (used in this order)
std::vector<CostDesc> parseBuildingCost(const kaguya::LuaRef& value, const std::string& context);

/// Read a tag query from lua: Either a single string or a list of strings
TagQuery parseTagQuery(const kaguya::LuaRef& value, const std::string& context);
/// Parse { baseSpeed = 0, tools = { "Axe", ... } } (both optional)
BuildingToolsDesc parseBuildingTools(const kaguya::LuaTable& luaData, const std::string& context);
/// Maximum number of tool slots of a building
constexpr unsigned MAX_BUILDING_TOOLS = 4;
/// Maximum number of tool materials
constexpr unsigned MAX_TOOL_MATERIALS = 8;
/// Maximum output ratio (wares per work step) in hundredths, also for the sum with the tool bonus
constexpr unsigned MAX_OUTPUT_RATIO = 800;
/// Maximum number of construction materials of a building and maximum sum of their amounts
constexpr unsigned MAX_COST_ENTRIES = 4;
constexpr unsigned MAX_TOTAL_COST = 31;
/// Maximum number of entries in outputs of a workshop
constexpr unsigned MAX_WORKSHOP_OUTPUTS = 16;
/// Maximum number of inputs of a workshop (rows in the building window)
constexpr unsigned MAX_WORKSHOP_INPUTS = 10;
