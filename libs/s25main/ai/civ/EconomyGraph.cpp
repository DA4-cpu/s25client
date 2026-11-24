// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ai/civ/EconomyGraph.h"
#include "AddonHelperFunctions.h"
#include "civ/Content.h"
#include "civ/Identifiers.h"
#include "civ/TechTree.h"
#include "figures/nofFarmhand.h"
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
    /// The inputs of a building for the AI: one per input slot. Built-in mines (any of their inputs): one with all
    std::vector<RecipeInput> makeRecipeInputs(const BldWorkDescription& workDesc)
    {
        std::vector<RecipeInput> result;
        if(!workDesc.useOneWareEach)
        {
            RecipeInput anyOf;
            for(const InputSlot& slot : workDesc.inputs)
                anyOf.wares.insert(anyOf.wares.end(), slot.wares.begin(), slot.wares.end());
            if(!anyOf.wares.empty())
                result.push_back(std::move(anyOf));
            return result;
        }
        for(const InputSlot& slot : workDesc.inputs)
            result.push_back(RecipeInput{slot.wares, slot.amount});
        return result;
    }

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

    /// Radius in which the worker takes the source, if it can be counted on the map around the building
    unsigned getReach(const Source& source, const std::optional<Job> job)
    {
        switch(source.kind)
        {
            case SourceKind::Ore: return MINER_RADIUS;
            case SourceKind::Trees:
            case SourceKind::StonePiles:
            case SourceKind::Fish:
            case SourceKind::MapObject: return job ? nofFarmhand::GetWorkRadius(*job) : 0u;
            // Animals move, land and plants are counted by the work of the building itself, water is under the well
            case SourceKind::None:
            case SourceKind::Water:
            case SourceKind::Animals:
            case SourceKind::Land:
            case SourceKind::Plantable: return 0;
        }
        return 0;
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
    AddRenewers(world);
}

void EconomyGraph::AddBuiltinRecipes(const GameWorldBase& world)
{
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        // The shipyard builds ships (no ware): it stays with the old AI
        if(isCustomBuilding(bld) || !BuildingProperties::IsUsual(bld) || BuildingProperties::IsMilitary(bld)
           || bld == BuildingType::Shipyard)
            continue;
        const BldWorkDescription& workDesc = BLD_WORK_DESC[bld];
        if(!workDesc.job)
            continue;
        Recipe recipe;
        recipe.building = bld;
        recipe.inputs = makeRecipeInputs(workDesc);
        recipe.outputs = getBuiltinOutputs(bld);
        recipe.source = getBuiltinSource(bld);
        const JobConst jobConst = JOB_CONSTS[*workDesc.job];
        AddRecipe(world, std::move(recipe), jobConst.wait1_length + jobConst.work_length + jobConst.wait2_length,
                  *workDesc.job);
    }
}

void EconomyGraph::AddCustomRecipes(const GameWorldBase& world)
{
    const civ::Content& content = world.GetTechTree().GetContent();
    for(unsigned i = 0; i < content.GetNumBuildings(); ++i)
    {
        const BuildingType bld = getCustomBuilding(i);
        const civ::CustomBuilding& building = *content.GetBuilding(bld);
        if(building.IsLibrary() || building.IsWarehouse())
            continue;
        Recipe recipe;
        recipe.building = bld;
        recipe.inputs = makeRecipeInputs(building.workDesc);
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
        if(building.IsCollector())
            recipe.source = {SourceKind::MapObject, static_cast<unsigned>(building.collectObject)};
        else if(building.IsPlanter())
            recipe.source = {SourceKind::Plantable, static_cast<unsigned>(building.plantObject)};
        else if(building.IsMine())
            recipe.source = {SourceKind::Ore, static_cast<unsigned>(building.mineResource)};
        const std::optional<Job> job =
          building.workDesc.job ? std::optional<Job>(*building.workDesc.job) : std::optional<Job>();
        AddRecipe(world, std::move(recipe), building.workGFs + building.waitGFs, job);
    }
}

void EconomyGraph::AddRecipe(const GameWorldBase& world, Recipe recipe, const unsigned cycleGFs,
                             const std::optional<Job> job)
{
    // Buildings of disabled addons (wine, leather ...) do not exist in this game
    if(makeIsUnusedBuilding(world.GetGGS())(recipe.building))
        return;
    for(const civ::Byproduct& byproduct : world.GetTechTree().GetByproducts(recipe.building))
    {
        if(byproduct.ware == GoodType::Nothing)
            continue;
        recipe.byproducts.push_back(byproduct.ware);
        recipe.byproductAmounts.push_back(byproduct.amount);
        recipe.byproductTechs.push_back(byproduct.technology ? static_cast<int>(*byproduct.technology) : -1);
    }
    // GoodType::Nothing is no ware (some buildings use it for "no product")
    const auto isNothing = [](const GoodType good) { return good == GoodType::Nothing; };
    helpers::erase_if(recipe.outputs, isNothing);
    for(RecipeInput& input : recipe.inputs)
        helpers::erase_if(input.wares, isNothing);
    helpers::erase_if(recipe.inputs, [](const RecipeInput& input) { return input.wares.empty(); });
    for(const RecipeInput& input : recipe.inputs)
    {
        for(const GoodType good : input.wares)
        {
            if(!helpers::contains(recipe.inputWares, good))
                recipe.inputWares.push_back(good);
        }
    }
    if(recipe.outputs.empty() && recipe.byproducts.empty())
        return; // Makes no wares (forester, donkey breeder ...)
    recipe.requiredTools = GetWorkTools(world.GetTechTree().GetContent(), recipe.building);
    recipe.lockedAtStart = world.GetTechTree().IsLockedAtStart(recipe.building);
    recipe.reach = getReach(recipe.source, job);
    const double gfsPerMinute = static_cast<double>(duration_to_gfs(std::chrono::minutes(1)));
    recipe.nominalStepsPerMinute = gfsPerMinute / std::max(1u, cycleGFs);

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

void EconomyGraph::AddRenewers(const GameWorldBase& world)
{
    // Buildings which only make a source grow: the forester, and the plantations of lua without a product (they plant
    // the map object which collectors take). Plantations with a product harvest themselves and are recipes
    const auto isUnused = makeIsUnusedBuilding(world.GetGGS());
    if(!isUnused(BuildingType::Forester))
    {
        const unsigned reach = nofFarmhand::GetWorkRadius(*BLD_WORK_DESC[BuildingType::Forester].job);
        renewers_.push_back({BuildingType::Forester, {SourceKind::Trees, 0}, reach});
    }
    const civ::Content& content = world.GetTechTree().GetContent();
    for(unsigned i = 0; i < content.GetNumBuildings(); ++i)
    {
        const BuildingType bld = getCustomBuilding(i);
        const civ::CustomBuilding& building = *content.GetBuilding(bld);
        if(!building.IsPlanter() || building.product != GoodType::Nothing || !building.productChoices.empty()
           || !building.productByInput.empty() || isUnused(bld))
            continue;
        renewers_.push_back(
          {bld, {SourceKind::MapObject, static_cast<unsigned>(building.plantObject)}, building.plantRadius});
    }
}

std::vector<std::vector<GoodType>> EconomyGraph::GetWorkTools(const civ::Content& content, const BuildingType bld)
{
    std::vector<std::vector<GoodType>> result;
    if(!content.HasToolReform())
        return result;
    const civ::BuildingTools& tools = content.GetBuildingTools(bld);
    if(tools.baseSpeed > 0)
        return result;
    for(const civ::BuildingTools::Slot& slot : tools.slots)
    {
        std::vector<GoodType> wares;
        for(const GoodType good : slot.wares)
        {
            if(const civ::ToolInfo* info = content.GetTool(good); info && info->speed > 0)
                wares.push_back(good);
        }
        if(!wares.empty())
            result.push_back(std::move(wares));
    }
    return result;
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
