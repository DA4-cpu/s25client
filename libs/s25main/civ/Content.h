// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "civ/Knowledge.h"
#include "civ/Looks.h"
#include "civ/MapCodes.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingQuality.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/BuildingTypes.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/JobTypes.h"
#include "gameTypes/Resource.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct WorldDescription;

namespace civ {

/// A list of tags. A ware matches if it has all of them
using TagQuery = std::vector<std::string>;

/// Ware defined in lua (rttr:AddWare)
struct CustomWare
{
    std::string name, title;
    /// Built-in ware whose graphics are used
    GoodType look;
    /// Built-in ware with the same transport priority
    GoodType transportLike;
    /// Knowledge points (whole points) when stored as a knowledge medium
    KnowledgePoints knowledge{};
};

/// Worker of a workshop defined in lua
struct CustomWorker
{
    /// Identifier (same as the workshop)
    std::string name;
    std::string title;
    /// Tool to recruit the worker. GoodType::Nothing: No tool required
    GoodType tool;
    /// Built-in job whose graphics are used
    Job look;
};

/// Object on the map which can be collected (rttr:AddMapObject), e.g. a loam deposit
struct CustomMapObject
{
    std::string name, title;
    /// Number of sizes = wares in a full object
    unsigned sizes = 6;
    /// Granite pile whose graphics are used (placeholder): 0 or 1
    unsigned lookGranite = 0;
    /// Color of the placeholder graphics (0xRRGGBB)
    unsigned tint = 0xFFFFFF;
    /// GFs until a planted object is grown. 0: Can not be planted
    unsigned growGFs = 0;
    /// Size of a grown planted object (1..sizes)
    unsigned plantSize = 0;

    bool CanBePlanted() const { return growGFs > 0; }
};

/// Ore in the mountains (rttr:AddOre), uses the slots ResourceType::Custom0...
struct CustomOre
{
    std::string name, title;
    /// Built-in ore whose sign is used (placeholder): Iron, Gold, Coal or Granite
    ResourceType look = ResourceType::Iron;
    /// Color of the sign (0xRRGGBB)
    unsigned tint = 0xFFFFFF;
};

/// Workshop defined in lua (rttr:AddWorkshop)
struct CustomBuilding
{
    std::string name, title, description;
    BuildingQuality size;
    BuildingCost cost;
    /// Built-in building whose graphics are used
    BuildingType look;
    /// What the building needs and produces (job is the custom worker slot)
    BldWorkDescription workDesc;
    /// Work and wait duration per product in GFs
    unsigned workGFs = 0, waitGFs = 0;

    /// Product made from the given input ware (input may be empty, e.g. if nothing was consumed yet)
    GoodType GetProduct(std::optional<GoodType> consumedInput) const;

    /// Product for each input ware if the properties of the input are passed on. Otherwise empty
    std::vector<std::pair<GoodType, GoodType>> productByInput;
    /// Product if properties are not passed on
    GoodType product = GoodType::Nothing;
    /// Possible products if the product is chosen by knowledge type (outputChoice = "knowledge"). Otherwise empty.
    /// The worker picks one of them for each product, weighted by the regulator "Wissen" of the player
    std::vector<GoodType> productChoices;
    /// Knowledge type of each entry of productChoices
    std::vector<KnowledgeType> productChoiceTypes;
    /// Library: stores up to this many knowledge media and produces nothing. 0: normal workshop
    unsigned libraryCapacity = 0;
    /// Collector: Index of the map object (Content::GetMapObject) the worker collects like the stonemason. -1: none
    int collectObject = -1;
    /// Collector: Maximum distance of the map objects
    unsigned collectRadius = 0;
    /// Plantation: Index of the map object the worker plants (and harvests if there is a product). -1: none
    int plantObject = -1;
    /// Plantation: Maximum distance of the planted objects
    unsigned plantRadius = 0;
    /// Mine: Ore the worker digs. Nothing: no mine
    ResourceType mineResource = ResourceType::Nothing;
    /// The AI may build it near a land border (otherwise that space is kept free for military buildings)
    bool aiNearBorder = false;

    bool IsLibrary() const { return libraryCapacity > 0; }
    bool IsCollector() const { return collectObject >= 0; }
    bool IsPlanter() const { return plantObject >= 0; }
    /// Plantation which also harvests its objects (has a product)
    bool HarvestsPlants() const { return IsPlanter() && product != GoodType::Nothing; }
    bool IsMine() const { return mineResource != ResourceType::Nothing; }
};

/// Material of tools (rttr:AddToolMaterial)
struct ToolMaterial
{
    std::string name, title;
    /// Default priority 0..10 of the regulator "Werkzeuge"
    uint8_t defaultPriority = 5;
};

/// Properties of a ware which is a tool used in buildings (rttr:AddTool)
struct ToolInfo
{
    /// Index into Content::GetToolMaterials
    unsigned material = 0;
    /// Number of work steps until it is broken
    unsigned durability = 0;
    /// Bonus to the work speed of the building in percent
    unsigned speed = 0;
};

/// Tools used in a building
struct BuildingTools
{
    struct Slot
    {
        TagQuery query;
        /// All tools matching the query
        std::vector<GoodType> wares;
    };
    /// Work speed without tools in percent of the normal speed
    unsigned baseSpeed = 100;
    /// One tool per slot can be in the building (plus a replacement)
    std::vector<Slot> slots;
};

/// Wares, workshops and tags of the civilization mode which are defined in lua.
/// They use the reserved custom slots of the GoodType/BuildingType/Job enums in order of definition.
class Content
{
public:
    /// Empty content (only built-in wares with their identifier as tag)
    Content();
    /// Resolve the descriptions. Throws GameDataError for invalid definitions
    explicit Content(const WorldDescription& desc);

    /// Definition of the custom ware or nullptr if it is not a custom ware or the slot is unused
    const CustomWare* GetWare(GoodType good) const;
    const CustomBuilding* GetBuilding(BuildingType bld) const;
    const CustomWorker* GetWorker(Job job) const;
    unsigned GetNumWares() const { return static_cast<unsigned>(wares_.size()); }
    unsigned GetNumBuildings() const { return static_cast<unsigned>(buildings_.size()); }
    /// Objects on the map that can be collected, in order of definition
    const std::vector<CustomMapObject>& GetMapObjects() const { return mapObjects_; }
    /// Map object with the given index or nullptr
    const CustomMapObject* GetMapObject(unsigned idx) const
    {
        return idx < mapObjects_.size() ? &mapObjects_[idx] : nullptr;
    }
    /// Index of the map object with the given identifier
    std::optional<unsigned> FindMapObject(const std::string& identifier) const;
    /// Ores in the mountains, in order of definition (ResourceType::Custom0 + index)
    const std::vector<CustomOre>& GetOres() const { return ores_; }
    /// Ore of the resource type or nullptr if it is no (defined) custom ore
    const CustomOre* GetOre(ResourceType type) const;
    /// Resource type of the ore with the given identifier
    std::optional<ResourceType> FindOre(const std::string& identifier) const;

    /// True if the ware exists in this game (built-in or defined custom ware)
    bool IsUsed(GoodType good) const;
    bool IsUsed(BuildingType bld) const;
    bool IsUsed(Job job) const;

    /// All tags of the ware: Its identifier, kind and properties
    const std::vector<std::string>& GetTags(GoodType good) const { return tags_[good]; }
    /// Properties of the ware (part of the tags)
    const std::vector<std::string>& GetProperties(GoodType good) const { return properties_[good]; }
    /// True if the ware has all tags of the query
    bool Matches(GoodType good, const TagQuery& query) const;
    /// Knowledge points (whole points) of a ware stored as knowledge medium. All 0 if it is no medium
    KnowledgePoints GetKnowledgeOf(GoodType good) const;
    /// All wares with knowledge points (knowledge media), in order of definition
    const std::vector<GoodType>& GetKnowledgeMedia() const { return knowledgeMedia_; }
    /// Default setting of the regulator "Wissen" (weight 0..10 per knowledge type)
    const helpers::EnumArray<uint8_t, KnowledgeType>& GetDefaultKnowledgeDistribution() const
    {
        return defaultKnowledgeDistribution_;
    }
    /// All usable wares which match the query
    std::vector<GoodType> FindWares(const TagQuery& query) const;

    /// Tool reform: True if tools are used in the buildings (tool materials are defined). Then workers of buildings
    /// are recruited without tools, the buildings order their tools, the tools wear out and set the work speed
    bool HasToolReform() const { return !toolMaterials_.empty(); }
    const std::vector<ToolMaterial>& GetToolMaterials() const { return toolMaterials_; }
    /// Properties of the tool or nullptr if the ware is no tool used in buildings
    const ToolInfo* GetTool(GoodType good) const;
    /// Tools of the building. Without the tool reform: none and normal speed
    const BuildingTools& GetBuildingTools(BuildingType bld) const { return buildingTools_[bld]; }
    /// A replacement is ordered when a tool has at most this many work steps left
    unsigned GetToolReorderSteps(GoodType tool) const;
    /// True if a helper can take this job without a tool (tool reform: all jobs of buildings).
    /// Builder, planer, geologist and scout still need their tool
    bool IsRecruitedWithoutTool(Job job) const;

    /// Ware (built-in or custom) for the identifier
    std::optional<GoodType> FindGood(const std::string& identifier) const;
    /// Building (built-in usable building or custom) for the identifier
    std::optional<BuildingType> FindBuilding(const std::string& identifier, bool allowHeadquarters = false) const;

    /// Names of the used custom slots in order. Used to check that a savegame fits the lua definitions
    std::vector<std::string> GetSignature() const;
    /// True if objects saved with the saved signature can be loaded with this content:
    /// Wares and workshops may be appended, but existing ones must stay in place
    bool IsCompatibleTo(const std::vector<std::string>& savedSignature) const;

private:
    void SetTags(GoodType good, const std::string& kind, const std::vector<std::string>& properties);
    /// Identifier of the ware according to this content
    std::string getIdentifierOf(GoodType good) const;

    std::vector<CustomWare> wares_;
    std::vector<CustomMapObject> mapObjects_;
    std::vector<CustomOre> ores_;
    std::vector<CustomBuilding> buildings_;
    std::vector<CustomWorker> workers_;
    helpers::EnumArray<std::vector<std::string>, GoodType> tags_, properties_;
    std::vector<GoodType> knowledgeMedia_;
    helpers::EnumArray<uint8_t, KnowledgeType> defaultKnowledgeDistribution_;
    std::vector<ToolMaterial> toolMaterials_;
    helpers::EnumArray<std::optional<ToolInfo>, GoodType> tools_;
    helpers::EnumArray<BuildingTools, BuildingType> buildingTools_;
    unsigned toolReorderPercent_ = 10;
};

/// Content of the running game. Empty unless a game in the civilization mode is running.
/// Used by global lookups which have no access to the world (names, graphics, building properties ...)
const Content& getActiveContent();
/// Set the content of the running game (nullptr: empty content)
void setActiveContent(std::shared_ptr<const Content> content);
/// Reset the active content if it is the given one
void resetActiveContent(const Content* content);

/// Built-in ware whose transport priority is used (identity for built-in wares)
GoodType getTransportLike(GoodType good);

} // namespace civ
