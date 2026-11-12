// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "civ/Content.h"
#include "civ/Identifiers.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GameConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/ShieldConsts.h"
#include "gameData/ToolConsts.h"
#include "gameData/WorldDescription.h"
#include <algorithm>
#include <array>
#include <chrono>
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

    constexpr std::array<GoodType, 12> TOOLS = {GoodType::Tongs,    GoodType::Hammer,     GoodType::Axe,
                                                GoodType::Saw,      GoodType::PickAxe,    GoodType::Shovel,
                                                GoodType::Crucible, GoodType::RodAndLine, GoodType::Scythe,
                                                GoodType::Cleaver,  GoodType::Rollingpin, GoodType::Bow};

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

    WaresNeeded makeWaresNeeded(const std::vector<GoodType>& goods)
    {
        switch(goods.size())
        {
            case 0: return WaresNeeded();
            case 1: return WaresNeeded(goods[0]);
            case 2: return WaresNeeded(goods[0], goods[1]);
            case 3: return WaresNeeded(goods[0], goods[1], goods[2]);
            default: throw GameDataError("Too many input wares");
        }
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
}

Content::Content(const WorldDescription& desc) : Content()
{
    fromLua_ = true;
    // Wares
    if(desc.wares.size() > NUM_CUSTOM_GOODS)
        throw GameDataError("At most " + std::to_string(NUM_CUSTOM_GOODS) + " new wares can be defined");
    std::set<std::string> wareNames;
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
    for(const auto& entry : desc.wareTags)
    {
        const auto good = goodFromIdentifier(entry.first);
        if(!good)
        {
            throw GameDataError("Tags for '" + entry.first
                                + "': not a built-in ware (new wares get their tags in rttr:AddWare)");
        }
        SetTags(*good, entry.second.kind, entry.second.properties);
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

    // Own regulators of the distribution window (used by workshops with outputChoice = <name>)
    for(const RegulatorDesc& regulatorDesc : desc.regulators)
    {
        const std::string context = "Regulator '" + regulatorDesc.name + "'";
        Regulator regulator;
        regulator.name = regulatorDesc.name;
        regulator.title = regulatorDesc.title;
        for(const RegulatorDesc::Entry& entryDesc : regulatorDesc.entries)
        {
            if(FindWares(entryDesc.tags).empty())
                throw GameDataError(context + ": entry " + joinTags(entryDesc.tags) + " matches no ware");
            Regulator::Entry entry;
            entry.tags = entryDesc.tags;
            entry.title = entryDesc.title;
            entry.defaultValue = static_cast<uint8_t>(entryDesc.value);
            regulator.entries.push_back(std::move(entry));
        }
        regulators_.push_back(std::move(regulator));
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

        // Worker
        CustomWorker worker;
        worker.name = wsDesc.name;
        worker.title = wsDesc.worker.title;
        if(wsDesc.worker.tool == "none")
            worker.tool = GoodType::Nothing;
        else
        {
            const auto tool = goodFromIdentifier(wsDesc.worker.tool);
            if(!tool || !helpers::contains(TOOLS, *tool))
            {
                throw GameDataError(context
                                    + ": the tool of the worker must be a tool (e.g. \"Hammer\") or \"none\", not '"
                                    + wsDesc.worker.tool + "'");
            }
            worker.tool = *tool;
        }
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

        // Inputs
        std::vector<GoodType> inputWares;
        for(const TagQuery& input : wsDesc.inputs)
        {
            const std::vector<GoodType> matches = FindWares(input);
            if(matches.empty())
                throw GameDataError(context + ": input " + joinTags(input) + " matches no ware");
            if(wsDesc.consumeEach && matches.size() != 1u)
            {
                throw GameDataError(context + ": input " + joinTags(input) + " matches "
                                    + std::to_string(matches.size())
                                    + " wares. With consume = \"each\" every input must match exactly one ware "
                                      "(use consume = \"any\" for alternatives)");
            }
            for(const GoodType good : matches)
            {
                if(!helpers::contains(inputWares, good))
                    inputWares.push_back(good);
            }
        }
        if(inputWares.size() > 3u)
            throw GameDataError(context + ": the inputs match more than 3 different wares");

        // Output: Either all wares matching output or one ware per entry of outputs
        const bool hasOutput = !wsDesc.output.empty() || !wsDesc.outputs.empty();
        std::vector<GoodType> outputMatches;
        std::string outputText;
        if(!wsDesc.outputs.empty())
        {
            outputText = "outputs";
            for(const TagQuery& query : wsDesc.outputs)
            {
                const std::vector<GoodType> matches = FindWares(query);
                if(matches.size() != 1u)
                {
                    throw GameDataError(context + ": entry " + joinTags(query) + " of outputs matches "
                                        + std::to_string(matches.size()) + " wares (expected exactly 1)");
                }
                if(helpers::contains(outputMatches, matches.front()))
                {
                    throw GameDataError(context + ": the ware '" + getIdentifierOf(matches.front())
                                        + "' is in outputs twice");
                }
                outputMatches.push_back(matches.front());
            }
        } else if(!wsDesc.output.empty())
        {
            outputText = "output " + joinTags(wsDesc.output);
            outputMatches = FindWares(wsDesc.output);
        }
        if(!hasOutput)
        {
            // Plantation that only plants (like the forester)
            if(wsDesc.inheritProperties || !wsDesc.outputChoice.empty())
                throw GameDataError(context + ": inheritProperties and outputChoice need an output");
        } else if(outputMatches.empty())
            throw GameDataError(context + ": " + outputText + " matches no ware");
        if(!wsDesc.outputChoice.empty() && (bld.IsCollector() || bld.IsPlanter() || bld.IsMine()))
            throw GameDataError(context + ": collectors, plantations and mines can not choose their product");
        if(wsDesc.inheritProperties)
        {
            if(inputWares.empty())
                throw GameDataError(context + ": inheritProperties requires inputs");
            for(const GoodType input : inputWares)
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
                                        + std::to_string(products.size()) + " wares match the output "
                                        + joinTags(wsDesc.output) + " with the same properties (expected exactly 1)");
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
                    throw GameDataError(context + ": with outputChoice = \"knowledge\" every ware of the " + outputText
                                        + " needs knowledge points of exactly one type (LWP, IWP or MWP), but '"
                                        + getIdentifierOf(candidate) + "' has " + std::to_string(types.size()));
                }
                bld.productChoiceTypes.push_back(types.front());
            }
            bld.productChoice = ProductChoice::Knowledge;
            bld.productChoices = outputMatches;
            bld.product = outputMatches.front();
        } else if(wsDesc.outputChoice == "tools")
        {
            // One product per tool type, chosen like the metalworks does (tool regulator and tool orders)
            for(const GoodType candidate : outputMatches)
            {
                const auto tool = GetToolTypeOf(candidate);
                if(!tool)
                {
                    throw GameDataError(context + ": with outputChoice = \"tools\" every ware of the " + outputText
                                        + " must be a tool type: '" + getIdentifierOf(candidate)
                                        + "' needs the identifier of exactly one built-in tool as tag (e.g. kind = "
                                          "\"Axe\")");
                }
                if(helpers::contains(bld.productChoiceTools, *tool))
                {
                    throw GameDataError(context
                                        + ": with outputChoice = \"tools\" every tool type may only be made "
                                          "once, but there are several of type '"
                                        + getIdentifierOf(TOOL_TO_GOOD[*tool]) + "'");
                }
                bld.productChoiceTools.push_back(*tool);
            }
            bld.productChoice = ProductChoice::Tools;
            bld.productChoices = outputMatches;
            bld.product = outputMatches.front();
        } else if(!wsDesc.outputChoice.empty())
        {
            // Own regulator: every product belongs to exactly one of its entries
            const auto itRegulator = helpers::find_if(
              regulators_, [&](const Regulator& regulator) { return regulator.name == wsDesc.outputChoice; });
            if(itRegulator == regulators_.end())
            {
                throw GameDataError(context + ": outputChoice '" + wsDesc.outputChoice
                                    + "' is neither \"knowledge\" nor \"tools\" nor a regulator (define it with "
                                      "rttr:AddRegulator in distribution.lua)");
            }
            for(const GoodType candidate : outputMatches)
            {
                std::vector<unsigned> entries;
                for(unsigned e = 0; e < itRegulator->entries.size(); ++e)
                {
                    if(Matches(candidate, itRegulator->entries[e].tags))
                        entries.push_back(e);
                }
                if(entries.size() != 1u)
                {
                    throw GameDataError(context + ": the product '" + getIdentifierOf(candidate) + "' matches "
                                        + std::to_string(entries.size()) + " entries of the regulator '"
                                        + itRegulator->name + "' (expected exactly 1)");
                }
                bld.productChoiceEntries.push_back(entries.front());
            }
            bld.productChoice = ProductChoice::Regulator;
            bld.regulator = static_cast<unsigned>(itRegulator - regulators_.begin());
            bld.productChoices = outputMatches;
            bld.product = outputMatches.front();
        } else if(hasOutput)
        {
            if(outputMatches.size() != 1u)
            {
                throw GameDataError(context + ": " + outputText + " matches " + std::to_string(outputMatches.size())
                                    + " wares (expected exactly 1, or use outputChoice)");
            }
            bld.product = outputMatches.front();
        }

        bld.workDesc.job = getCustomJob(i);
        if(!hasOutput)
            bld.workDesc.producedWare = std::nullopt;
        else
            bld.workDesc.producedWare = bld.GetProduct(std::nullopt);
        bld.workDesc.waresNeeded = makeWaresNeeded(inputWares);
        bld.workDesc.numSpacesPerWare = static_cast<uint8_t>(wsDesc.storage);
        bld.workDesc.useOneWareEach = wsDesc.consumeEach;
        outputRatios_[getCustomBuilding(i)] = static_cast<uint16_t>(wsDesc.outputRatio);
        bld.workGFs = static_cast<unsigned>(duration_to_gfs(std::chrono::seconds(wsDesc.workTime)));
        bld.waitGFs = static_cast<unsigned>(duration_to_gfs(std::chrono::seconds(wsDesc.waitTime)));

        buildings_.push_back(std::move(bld));
        workers_.push_back(std::move(worker));
    }

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
    // Default: The tool of the worker and no work without it. Workers without a tool work as before
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
          wsDesc.tools ? *wsDesc.tools : makeDefaultTools(workers_[i].tool), "Workshop '" + wsDesc.name + "'");
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
    return getIdentifier(good);
}

void Content::SetTags(GoodType good, const std::string& kind, const std::vector<std::string>& properties)
{
    std::vector<std::string>& tags = tags_[good];
    if(!kind.empty() && !helpers::contains(tags, kind))
        tags.push_back(kind);
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
        if(helpers::contains(tags_[good], getIdentifier(TOOL_TO_GOOD[tool])))
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
    if(const auto good = goodFromIdentifier(identifier))
        return good;
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
        return bld;
    const CustomBuilding* building = getActiveContent().GetBuilding(bld);
    return building ? building->look : BuildingType::Woodcutter;
}

Job getLook(Job job)
{
    if(!isCustomJob(job))
        return job;
    const CustomWorker* worker = getActiveContent().GetWorker(job);
    return worker ? worker->look : Job::Helper;
}

} // namespace civ
