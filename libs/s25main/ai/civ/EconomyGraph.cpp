// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ai/civ/EconomyGraph.h"
#include "AddonHelperFunctions.h"
#include "civ/Content.h"
#include "civ/Identifiers.h"
#include "civ/TechTree.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "world/GameWorldBase.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GameConsts.h"
#include "gameData/JobConsts.h"
#include "gameData/ToolConsts.h"
#include <chrono>

namespace civai {

namespace {
    /// Estimated walking time of a worker who works outside (to the work point and back) in GFs
    constexpr unsigned OUTSIDE_WALK_GFS = 160;

    /// What the built-in buildings take from the map. These buildings have their own figures in the code, so this is
    /// the only place where the AI knows them by name
    Source getBuiltinSource(const BuildingType bld)
    {
        switch(bld)
        {
            case BuildingType::Woodcutter: return {SourceKind::Trees, 0};
            case BuildingType::Quarry: return {SourceKind::StonePiles, 0};
            case BuildingType::Fishery: return {SourceKind::Fish, 0};
            case BuildingType::Hunter: return {SourceKind::Animals, 0};
            case BuildingType::Well: return {SourceKind::Water, 0};
            case BuildingType::Farm:
            case BuildingType::Vineyard:
            case BuildingType::Charburner: return {SourceKind::Land, 0};
            case BuildingType::GraniteMine: return {SourceKind::Ore, static_cast<unsigned>(ResourceType::Granite)};
            case BuildingType::CoalMine: return {SourceKind::Ore, static_cast<unsigned>(ResourceType::Coal)};
            case BuildingType::IronMine: return {SourceKind::Ore, static_cast<unsigned>(ResourceType::Iron)};
            case BuildingType::GoldMine: return {SourceKind::Ore, static_cast<unsigned>(ResourceType::Gold)};
            default: return {};
        }
    }

    /// Products of the built-in buildings which choose between several wares
    std::vector<GoodType> getBuiltinOutputs(const BuildingType bld)
    {
        switch(bld)
        {
            case BuildingType::Metalworks: return std::vector<GoodType>(TOOL_TO_GOOD.begin(), TOOL_TO_GOOD.end());
            case BuildingType::Armory: return {GoodType::Sword, GoodType::ShieldRomans};
            case BuildingType::Temple: return {GoodType::Gold, GoodType::IronOre, GoodType::Coal, GoodType::Stones};
            default: break;
        }
        std::vector<GoodType> result;
        if(const auto* good = boost::variant2::get_if<GoodType>(&BLD_WORK_DESC[bld].producedWare))
            result.push_back(*good);
        return result;
    }
} // namespace

EconomyGraph::EconomyGraph(const GameWorldBase& world)
{
    const civ::Content& content = world.GetTechTree().GetContent();
    for(const civ::CustomMapObject& obj : content.GetMapObjects())
        mapObjectNames_.push_back(obj.title);
    for(const civ::CustomOre& ore : content.GetOres())
        oreNames_.push_back(ore.title);
    AddBuiltinRecipes(world);
    AddCustomRecipes(world);
}

void EconomyGraph::AddBuiltinRecipes(const GameWorldBase& world)
{
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(isCustomBuilding(bld) || !BuildingProperties::IsUsual(bld) || BuildingProperties::IsMilitary(bld))
            continue;
        const BldWorkDescription& workDesc = BLD_WORK_DESC[bld];
        if(!workDesc.job)
            continue;
        Recipe recipe;
        recipe.building = bld;
        recipe.inputs.assign(workDesc.waresNeeded.begin(), workDesc.waresNeeded.end());
        recipe.consumeEach = workDesc.useOneWareEach;
        recipe.outputs = getBuiltinOutputs(bld);
        recipe.source = getBuiltinSource(bld);
        const JobConst jobConst = JOB_CONSTS[*workDesc.job];
        unsigned cycle = jobConst.wait1_length + jobConst.work_length + jobConst.wait2_length;
        if(recipe.source.kind != SourceKind::None && recipe.source.kind != SourceKind::Ore
           && recipe.source.kind != SourceKind::Water)
            cycle += OUTSIDE_WALK_GFS;
        AddRecipe(world, std::move(recipe), cycle);
    }
}

void EconomyGraph::AddCustomRecipes(const GameWorldBase& world)
{
    const civ::Content& content = world.GetTechTree().GetContent();
    for(unsigned i = 0; i < content.GetNumBuildings(); ++i)
    {
        const BuildingType bld = getCustomBuilding(i);
        const civ::CustomBuilding& building = *content.GetBuilding(bld);
        if(building.IsLibrary())
            continue;
        Recipe recipe;
        recipe.building = bld;
        recipe.inputs.assign(building.workDesc.waresNeeded.begin(), building.workDesc.waresNeeded.end());
        recipe.consumeEach = building.workDesc.useOneWareEach;
        if(!building.productChoices.empty())
            recipe.outputs = building.productChoices;
        else if(!building.productByInput.empty())
        {
            for(const auto& entry : building.productByInput)
            {
                if(!helpers::contains(recipe.outputs, entry.second))
                    recipe.outputs.push_back(entry.second);
            }
        } else if(building.product != GoodType::Nothing)
            recipe.outputs.push_back(building.product);
        unsigned cycle = building.workGFs + building.waitGFs;
        if(building.IsCollector())
        {
            recipe.source = {SourceKind::MapObject, static_cast<unsigned>(building.collectObject)};
            cycle += OUTSIDE_WALK_GFS;
        } else if(building.IsPlanter())
        {
            // Planting and harvesting: about every second trip yields a ware
            recipe.source = {SourceKind::Plantable, static_cast<unsigned>(building.plantObject)};
            cycle = 2 * (cycle + OUTSIDE_WALK_GFS);
        } else if(building.IsMine())
            recipe.source = {SourceKind::Ore, static_cast<unsigned>(building.mineResource)};
        AddRecipe(world, std::move(recipe), cycle);
    }
}

void EconomyGraph::AddRecipe(const GameWorldBase& world, Recipe recipe, const unsigned cycleGFs)
{
    // Buildings of disabled addons (wine, leather ...) do not exist in this game
    if(makeIsUnusedBuilding(world.GetGGS())(recipe.building))
        return;
    for(const civ::Byproduct& byproduct : world.GetTechTree().GetByproducts(recipe.building))
    {
        if(byproduct.ware == GoodType::Nothing)
            continue;
        recipe.byproducts.push_back(byproduct.ware);
        recipe.byproductTechs.push_back(byproduct.technology ? static_cast<int>(*byproduct.technology) : -1);
    }
    // GoodType::Nothing is no ware (some buildings use it for "no product")
    const auto isNothing = [](const GoodType good) { return good == GoodType::Nothing; };
    helpers::erase_if(recipe.outputs, isNothing);
    helpers::erase_if(recipe.inputs, isNothing);
    if(recipe.outputs.empty() && recipe.byproducts.empty())
        return; // Makes no wares (forester, donkey breeder ...)
    const civ::Content& content = world.GetTechTree().GetContent();
    const civ::BuildingTools& tools = content.GetBuildingTools(recipe.building);
    if(tools.baseSpeed == 0)
    {
        for(const civ::BuildingTools::Slot& slot : tools.slots)
            recipe.requiredTools.push_back(slot.wares);
    }
    recipe.lockedAtStart = world.GetTechTree().IsLockedAtStart(recipe.building);
    const double gfsPerMinute = static_cast<double>(duration_to_gfs(std::chrono::minutes(1)));
    recipe.waresPerMinute = gfsPerMinute / std::max(1u, cycleGFs) * content.GetOutputRatio(recipe.building) / 100.;

    const auto idx = static_cast<unsigned>(recipes_.size());
    for(const GoodType good : recipe.outputs)
        producers_[good].push_back(idx);
    for(const GoodType good : recipe.byproducts)
    {
        if(!helpers::contains(producers_[good], idx))
            producers_[good].push_back(idx);
    }
    recipes_.push_back(std::move(recipe));
}

std::string EconomyGraph::GetSourceName(const Source& source) const
{
    switch(source.kind)
    {
        case SourceKind::None: return "-";
        case SourceKind::Trees: return "Bäume";
        case SourceKind::StonePiles: return "Steinhaufen";
        case SourceKind::Fish: return "Fisch";
        case SourceKind::Water: return "Wasser";
        case SourceKind::Animals: return "Tiere";
        case SourceKind::Land: return "Ackerland";
        case SourceKind::Ore:
        {
            const auto type = static_cast<ResourceType>(source.index);
            if(isCustomOre(type))
            {
                const unsigned idx = static_cast<unsigned>(type) - static_cast<unsigned>(ResourceType::Custom0);
                return "Erz " + (idx < oreNames_.size() ? oreNames_[idx] : std::to_string(idx));
            }
            switch(type)
            {
                case ResourceType::Iron: return "Eisenerz";
                case ResourceType::Gold: return "Gold";
                case ResourceType::Coal: return "Kohle";
                case ResourceType::Granite: return "Granit";
                default: return "Erz";
            }
        }
        case SourceKind::MapObject:
            return source.index < mapObjectNames_.size() ? mapObjectNames_[source.index] : "Objekt";
        case SourceKind::Plantable:
            return "Anbaufläche " + (source.index < mapObjectNames_.size() ? mapObjectNames_[source.index] : "");
    }
    return "-";
}

} // namespace civai
