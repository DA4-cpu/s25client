// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Content.h"
#include "addons/AddonForesterReachRadius.h"
#include "addons/AddonStonemasonReachRadius.h"
#include "addons/AddonWoodcutterReachRadius.h"
#include "civ/Identifiers.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GameConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/MilitaryConsts.h"
#include "gameData/ShieldConsts.h"
#include "gameData/ToolConsts.h"
#include "gameData/WorldDescription.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <set>

namespace civ {

namespace {
    std::shared_ptr<const Content>& activeContentPtr()
    {
        static std::shared_ptr<const Content> content;
        return content;
    }

    bool isUsableBuiltinGood(GoodType good)
    {
        return !isCustomGood(good) && good != GoodType::WaterEmpty && ConvertShields(good) == good;
    }

    std::string joinTags(const TagQuery& query)
    {
        std::string result;
        for(const std::string& tag : query)
        {
            if(!result.empty())
                result += ", ";
            result += tag;
        }
        return "{" + result + "}";
    }

    bool sameProperties(std::vector<std::string> lhs, std::vector<std::string> rhs)
    {
        std::sort(lhs.begin(), lhs.end());
        std::sort(rhs.begin(), rhs.end());
        return lhs == rhs;
    }

    /// The built-in military buildings in the order of the original tables (gameData/MilitaryConsts.h)
    constexpr std::array<BuildingType, NUM_MILITARY_BLDS> BUILTIN_MILITARY_BUILDINGS = {
      BuildingType::Barracks, BuildingType::Guardhouse, BuildingType::Watchtower, BuildingType::Fortress};
    /// Built-in buildings the AI may build near a land border (besides the military buildings)
    constexpr std::array<BuildingType, 9> BUILTIN_NEAR_BORDER = {
      BuildingType::Woodcutter,  BuildingType::Quarry,         BuildingType::Fishery,
      BuildingType::GoldMine,    BuildingType::CoalMine,       BuildingType::IronMine,
      BuildingType::GraniteMine, BuildingType::HarborBuilding, BuildingType::Catapult};
    /// Built-in collectors (collect = { radius }) and plantations (plant = { radius })
    constexpr std::array<BuildingType, 4> BUILTIN_COLLECTORS = {BuildingType::Woodcutter, BuildingType::Quarry,
                                                                BuildingType::Fishery, BuildingType::Hunter};
    constexpr std::array<BuildingType, 4> BUILTIN_PLANTATIONS = {BuildingType::Forester, BuildingType::Farm,
                                                                 BuildingType::Vineyard, BuildingType::Charburner};
    /// Built-in buildings whose way of working stays as it is for now (Nutzer 2026-11-25): only their construction
    /// costs, knowledge points, tools, output ratio and aiNearBorder are set in lua
    constexpr std::array<BuildingType, 5> KEPT_BUILDINGS = {BuildingType::DonkeyBreeder, BuildingType::Shipyard,
                                                            BuildingType::Catapult, BuildingType::LookoutTower,
                                                            BuildingType::Temple};

    /// Radius of the built-in collectors and plantations in the civilization mode: the original values (the addons
    /// for the reach have no effect there)
    unsigned getOriginalWorkRadius(const BuildingType bld)
    {
        switch(bld)
        {
            case BuildingType::Woodcutter: return woodcutterRadiusValues[0];
            case BuildingType::Forester: return foresterRadiusValues[0];
            case BuildingType::Quarry: return stonemasonRadiusValues[0];
            case BuildingType::Fishery: return FISHER_RADIUS;
            case BuildingType::Hunter: return HUNTER_RADIUS;
            case BuildingType::Farm: return FARMER_RADIUS;
            case BuildingType::Vineyard: return WINEGROWER_RADIUS;
            case BuildingType::Charburner: return CHARBURNER_RADIUS;
            default: return 0;
        }
    }

    BuildingQuality sizeFromName(const std::string& size)
    {
        if(size == "small")
            return BuildingQuality::Hut;
        if(size == "medium")
            return BuildingQuality::House;
        if(size == "mine")
            return BuildingQuality::Mine;
        return BuildingQuality::Castle;
    }

} // namespace

GoodType CustomBuilding::GetProduct(std::optional<GoodType> consumedInput) const
{
    if(productByInput.empty())
        return product;
    if(consumedInput)
    {
        for(const auto& entry : productByInput)
        {
            if(entry.first == *consumedInput)
                return entry.second;
        }
    }
    return productByInput.front().second;
}

unsigned CustomBuilding::GetProductSteps(const GoodType good) const
{
    for(const auto& entry : productSteps)
    {
        if(entry.first == good)
            return entry.second;
    }
    return 100;
}

std::vector<GoodType> CustomBuilding::GetAllProducts() const
{
    if(!productChoices.empty())
        return productChoices;
    std::vector<GoodType> result;
    for(const auto& entry : productByInput)
    {
        if(!helpers::contains(result, entry.second))
            result.push_back(entry.second);
    }
    if(result.empty() && product != GoodType::Nothing)
        result.push_back(product);
    return result;
}

int ToolInfo::GetOutputBonus(const helpers::OptionalEnum<GoodType> product) const
{
    int result = outputRatio;
    if(product)
    {
        for(const TagBonus& entry : outputRatioByTag)
        {
            if(std::binary_search(entry.wares.begin(), entry.wares.end(), *product))
                result += entry.bonus;
        }
    }
    return result;
}

int ToolInfo::GetInputBonus(const std::vector<GoodType>& inputWares) const
{
    int result = inputRatio;
    for(const TagBonus& entry : inputRatioByTag)
    {
        if(!inputWares.empty() && std::all_of(inputWares.begin(), inputWares.end(), [&](const GoodType good) {
               return std::binary_search(entry.wares.begin(), entry.wares.end(), good);
           }))
            result += entry.bonus;
    }
    return result;
}

int ToolInfo::GetBestOutputBonus() const
{
    int best = 0;
    for(const TagBonus& entry : outputRatioByTag)
        best = std::max(best, entry.bonus);
    return outputRatio + best;
}

namespace {
    /// Jobs without a building of their own which are still recruited with their tool in the tool reform
    constexpr std::array<Job, 4> TOOL_RECRUITED_JOBS = {Job::Builder, Job::Planer, Job::Geologist, Job::Scout};
} // namespace

Content::Content()
{
    // Regulator "Wissen": all knowledge types equal unless set in lua
    std::fill(defaultKnowledgeDistribution_.begin(), defaultKnowledgeDistribution_.end(), static_cast<uint8_t>(5));
    std::fill(outputRatios_.begin(), outputRatios_.end(), static_cast<uint16_t>(100));
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(isUsableBuiltinGood(good))
            tags_[good] = {getIdentifier(good)};
    }
    // Original construction costs: boards and stones
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!isCustomBuilding(bld))
            costs_[bld] = makeBuiltinCost(bld);
    }
    initBuiltinBuildings();
    initMilitary();
}

Content::Content(const WorldDescription& desc) : Content()
{
    fromLua_ = true;
    // Wares
    if(desc.wares.size() > NUM_CUSTOM_GOODS)
        throw GameDataError("At most " + std::to_string(NUM_CUSTOM_GOODS) + " new wares can be defined");
    // Built-in wares changed by rttr:SetWare: new identifier (also the displayed name and the name tag) and tags. The
    // original identifiers stay reserved: look and transport of new wares refer to them
    std::set<std::string> wareNames;
    for(const auto& entry : desc.builtinWares)
    {
        const std::string context = "Built-in ware '" + entry.first + "' (rttr:SetWare)";
        const auto good = goodFromIdentifier(entry.first);
        if(!good || !isUsableBuiltinGood(*good))
            throw GameDataError(context + ": not a built-in ware (new wares get their tags in rttr:AddWare)");
        const BuiltinWareDesc& wareDesc = entry.second;
        if(!wareDesc.name.empty() && wareDesc.name != entry.first)
        {
            if(goodFromIdentifier(wareDesc.name) || wareDesc.name == "WaterEmpty"
               || !wareNames.insert(wareDesc.name).second)
            {
                throw GameDataError(context + ": the name '" + wareDesc.name + "' is already the name of another ware");
            }
            builtinNames_[*good] = wareDesc.name;
            tags_[*good] = {wareDesc.name};
        }
        SetTags(*good, wareDesc.kind, wareDesc.properties);
    }
    for(unsigned i = 0; i < desc.wares.size(); ++i)
    {
        const WareDesc& wareDesc = desc.wares[i];
        const std::string context = "Ware '" + wareDesc.name + "'";
        if(goodFromIdentifier(wareDesc.name) || wareDesc.name == "WaterEmpty"
           || !wareNames.insert(wareDesc.name).second)
            throw GameDataError(context + " is defined twice (or is the name of a built-in ware)");
        CustomWare ware;
        ware.name = wareDesc.name;
        ware.title = wareDesc.title;
        const auto look = goodFromIdentifier(wareDesc.look);
        if(!look)
            throw GameDataError(context + ": look must be a built-in ware, not '" + wareDesc.look + "'");
        ware.look = *look;
        const auto transport = goodFromIdentifier(wareDesc.transport);
        if(!transport)
            throw GameDataError(context + ": transport must be a built-in ware, not '" + wareDesc.transport + "'");
        ware.transportLike = *transport;
        for(const auto type : helpers::enumRange<KnowledgeType>())
            ware.knowledge[type] = wareDesc.knowledge[static_cast<unsigned>(type)];
        wares_.push_back(ware);
        tags_[getCustomGood(i)] = {ware.name};
        SetTags(getCustomGood(i), wareDesc.kind, wareDesc.properties);
    }
    for(unsigned i = 0; i < wares_.size(); ++i)
    {
        if(std::any_of(wares_[i].knowledge.begin(), wares_[i].knowledge.end(), [](int points) { return points != 0; }))
            knowledgeMedia_.push_back(getCustomGood(i));
    }
    if(desc.knowledgeDistribution)
    {
        for(const auto type : helpers::enumRange<KnowledgeType>())
        {
            defaultKnowledgeDistribution_[type] =
              static_cast<uint8_t>((*desc.knowledgeDistribution)[static_cast<unsigned>(type)]);
        }
    }

    // Map objects
    if(desc.mapObjects.size() > MAX_CUSTOM_MAP_OBJECTS)
        throw GameDataError("At most " + std::to_string(MAX_CUSTOM_MAP_OBJECTS) + " map objects can be defined");
    for(const MapObjectDesc& objDesc : desc.mapObjects)
    {
        if(FindMapObject(objDesc.name))
            throw GameDataError("Map object '" + objDesc.name + "' is defined twice");
        CustomMapObject obj;
        obj.name = objDesc.name;
        obj.title = objDesc.title;
        obj.sizes = std::min(objDesc.sizes, MAX_MAP_OBJECT_SIZES);
        obj.lookGranite = objDesc.look == "Granite2" ? 1u : 0u;
        obj.tint = objDesc.tint;
        obj.growGFs = static_cast<unsigned>(duration_to_gfs(std::chrono::seconds(objDesc.growTime)));
        if(objDesc.growTime > 0 && obj.growGFs == 0)
            obj.growGFs = 1;
        obj.plantSize = objDesc.plantSize > 0 ? objDesc.plantSize : obj.sizes;
        mapObjects_.push_back(obj);
    }

    // Ores
    if(desc.ores.size() > NUM_CUSTOM_ORES)
        throw GameDataError("At most " + std::to_string(NUM_CUSTOM_ORES) + " ores can be defined");
    for(const OreDesc& oreDesc : desc.ores)
    {
        const std::string context = "Ore '" + oreDesc.name + "'";
        if(FindOre(oreDesc.name) || FindMapObject(oreDesc.name)
           || helpers::contains(getMapGenObjectNames(), oreDesc.name))
            throw GameDataError(context + " is defined twice (or is the name of a map object)");
        CustomOre ore;
        ore.name = oreDesc.name;
        ore.title = oreDesc.title;
        if(oreDesc.look == "Gold")
            ore.look = ResourceType::Gold;
        else if(oreDesc.look == "Coal")
            ore.look = ResourceType::Coal;
        else if(oreDesc.look == "Granite")
            ore.look = ResourceType::Granite;
        else
            ore.look = ResourceType::Iron;
        ore.tint = oreDesc.tint;
        ores_.push_back(ore);
    }

    // Workshops
    if(desc.workshops.size() > NUM_CUSTOM_BUILDINGS)
        throw GameDataError("At most " + std::to_string(NUM_CUSTOM_BUILDINGS) + " workshops can be defined");
    std::set<std::string> buildingNames;
    for(unsigned i = 0; i < desc.workshops.size(); ++i)
    {
        const WorkshopDesc& wsDesc = desc.workshops[i];
        const std::string context = "Workshop '" + wsDesc.name + "'";
        if(buildingFromIdentifier(wsDesc.name, true) || wsDesc.name == "Nothing9"
           || !buildingNames.insert(wsDesc.name).second)
            throw GameDataError(context + " is defined twice (or is the name of a built-in building)");
        CustomBuilding bld;
        bld.name = wsDesc.name;
        bld.title = wsDesc.title;
        bld.description = wsDesc.description;
        if(wsDesc.size == "small")
            bld.size = BuildingQuality::Hut;
        else if(wsDesc.size == "medium")
            bld.size = BuildingQuality::House;
        else if(wsDesc.size == "mine")
            bld.size = BuildingQuality::Mine;
        else
            bld.size = BuildingQuality::Castle;

        costs_[getCustomBuilding(i)] = resolveCost(wsDesc.cost, context);

        const auto look = buildingFromIdentifier(wsDesc.look);
        if(!look)
            throw GameDataError(context + ": look must be a built-in building, not '" + wsDesc.look + "'");
        bld.look = *look;
        bld.aiNearBorder = wsDesc.aiNearBorder;

        if(wsDesc.military)
        {
            // Military building: soldiers hold the land around it, no worker (the slot of the worker stays empty)
            const auto itLook = helpers::find(BUILTIN_MILITARY_BUILDINGS, bld.look);
            if(itLook == BUILTIN_MILITARY_BUILDINGS.end())
            {
                throw GameDataError(context
                                    + ": the look of a military building must be a built-in military building "
                                      "(Barracks, Guardhouse, Watchtower or Fortress), not '"
                                    + wsDesc.look + "'");
            }
            const MilitaryDesc& milDesc = *wsDesc.military;
            MilitaryValues values;
            values.soldiers = milDesc.soldiers;
            values.radius = milDesc.radius;
            values.vision = milDesc.vision.value_or(milDesc.radius + VISUALRANGE_MILITARY);
            values.coins = milDesc.coins;
            values.armor = milDesc.armor.value_or(milDesc.coins);
            values.look = static_cast<unsigned>(itLook - BUILTIN_MILITARY_BUILDINGS.begin());
            bld.military = values;
            bld.aiNearBorder = true;
            bld.workDesc.job = std::nullopt;
            bld.workDesc.producedWare = std::nullopt;
            bld.workDesc.numSpacesPerWare = 0;
            buildings_.push_back(std::move(bld));
            CustomWorker noWorker;
            noWorker.name = wsDesc.name;
            noWorker.title = wsDesc.title;
            noWorker.look = Job::Helper;
            workers_.push_back(std::move(noWorker));
            continue;
        }

        if(wsDesc.warehouseCapacity > 0)
        {
            // Warehouse: stores the wares matching the queries, no worker (the slot of the worker stays empty)
            for(const TagQuery& query : wsDesc.warehouseWares)
            {
                const std::vector<GoodType> matching = FindWares(query);
                if(matching.empty())
                    throw GameDataError(context + ": warehouse wares " + joinTags(query) + " matches no ware");
                for(const GoodType good : matching)
                {
                    if(!helpers::contains(bld.warehouseWares, good))
                        bld.warehouseWares.push_back(good);
                }
            }
            warehouseCapacity_[getCustomBuilding(i)] = wsDesc.warehouseCapacity;
            bld.workDesc.job = std::nullopt;
            bld.workDesc.producedWare = std::nullopt;
            bld.workDesc.numSpacesPerWare = 0;
            buildings_.push_back(std::move(bld));
            CustomWorker noWorker;
            noWorker.name = wsDesc.name;
            noWorker.title = wsDesc.title;
            noWorker.look = Job::Helper;
            workers_.push_back(std::move(noWorker));
            continue;
        }

        // Worker
        CustomWorker worker;
        worker.name = wsDesc.name;
        worker.title = wsDesc.worker.title;
        const auto workerLook = jobFromIdentifier(wsDesc.worker.look);
        if(!workerLook)
            throw GameDataError(context + ": the look of the worker must be a built-in job (e.g. \"Carpenter\"), not '"
                                + wsDesc.worker.look + "'");
        worker.look = *workerLook;

        if(wsDesc.libraryCapacity > 0)
        {
            // Library: takes all knowledge media, produces nothing
            bld.libraryCapacity = wsDesc.libraryCapacity;
            if(knowledgeMedia_.empty())
                throw GameDataError(context + ": a library needs knowledge media (wares with knowledge in wares.lua)");
            bld.workDesc.job = getCustomJob(i);
            bld.workDesc.producedWare = std::nullopt;
            bld.workDesc.numSpacesPerWare = 0;
            buildings_.push_back(std::move(bld));
            workers_.push_back(std::move(worker));
            continue;
        }

        if(!wsDesc.collectObject.empty())
        {
            const auto objIdx = FindMapObject(wsDesc.collectObject);
            if(!objIdx)
                throw GameDataError(context + ": unknown map object '" + wsDesc.collectObject
                                    + "' to collect (define it with rttr:AddMapObject)");
            bld.collectObject = static_cast<int>(*objIdx);
            bld.collectRadius = wsDesc.collectRadius;
        }
        if(!wsDesc.plantObject.empty())
        {
            const auto objIdx = FindMapObject(wsDesc.plantObject);
            if(!objIdx)
                throw GameDataError(context + ": unknown map object '" + wsDesc.plantObject
                                    + "' to plant (define it with rttr:AddMapObject)");
            if(!mapObjects_[*objIdx].CanBePlanted())
                throw GameDataError(context + ": the map object '" + wsDesc.plantObject
                                    + "' can not be planted (it needs growTime in rttr:AddMapObject)");
            bld.plantObject = static_cast<int>(*objIdx);
            bld.plantRadius = wsDesc.plantRadius;
        }
        if(!wsDesc.mineResource.empty())
        {
            const auto ore = FindOre(wsDesc.mineResource);
            if(!ore)
                throw GameDataError(context + ": unknown ore '" + wsDesc.mineResource
                                    + "' to mine (define it with rttr:AddOre)");
            bld.mineResource = *ore;
        }

        // Inputs: a place for all wares of the tag query each
        for(const InputDesc& input : wsDesc.inputs)
        {
            InputSlot slot;
            slot.wares = FindWares(input.tags);
            if(slot.wares.empty())
                throw GameDataError(context + ": input " + joinTags(input.tags) + " matches no ware");
            slot.amount = static_cast<uint16_t>(input.amount);
            slot.storage = static_cast<uint8_t>(input.storage != 0 ? input.storage : wsDesc.storage);
            if(slot.amount > slot.storage * 100u)
            {
                throw GameDataError(context + ": input " + joinTags(input.tags)
                                    + " needs more wares per work step (amount) than it holds (storage)");
            }
            bld.workDesc.inputs.push_back(std::move(slot));
        }

        // Products: every ware matching an entry of outputs (an entry may match several, e.g. all carved bones)
        const bool hasOutput = !wsDesc.outputs.empty();
        std::vector<GoodType> outputMatches;
        for(unsigned k = 0; k < wsDesc.outputs.size(); ++k)
        {
            const TagQuery& query = wsDesc.outputs[k];
            const std::vector<GoodType> matches = FindWares(query);
            if(matches.empty())
                throw GameDataError(context + ": entry " + joinTags(query) + " of outputs matches no ware");
            const unsigned steps = k < wsDesc.outputSteps.size() ? wsDesc.outputSteps[k] : 100u;
            for(const GoodType good : matches)
            {
                if(helpers::contains(outputMatches, good))
                {
                    throw GameDataError(context + ": the ware '" + getIdentifierOf(good)
                                        + "' is in outputs twice (entry " + joinTags(query) + ")");
                }
                outputMatches.push_back(good);
                if(steps != 100u)
                    bld.productSteps.emplace_back(good, steps);
            }
        }
        if(!hasOutput)
        {
            // Plantation that only plants (like the forester)
            if(wsDesc.inheritProperties || !wsDesc.outputChoice.empty())
                throw GameDataError(context + ": inheritProperties and outputChoice need outputs");
        }
        if(!wsDesc.outputChoice.empty() && (bld.IsCollector() || bld.IsPlanter() || bld.IsMine()))
            throw GameDataError(context + ": collectors, plantations and mines can not choose their product");
        if(wsDesc.inheritProperties)
        {
            // The ware used from the first input decides the product
            if(bld.workDesc.inputs.empty())
                throw GameDataError(context + ": inheritProperties requires inputs");
            for(const GoodType input : bld.workDesc.inputs.front().wares)
            {
                std::vector<GoodType> products;
                for(const GoodType candidate : outputMatches)
                {
                    if(sameProperties(GetProperties(candidate), GetProperties(input)))
                        products.push_back(candidate);
                }
                if(products.size() != 1u)
                {
                    throw GameDataError(context + ": for input '" + getIdentifierOf(input) + "' "
                                        + std::to_string(products.size())
                                        + " wares of outputs have the same properties (expected exactly 1)");
                }
                bld.productByInput.emplace_back(input, products.front());
            }
        } else if(wsDesc.outputChoice == "knowledge")
        {
            // One product per knowledge type, e.g. carved bones for LWP, IWP and MWP
            for(const GoodType candidate : outputMatches)
            {
                const KnowledgePoints knowledge = GetKnowledgeOf(candidate);
                std::vector<KnowledgeType> types;
                for(const auto type : helpers::enumRange<KnowledgeType>())
                {
                    if(knowledge[type] != 0)
                        types.push_back(type);
                }
                if(types.size() != 1u)
                {
                    throw GameDataError(context + ": with outputChoice = \"knowledge\" every ware of outputs"
                                        + " needs knowledge points of exactly one type (LWP, IWP or MWP), but '"
                                        + getIdentifierOf(candidate) + "' has " + std::to_string(types.size()));
                }
                bld.productChoiceTypes.push_back(types.front());
            }
            bld.productChoice = ProductChoice::Knowledge;
            bld.productChoices = outputMatches;
            bld.product = outputMatches.front();
        } else if(!wsDesc.outputChoice.empty())
        {
            throw GameDataError(context + ": outputChoice '" + wsDesc.outputChoice
                                + "' does not exist. Only \"knowledge\" is left: a workshop with several products is "
                                  "steered by the distribution of all wares (Gesamtverteilung)");
        } else if(hasOutput && outputMatches.size() > 1u)
        {
            if(bld.IsCollector() || bld.IsPlanter() || bld.IsMine())
                throw GameDataError(context + ": collectors, plantations and mines can not choose their product");
            for(const GoodType candidate : outputMatches)
            {
                const KnowledgePoints knowledge = GetKnowledgeOf(candidate);
                if(std::any_of(knowledge.begin(), knowledge.end(), [](const auto points) { return points != 0; }))
                {
                    throw GameDataError(context + ": the product '" + getIdentifierOf(candidate)
                                        + "' is a knowledge medium: several of them need outputChoice = \"knowledge\"");
                }
            }
            bld.productChoice = ProductChoice::Distribution;
            bld.productChoices = outputMatches;
            bld.product = outputMatches.front();
        } else if(hasOutput)
            bld.product = outputMatches.front();

        bld.workDesc.job = getCustomJob(i);
        if(!hasOutput)
            bld.workDesc.producedWare = std::nullopt;
        else
            bld.workDesc.producedWare = bld.GetProduct(std::nullopt);
        bld.workDesc.useOneWareEach = true;
        outputRatios_[getCustomBuilding(i)] = static_cast<uint16_t>(wsDesc.outputRatio);
        bld.workGFs = wsDesc.workTime;
        bld.waitGFs = wsDesc.waitTime;
        bld.waitAfterGFs = wsDesc.waitAfterTime;

        buildings_.push_back(std::move(bld));
        workers_.push_back(std::move(worker));
    }

    applyBuiltinBuildings(desc);
    initMilitary();

    // Construction costs of built-in buildings: boards and stones unless set in lua. The tags "Boards" and "Stones"
    // also match new wares with them as tag
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!isCustomBuilding(bld))
            costs_[bld] = makeBuiltinCost(bld);
    }
    for(const auto& entry : desc.buildingCosts)
    {
        const std::string context = "Construction costs of building '" + entry.first + "'";
        const auto bld = buildingFromIdentifier(entry.first);
        if(!bld || !BuildingProperties::IsValid(*bld) || *bld == BuildingType::Headquarters)
            throw GameDataError(context + ": not a built-in building (workshops get them in rttr:AddWorkshop)");
        if(*bld == BuildingType::HarborBuilding)
            throw GameDataError(context + ": the harbor always costs boards and stones (expeditions carry them)");
        costs_[*bld] = resolveCost(entry.second, context);
    }

    // Capacity of the warehouses
    for(const auto& entry : desc.warehouseCapacity)
    {
        const auto bld = FindBuilding(entry.first, true);
        if(!bld || !BuildingProperties::IsWareHouse(*bld))
            throw GameDataError("Capacity of the warehouses: '" + entry.first
                                + "' is no warehouse (Headquarters, Storehouse, HarborBuilding)");
        warehouseCapacity_[*bld] = entry.second;
    }
    flagWorkLimit_ = desc.flagWorkLimit;

    // Output ratio of built-in buildings (workshops have it in rttr:AddWorkshop)
    for(const auto& entry : desc.buildingOutputRatios)
    {
        const auto bld = buildingFromIdentifier(entry.first);
        if(!bld || !BuildingProperties::IsUsual(*bld) || !BUILTIN_BLD_WORK_DESC[static_cast<unsigned>(*bld)].job)
        {
            throw GameDataError("Output ratio of building '" + entry.first
                                + "': not a built-in building with a worker (workshops get it in rttr:AddWorkshop)");
        }
        outputRatios_[*bld] = static_cast<uint16_t>(entry.second);
    }

    // Tool reform: materials, tools and the tools of the buildings
    if(desc.toolMaterials.empty())
    {
        if(!desc.tools.empty() || !desc.buildingTools.empty() || !desc.toolMaterialPriorities.empty())
            throw GameDataError("Tools need tool materials (rttr:AddToolMaterial)");
        for(const WorkshopDesc& wsDesc : desc.workshops)
        {
            if(wsDesc.tools)
                throw GameDataError("Workshop '" + wsDesc.name + "': tools need tool materials (rttr:AddToolMaterial)");
        }
        return;
    }
    toolReorderPercent_ = desc.toolReorderPercent;
    for(const ToolMaterialDesc& materialDesc : desc.toolMaterials)
    {
        ToolMaterial material;
        material.name = materialDesc.name;
        material.title = materialDesc.title;
        const auto itPriority = desc.toolMaterialPriorities.find(material.name);
        if(itPriority != desc.toolMaterialPriorities.end())
            material.defaultPriority = static_cast<uint8_t>(itPriority->second);
        toolMaterials_.push_back(std::move(material));
    }
    for(const auto& entry : desc.toolMaterialPriorities)
    {
        if(!helpers::contains_if(toolMaterials_,
                                 [&](const ToolMaterial& material) { return material.name == entry.first; }))
        {
            throw GameDataError("rttr:SetToolMaterialPriorities: unknown tool material '" + entry.first
                                + "' (define it with rttr:AddToolMaterial)");
        }
    }
    for(const ToolDesc& toolDesc : desc.tools)
    {
        const std::string context = "Tool '" + toolDesc.ware + "'";
        const auto good = FindGood(toolDesc.ware);
        if(!good)
            throw GameDataError(context + ": unknown ware");
        const auto itMaterial =
          helpers::find_if(toolMaterials_, [&](const ToolMaterial& mat) { return mat.name == toolDesc.material; });
        if(itMaterial == toolMaterials_.end())
            throw GameDataError(context + ": unknown material '" + toolDesc.material
                                + "' (define it with rttr:AddToolMaterial)");
        ToolInfo tool;
        tool.material = static_cast<unsigned>(itMaterial - toolMaterials_.begin());
        tool.durability = toolDesc.durability;
        tool.speed = toolDesc.speed;
        tool.outputRatio = toolDesc.outputRatio;
        tool.inputRatio = toolDesc.inputRatio;
        const auto resolveTagBonuses = [&](const std::vector<std::pair<std::string, int>>& byTag,
                                           const std::string& field) {
            std::vector<ToolInfo::TagBonus> result;
            for(const auto& entry : byTag)
            {
                ToolInfo::TagBonus bonus;
                bonus.wares = FindWares(TagQuery{entry.first});
                if(bonus.wares.empty())
                    throw GameDataError(context + ", " + field + ": no ware has the tag '" + entry.first + "'");
                std::sort(bonus.wares.begin(), bonus.wares.end());
                bonus.bonus = entry.second;
                result.push_back(std::move(bonus));
            }
            return result;
        };
        tool.outputRatioByTag = resolveTagBonuses(toolDesc.outputRatioByTag, "outputRatio");
        tool.inputRatioByTag = resolveTagBonuses(toolDesc.inputRatioByTag, "inputRatio");
        tools_[*good] = tool;
    }
    const auto resolveTools = [this](const BuildingToolsDesc& toolsDesc, const std::string& context) {
        BuildingTools result;
        result.baseSpeed = toolsDesc.baseSpeed;
        for(const TagQuery& query : toolsDesc.tools)
        {
            BuildingTools::Slot slot;
            slot.query = query;
            for(const GoodType good : FindWares(query))
            {
                if(tools_[good])
                    slot.wares.push_back(good);
            }
            if(slot.wares.empty())
                throw GameDataError(context + ": no tool (rttr:AddTool) matches '" + query.front() + "'");
            for(const BuildingTools::Slot& other : result.slots)
            {
                for(const GoodType good : slot.wares)
                {
                    if(helpers::contains(other.wares, good))
                        throw GameDataError(context + ": the tool '" + getIdentifierOf(good)
                                            + "' fits to several of its tool entries");
                }
            }
            result.slots.push_back(std::move(slot));
        }
        return result;
    };
    // Default of built-in buildings: The tool of the worker and no work without it. Workers without a tool and
    // workshops without tools work as before (full speed)
    const auto makeDefaultTools = [this](const GoodType workerTool) {
        BuildingToolsDesc toolsDesc;
        if(workerTool != GoodType::Nothing && tools_[workerTool])
            toolsDesc.tools.push_back(TagQuery{getIdentifierOf(workerTool)});
        else
            toolsDesc.baseSpeed = 100;
        return toolsDesc;
    };
    std::set<BuildingType> definedBuildings;
    for(const auto& entry : desc.buildingTools)
    {
        const std::string context = "Tools of building '" + entry.first + "'";
        const auto bld = buildingFromIdentifier(entry.first);
        if(!bld || !BuildingProperties::IsUsual(*bld) || !BUILTIN_BLD_WORK_DESC[static_cast<unsigned>(*bld)].job)
            throw GameDataError(context
                                + ": not a built-in building with a worker (workshops get their tools in "
                                  "rttr:AddWorkshop)");
        buildingTools_[*bld] = resolveTools(entry.second, context);
        definedBuildings.insert(*bld);
    }
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(isCustomBuilding(bld) || definedBuildings.count(bld) || !BuildingProperties::IsUsual(bld))
            continue;
        const auto job = BUILTIN_BLD_WORK_DESC[static_cast<unsigned>(bld)].job;
        if(!job || isCustomJob(*job))
            continue;
        const auto workerTool = JOB_CONSTS[*job].tool;
        // The scout of the lookout tower is still recruited with his tool
        const bool usesTool = workerTool && !helpers::contains(TOOL_RECRUITED_JOBS, *job);
        buildingTools_[bld] = resolveTools(makeDefaultTools(usesTool ? *workerTool : GoodType::Nothing),
                                           std::string("Tools of building '") + getIdentifier(bld) + "'");
    }
    for(unsigned i = 0; i < buildings_.size(); ++i)
    {
        const WorkshopDesc& wsDesc = desc.workshops[i];
        buildingTools_[getCustomBuilding(i)] = resolveTools(
          wsDesc.tools ? *wsDesc.tools : makeDefaultTools(GoodType::Nothing), "Workshop '" + wsDesc.name + "'");
    }
}

const ToolInfo* Content::GetTool(const GoodType good) const
{
    return tools_[good] ? &*tools_[good] : nullptr;
}

unsigned Content::GetToolReorderSteps(const GoodType tool) const
{
    const ToolInfo* info = GetTool(tool);
    if(!info)
        return 0;
    return std::max(1u, info->durability * toolReorderPercent_ / 100u);
}

bool Content::IsRecruitedWithoutTool(const Job job) const
{
    if(!HasToolReform() || helpers::contains(TOOL_RECRUITED_JOBS, job))
        return false;
    // Only jobs which are recruited from helpers at all
    return JOB_CONSTS[job].tool.has_value();
}

std::string Content::getIdentifierOf(GoodType good) const
{
    if(const CustomWare* ware = GetWare(good))
        return ware->name;
    if(!isCustomGood(good) && good != GoodType::Nothing && !builtinNames_[good].empty())
        return builtinNames_[good];
    return getIdentifier(good);
}

void Content::SetTags(GoodType good, const std::vector<std::string>& kinds, const std::vector<std::string>& properties)
{
    std::vector<std::string>& tags = tags_[good];
    for(const std::string& kind : kinds)
    {
        if(!helpers::contains(tags, kind))
            tags.push_back(kind);
    }
    for(const std::string& property : properties)
    {
        if(!helpers::contains(tags, property))
            tags.push_back(property);
        if(!helpers::contains(properties_[good], property))
            properties_[good].push_back(property);
    }
}

const CustomWare* Content::GetWare(GoodType good) const
{
    if(!isCustomGood(good))
        return nullptr;
    const unsigned idx = getCustomGoodIdx(good);
    return idx < wares_.size() ? &wares_[idx] : nullptr;
}

bool Content::IsCustomWarehouse(const BuildingType bld) const
{
    const CustomBuilding* building = GetBuilding(bld);
    return building && building->IsWarehouse();
}

bool Content::IsCustomMilitary(const BuildingType bld) const
{
    const CustomBuilding* building = GetBuilding(bld);
    return building && building->IsMilitary();
}

MilitaryValues Content::GetMilitaryValues(const BuildingType bld, const Nation nation) const
{
    if(const CustomBuilding* building = GetBuilding(bld))
        return building->military.value_or(MilitaryValues{});
    const auto itBuiltin = helpers::find(BUILTIN_MILITARY_BUILDINGS, bld);
    if(itBuiltin == BUILTIN_MILITARY_BUILDINGS.end())
        return MilitaryValues{};
    const auto idx = static_cast<unsigned>(itBuiltin - BUILTIN_MILITARY_BUILDINGS.begin());
    MilitaryValues values;
    if(const auto& military = builtin_[static_cast<unsigned>(bld)].military)
        values = *military;
    else
    {
        values.soldiers = static_cast<unsigned>(NUM_TROOPS[nation][idx]);
        values.radius = MILITARY_RADIUS[idx];
        values.vision = MILITARY_RADIUS[idx] + VISUALRANGE_MILITARY;
        values.coins = static_cast<unsigned>(NUM_GOLDS[nation][idx]);
        values.armor = static_cast<unsigned>(NUM_ARMOR[nation][idx]);
    }
    // Graphics and flags of the look (another built-in military building if set)
    const auto itLook = helpers::find(BUILTIN_MILITARY_BUILDINGS, GetBuiltinLook(bld));
    values.look = itLook != BUILTIN_MILITARY_BUILDINGS.end() ?
                    static_cast<unsigned>(itLook - BUILTIN_MILITARY_BUILDINGS.begin()) :
                    idx;
    return values;
}

void Content::initBuiltinBuildings()
{
    for(unsigned i = 0; i < NUM_BUILTIN_BUILDINGS; ++i)
    {
        const auto bld = static_cast<BuildingType>(i);
        BuiltinBuilding& building = builtin_[i];
        building.size = BUILTIN_BUILDING_SIZE[i];
        building.look = bld;
        // One input per ware the building needs
        building.workDesc = BUILTIN_BLD_WORK_DESC[i];
        for(const GoodType good : building.workDesc.waresNeeded)
            building.workDesc.inputs.push_back(InputSlot{{good}, 100, building.workDesc.numSpacesPerWare});
        if(building.workDesc.job)
        {
            const JobConst jobConst = JOB_CONSTS[*building.workDesc.job];
            building.times = WorkTimes{jobConst.work_length, jobConst.wait1_length, jobConst.wait2_length};
        }
        building.aiNearBorder = helpers::contains(BUILTIN_NEAR_BORDER, bld);
    }
    builtin_[static_cast<unsigned>(BuildingType::GraniteMine)].mineResource = ResourceType::Granite;
    builtin_[static_cast<unsigned>(BuildingType::CoalMine)].mineResource = ResourceType::Coal;
    builtin_[static_cast<unsigned>(BuildingType::IronMine)].mineResource = ResourceType::Iron;
    builtin_[static_cast<unsigned>(BuildingType::GoldMine)].mineResource = ResourceType::Gold;
    for(unsigned i = 0; i < NUM_BUILTIN_JOBS; ++i)
        builtinJobLooks_[i] = static_cast<Job>(i);
}

void Content::applyBuiltinBuildings(const WorldDescription& desc)
{
    // Civilization mode: the collectors and plantations use their original radius (also if not set in lua)
    for(const BuildingType bld : BUILTIN_COLLECTORS)
        builtin_[static_cast<unsigned>(bld)].workRadius = getOriginalWorkRadius(bld);
    for(const BuildingType bld : BUILTIN_PLANTATIONS)
        builtin_[static_cast<unsigned>(bld)].workRadius = getOriginalWorkRadius(bld);

    // Name and graphics set for a job: job -> (title, look, building that set it)
    struct JobSetting
    {
        std::optional<std::string> title, look;
        std::string building;
    };
    std::map<Job, JobSetting> jobSettings;
    for(const BuiltinBuildingDesc& bldDesc : desc.builtinBuildings)
    {
        const std::string context = "Building '" + bldDesc.name + "' (rttr:SetBuilding)";
        const auto bld = buildingFromIdentifier(bldDesc.name, true);
        if(!bld || isCustomBuilding(*bld) || !BuildingProperties::IsValid(*bld))
            throw GameDataError(context + ": not a built-in building (workshops are defined with rttr:AddWorkshop)");
        BuiltinBuilding& building = builtin_[static_cast<unsigned>(*bld)];
        const bool isMilitary = helpers::contains(BUILTIN_MILITARY_BUILDINGS, *bld);
        const bool isMine = building.mineResource != ResourceType::Nothing;

        // Fields the building may have
        std::vector<std::string> allowed;
        std::string kind;
        if(*bld == BuildingType::Headquarters)
        {
            allowed = {"knowledge", "warehouse"};
            kind = "the headquarters";
        } else if(*bld == BuildingType::HarborBuilding)
        {
            allowed = {"knowledge", "warehouse", "aiNearBorder"};
            kind = "the harbor";
        } else if(*bld == BuildingType::Storehouse)
        {
            allowed = {"title", "description", "size", "cost", "look", "knowledge", "aiNearBorder", "warehouse"};
            kind = "a warehouse";
        } else if(isMilitary)
        {
            allowed = {"title", "description", "size", "cost", "look", "knowledge", "military"};
            kind = "a military building";
        } else if(helpers::contains(KEPT_BUILDINGS, *bld))
        {
            allowed = {"cost", "knowledge", "tools", "outputRatio", "aiNearBorder"};
            kind = "a building whose way of working stays as it is for now";
        } else
        {
            allowed = {"title",     "description", "size",        "cost",     "look",
                       "knowledge", "tools",       "outputRatio", "waitTime", "aiNearBorder"};
            if(*bld != BuildingType::Well) // its worker is the helper
                allowed.push_back("worker");
            // The hunter has no work time of his own (his work is the hunt: walking, shooting, skinning)
            if(*bld != BuildingType::Hunter)
                allowed.push_back("workTime");
            // Only workers who make their wares in the building wait after a work step (not collectors and
            // plantations, they go out again after waitTime)
            const bool isCollector = helpers::contains(BUILTIN_COLLECTORS, *bld);
            const bool isPlantation = helpers::contains(BUILTIN_PLANTATIONS, *bld);
            if(!isCollector && !isPlantation)
                allowed.push_back("waitAfterTime");
            // Places only for the wares of inputs
            if(!building.workDesc.inputs.empty())
                allowed.push_back("storage");
            if(isCollector)
                allowed.push_back("collect");
            if(isPlantation)
                allowed.push_back("plant");
            if(isMine)
                allowed.push_back("mine");
            kind = "this building";
        }
        for(const std::string& field : bldDesc.fields)
        {
            if(!helpers::contains(allowed, field))
                throw GameDataError(context + ": " + kind + " has no '" + field + "'");
        }

        if(bldDesc.title)
            building.title = *bldDesc.title;
        if(bldDesc.description)
            building.description = *bldDesc.description;
        if(bldDesc.size)
        {
            const BuildingQuality size = sizeFromName(*bldDesc.size);
            if((size == BuildingQuality::Mine) != isMine)
                throw GameDataError(
                  context + (isMine ? ": a mine needs size = \"mine\"" : ": only a mine has size = \"mine\""));
            building.size = size;
        }
        if(bldDesc.look)
        {
            const auto look = buildingFromIdentifier(*bldDesc.look);
            if(!look || isCustomBuilding(*look))
                throw GameDataError(context + ": look must be a built-in building, not '" + *bldDesc.look + "'");
            if(isMilitary && !helpers::contains(BUILTIN_MILITARY_BUILDINGS, *look))
            {
                throw GameDataError(context
                                    + ": the look of a military building must be a built-in military building "
                                      "(Barracks, Guardhouse, Watchtower or Fortress), not '"
                                    + *bldDesc.look + "'");
            }
            building.look = *look;
        }
        if(bldDesc.workerTitle || bldDesc.workerLook)
        {
            const Job job = *building.workDesc.job;
            JobSetting& setting = jobSettings[job];
            const auto differs = [](const std::optional<std::string>& lhs, const std::optional<std::string>& rhs) {
                return lhs && rhs && *lhs != *rhs;
            };
            if(differs(setting.title, bldDesc.workerTitle) || differs(setting.look, bldDesc.workerLook))
            {
                throw GameDataError(context + ": the worker (job '" + getIdentifier(job)
                                    + "') has another title or look at '" + setting.building
                                    + "'. A job is the same in all buildings that use it");
            }
            if(bldDesc.workerTitle)
            {
                setting.title = bldDesc.workerTitle;
                builtinJobTitles_[static_cast<unsigned>(job)] = *bldDesc.workerTitle;
            }
            if(bldDesc.workerLook)
            {
                const auto look = jobFromIdentifier(*bldDesc.workerLook);
                if(!look || isCustomJob(*look))
                {
                    throw GameDataError(context
                                        + ": the look of the worker must be a built-in job (e.g. \"Carpenter\"), not '"
                                        + *bldDesc.workerLook + "'");
                }
                setting.look = bldDesc.workerLook;
                builtinJobLooks_[static_cast<unsigned>(job)] = *look;
            }
            setting.building = bldDesc.name;
        }
        if(bldDesc.storage)
        {
            building.workDesc.numSpacesPerWare = static_cast<uint8_t>(*bldDesc.storage);
            for(InputSlot& input : building.workDesc.inputs)
                input.storage = static_cast<uint8_t>(*bldDesc.storage);
        }
        if(bldDesc.workTime)
            building.times.work = *bldDesc.workTime;
        if(bldDesc.waitTime)
            building.times.wait = *bldDesc.waitTime;
        if(bldDesc.waitAfterTime)
            building.times.waitAfter = *bldDesc.waitAfterTime;
        if(bldDesc.aiNearBorder)
            building.aiNearBorder = *bldDesc.aiNearBorder;
        if(bldDesc.collectRadius)
            building.workRadius = *bldDesc.collectRadius;
        if(bldDesc.plantRadius)
            building.workRadius = *bldDesc.plantRadius;
        if(bldDesc.mineResource)
        {
            const std::string& name = *bldDesc.mineResource;
            if(name == "Granite")
                building.mineResource = ResourceType::Granite;
            else if(name == "Coal")
                building.mineResource = ResourceType::Coal;
            else if(name == "Iron")
                building.mineResource = ResourceType::Iron;
            else if(name == "Gold")
                building.mineResource = ResourceType::Gold;
            else if(const auto ore = FindOre(name))
                building.mineResource = *ore;
            else
            {
                throw GameDataError(context + ": mine resource '" + name
                                    + "' is no ore (Granite, Coal, Iron, Gold or one of rttr:AddOre)");
            }
        }
        if(bldDesc.military)
        {
            const MilitaryDesc& milDesc = *bldDesc.military;
            MilitaryValues values;
            values.soldiers = milDesc.soldiers;
            values.radius = milDesc.radius;
            values.vision = milDesc.vision.value_or(milDesc.radius + VISUALRANGE_MILITARY);
            values.coins = milDesc.coins;
            values.armor = milDesc.armor.value_or(milDesc.coins);
            building.military = values;
        }
    }
}

const std::string& Content::GetBuiltinTitle(const BuildingType bld) const
{
    return builtin_[static_cast<unsigned>(bld)].title;
}

const std::string& Content::GetBuiltinDescription(const BuildingType bld) const
{
    return builtin_[static_cast<unsigned>(bld)].description;
}

BuildingQuality Content::GetBuiltinSize(const BuildingType bld) const
{
    return builtin_[static_cast<unsigned>(bld)].size;
}

BuildingType Content::GetBuiltinLook(const BuildingType bld) const
{
    return isCustomBuilding(bld) ? bld : builtin_[static_cast<unsigned>(bld)].look;
}

const BldWorkDescription& Content::GetBuiltinWorkDesc(const BuildingType bld) const
{
    return builtin_[static_cast<unsigned>(bld)].workDesc;
}

WorkTimes Content::GetWorkTimes(const BuildingType bld) const
{
    if(isCustomBuilding(bld))
    {
        const CustomBuilding* building = GetBuilding(bld);
        if(!building)
            return WorkTimes{};
        return WorkTimes{std::max(building->workGFs, 1u), std::max(building->waitGFs, 1u), building->waitAfterGFs};
    }
    return builtin_[static_cast<unsigned>(bld)].times;
}

std::optional<unsigned> Content::GetWorkRadius(const BuildingType bld) const
{
    if(isCustomBuilding(bld))
    {
        const CustomBuilding* building = GetBuilding(bld);
        if(!building || (!building->IsCollector() && !building->IsPlanter()))
            return std::nullopt;
        return std::max(building->collectRadius, building->plantRadius);
    }
    return builtin_[static_cast<unsigned>(bld)].workRadius;
}

ResourceType Content::GetMineResource(const BuildingType bld) const
{
    return isCustomBuilding(bld) ? ResourceType::Nothing : builtin_[static_cast<unsigned>(bld)].mineResource;
}

bool Content::IsAINearBorder(const BuildingType bld) const
{
    if(helpers::contains(BUILTIN_MILITARY_BUILDINGS, bld) || IsCustomMilitary(bld))
        return true;
    if(isCustomBuilding(bld))
    {
        const CustomBuilding* building = GetBuilding(bld);
        return building && building->aiNearBorder;
    }
    return builtin_[static_cast<unsigned>(bld)].aiNearBorder;
}

const std::string& Content::GetBuiltinJobTitle(const Job job) const
{
    static const std::string empty;
    return isCustomJob(job) ? empty : builtinJobTitles_[static_cast<unsigned>(job)];
}

Job Content::GetBuiltinJobLook(const Job job) const
{
    return isCustomJob(job) ? job : builtinJobLooks_[static_cast<unsigned>(job)];
}

void Content::initMilitary()
{
    militaryBuildings_.assign(BUILTIN_MILITARY_BUILDINGS.begin(), BUILTIN_MILITARY_BUILDINGS.end());
    for(unsigned i = 0; i < buildings_.size(); ++i)
    {
        if(buildings_[i].IsMilitary())
            militaryBuildings_.push_back(getCustomBuilding(i));
    }
    // By soldiers, then radius; equal ones stay in the order of definition (built-in ones first)
    std::stable_sort(militaryBuildings_.begin(), militaryBuildings_.end(),
                     [this](const BuildingType lhs, const BuildingType rhs) {
                         const MilitaryValues lhsValues = GetMilitaryValues(lhs);
                         const MilitaryValues rhsValues = GetMilitaryValues(rhs);
                         if(lhsValues.soldiers != rhsValues.soldiers)
                             return lhsValues.soldiers < rhsValues.soldiers;
                         return lhsValues.radius < rhsValues.radius;
                     });
    // Land around a building reaches radius + 2 (GameWorld::RecalcTerritory), the land of another building reaches
    // its radius: they can meet if the buildings are up to 2 * the biggest radius + 2 apart. The sight of a building
    // reaches its vision
    unsigned maxRadius = std::max(HQ_RADIUS, HARBOR_RADIUS), maxVision = 0;
    for(const BuildingType bld : militaryBuildings_)
    {
        const MilitaryValues values = GetMilitaryValues(bld);
        maxRadius = std::max(maxRadius, values.radius);
        maxVision = std::max(maxVision, values.vision);
    }
    const unsigned maxDistance = std::max(2 * maxRadius + 2, maxVision);
    militarySearchSquares_ = std::max(3u, (maxDistance + MILITARY_SQUARE_SIZE - 1) / MILITARY_SQUARE_SIZE);
}

bool Content::WarehouseStores(const BuildingType bld, const GoodType good) const
{
    const CustomBuilding* building = GetBuilding(bld);
    return !building || !building->IsWarehouse() || helpers::contains(building->warehouseWares, good);
}

const CustomBuilding* Content::GetBuilding(BuildingType bld) const
{
    if(!isCustomBuilding(bld))
        return nullptr;
    const unsigned idx = getCustomBuildingIdx(bld);
    return idx < buildings_.size() ? &buildings_[idx] : nullptr;
}

const CustomWorker* Content::GetWorker(Job job) const
{
    if(!isCustomJob(job))
        return nullptr;
    const unsigned idx = getCustomJobIdx(job);
    return idx < workers_.size() ? &workers_[idx] : nullptr;
}

bool Content::IsUsed(GoodType good) const
{
    return isCustomGood(good) ? GetWare(good) != nullptr : good != GoodType::Nothing;
}

bool Content::IsUsed(BuildingType bld) const
{
    return isCustomBuilding(bld) ? GetBuilding(bld) != nullptr : bld != BuildingType::Nothing9;
}

bool Content::IsUsed(Job job) const
{
    return !isCustomJob(job) || GetWorker(job) != nullptr;
}

KnowledgePoints Content::GetKnowledgeOf(GoodType good) const
{
    const CustomWare* ware = GetWare(good);
    return ware ? ware->knowledge : KnowledgePoints{};
}

bool Content::Matches(GoodType good, const TagQuery& query) const
{
    const std::vector<std::string>& tags = tags_[good];
    if(tags.empty())
        return false;
    return std::all_of(query.begin(), query.end(),
                       [&tags](const std::string& tag) { return helpers::contains(tags, tag); });
}

std::vector<GoodType> Content::FindWares(const TagQuery& query) const
{
    std::vector<GoodType> result;
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(Matches(good, query))
            result.push_back(good);
    }
    return result;
}

BuildingCosts Content::makeBuiltinCost(const BuildingType bld) const
{
    BuildingCosts result;
    const BuildingCost cost = BUILTIN_BUILDING_COSTS[static_cast<unsigned>(bld)];
    const auto add = [&](const GoodType good, const uint8_t amount) {
        if(amount == 0)
            return;
        CostEntry entry;
        entry.tags = {getIdentifier(good)};
        entry.wares = FindWares(entry.tags);
        if(entry.wares.empty())
            entry.wares = {good};
        entry.amount = amount;
        result.push_back(std::move(entry));
    };
    add(GoodType::Boards, cost.boards);
    add(GoodType::Stones, cost.stones);
    return result;
}

BuildingCosts Content::resolveCost(const std::vector<CostDesc>& desc, const std::string& context) const
{
    BuildingCosts result;
    for(const CostDesc& entryDesc : desc)
    {
        CostEntry entry;
        entry.tags = entryDesc.tags;
        entry.wares = FindWares(entryDesc.tags);
        if(entry.wares.empty())
            throw GameDataError(context + ": " + joinTags(entryDesc.tags) + " matches no ware");
        entry.amount = static_cast<uint8_t>(entryDesc.amount);
        result.push_back(std::move(entry));
    }
    if(result.empty())
        throw GameDataError(context + ": construction costs are missing");
    return result;
}

std::optional<Tool> Content::GetToolTypeOf(const GoodType good) const
{
    std::optional<Tool> result;
    for(const auto tool : helpers::enumRange<Tool>())
    {
        // A built-in tool is always of its type, also when it was renamed (rttr:SetWare)
        if(TOOL_TO_GOOD[tool] == good)
            return tool;
    }
    if(tags_[good].empty())
        return result;
    // Identifiers of the built-in tools (the original ones, they are no custom wares), built only once
    static const helpers::EnumArray<std::string, Tool> toolIds = [] {
        helpers::EnumArray<std::string, Tool> ids;
        for(const auto tool : helpers::enumRange<Tool>())
            ids[tool] = getIdentifier(TOOL_TO_GOOD[tool]);
        return ids;
    }();
    for(const auto tool : helpers::enumRange<Tool>())
    {
        if(helpers::contains(tags_[good], toolIds[tool]))
        {
            if(result)
                return std::nullopt;
            result = tool;
        }
    }
    return result;
}

std::optional<GoodType> Content::FindGood(const std::string& identifier) const
{
    // Renamed built-in wares (rttr:SetWare) are only found by their new identifier
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(!identifier.empty() && builtinNames_[good] == identifier)
            return good;
    }
    if(const auto good = goodFromIdentifier(identifier))
    {
        if(!builtinNames_[*good].empty())
            return std::nullopt;
        return good;
    }
    for(unsigned i = 0; i < wares_.size(); ++i)
    {
        if(wares_[i].name == identifier)
            return getCustomGood(i);
    }
    return std::nullopt;
}

std::optional<BuildingType> Content::FindBuilding(const std::string& identifier, bool allowHeadquarters) const
{
    if(const auto bld = buildingFromIdentifier(identifier, allowHeadquarters))
        return bld;
    for(unsigned i = 0; i < buildings_.size(); ++i)
    {
        if(buildings_[i].name == identifier)
            return getCustomBuilding(i);
    }
    return std::nullopt;
}

std::optional<unsigned> Content::FindMapObject(const std::string& identifier) const
{
    for(unsigned i = 0; i < mapObjects_.size(); ++i)
    {
        if(mapObjects_[i].name == identifier)
            return i;
    }
    return std::nullopt;
}

const CustomOre* Content::GetOre(ResourceType type) const
{
    if(!isCustomOre(type))
        return nullptr;
    const unsigned idx = getCustomOreIdx(type);
    return idx < ores_.size() ? &ores_[idx] : nullptr;
}

std::optional<ResourceType> Content::FindOre(const std::string& identifier) const
{
    for(unsigned i = 0; i < ores_.size(); ++i)
    {
        if(ores_[i].name == identifier)
            return getCustomOre(i);
    }
    return std::nullopt;
}

std::vector<std::string> Content::GetSignature() const
{
    std::vector<std::string> result;
    for(const CustomWare& ware : wares_)
        result.push_back("W:" + ware.name);
    for(const CustomBuilding& bld : buildings_)
        result.push_back("B:" + bld.name);
    for(const CustomMapObject& obj : mapObjects_)
        result.push_back("O:" + obj.name);
    for(const CustomOre& ore : ores_)
        result.push_back("R:" + ore.name);
    return result;
}

bool Content::IsCompatibleTo(const std::vector<std::string>& savedSignature) const
{
    std::vector<std::string> savedWares, savedBuildings, savedObjects, savedOres;
    for(const std::string& entry : savedSignature)
    {
        if(entry.compare(0, 2, "W:") == 0)
            savedWares.push_back(entry.substr(2));
        else if(entry.compare(0, 2, "B:") == 0)
            savedBuildings.push_back(entry.substr(2));
        else if(entry.compare(0, 2, "O:") == 0)
            savedObjects.push_back(entry.substr(2));
        else if(entry.compare(0, 2, "R:") == 0)
            savedOres.push_back(entry.substr(2));
        else
            return false;
    }
    if(savedWares.size() > wares_.size() || savedBuildings.size() > buildings_.size()
       || savedObjects.size() > mapObjects_.size() || savedOres.size() > ores_.size())
        return false;
    for(unsigned i = 0; i < savedOres.size(); ++i)
    {
        if(savedOres[i] != ores_[i].name)
            return false;
    }
    for(unsigned i = 0; i < savedObjects.size(); ++i)
    {
        if(savedObjects[i] != mapObjects_[i].name)
            return false;
    }
    for(unsigned i = 0; i < savedWares.size(); ++i)
    {
        if(savedWares[i] != wares_[i].name)
            return false;
    }
    for(unsigned i = 0; i < savedBuildings.size(); ++i)
    {
        if(savedBuildings[i] != buildings_[i].name)
            return false;
    }
    return true;
}

const Content& getActiveContent()
{
    static const Content emptyContent;
    const auto& content = activeContentPtr();
    return content ? *content : emptyContent;
}

void setActiveContent(std::shared_ptr<const Content> content)
{
    activeContentPtr() = std::move(content);
}

void resetActiveContent(const Content* content)
{
    if(activeContentPtr().get() == content)
        activeContentPtr().reset();
}

GoodType getTransportLike(GoodType good)
{
    if(!isCustomGood(good))
        return good;
    const CustomWare* ware = getActiveContent().GetWare(good);
    return ware ? ware->transportLike : GoodType::Stones;
}

GoodType getLook(GoodType good)
{
    if(!isCustomGood(good))
        return good;
    const CustomWare* ware = getActiveContent().GetWare(good);
    return ware ? ware->look : GoodType::Stones;
}

BuildingType getLook(BuildingType bld)
{
    if(!isCustomBuilding(bld))
        return getActiveContent().GetBuiltinLook(bld);
    const CustomBuilding* building = getActiveContent().GetBuilding(bld);
    return building ? building->look : BuildingType::Woodcutter;
}

Job getLook(Job job)
{
    if(!isCustomJob(job))
        return getActiveContent().GetBuiltinJobLook(job);
    const CustomWorker* worker = getActiveContent().GetWorker(job);
    return worker ? worker->look : Job::Helper;
}

} // namespace civ
