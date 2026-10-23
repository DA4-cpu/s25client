// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "civ/Knowledge.h"
#include "civ/Looks.h"
#include "helpers/EnumArray.h"
#include "gameTypes/BuildingQuality.h"
#include "gameTypes/BuildingType.h"
#include "gameTypes/BuildingTypes.h"
#include "gameTypes/GoodTypes.h"
#include "gameTypes/JobTypes.h"
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

    bool IsLibrary() const { return libraryCapacity > 0; }
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
    std::vector<CustomBuilding> buildings_;
    std::vector<CustomWorker> workers_;
    helpers::EnumArray<std::vector<std::string>, GoodType> tags_, properties_;
    std::vector<GoodType> knowledgeMedia_;
    helpers::EnumArray<uint8_t, KnowledgeType> defaultKnowledgeDistribution_;
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
