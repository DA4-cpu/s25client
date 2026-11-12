// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ai/civ/ProductionPlanner.h"
#include "BuildingRegister.h"
#include "GamePlayer.h"
#include "GlobalGameSettings.h"
#include "LeatherLoader.h"
#include "ai/civ/EconomyGraph.h"
#include "ai/civ/MapProfile.h"
#include "buildings/noBuildingSite.h"
#include "buildings/nobMilitary.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/PlayerResearch.h"
#include "civ/Research.h"
#include "civ/TechTree.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "world/GameWorldBase.h"
#include "gameTypes/BuildingCount.h"
#include "gameTypes/Inventory.h"
#include "gameData/BuildingConsts.h"
#include "gameData/BuildingProperties.h"
#include "gameData/GoodConsts.h"
#include "gameData/JobConsts.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>

namespace civai {

namespace {
    constexpr double INF = std::numeric_limits<double>::infinity();
    /// Share of the estimated rate the buildings really reach (transport, waiting for wares, walking)
    constexpr double EFFICIENCY = 0.6;
    /// Construction costs of a building count like the occupancy of one more building after this many wares
    constexpr double BUILD_AMORTIZATION = 10.;
    /// At most this many building sites of one type at the same time
    constexpr unsigned MAX_SITES_PER_TYPE = 2;
    /// A building more only if the work exceeds the existing ones by this share
    constexpr double ROUND_SLACK = 0.1;
    /// Smaller needs (wares per minute, buildings) are ignored
    constexpr double MIN_NEED = 1e-3;
    /// Reserves: construction material per cost entry of the military buildings, tools per kind of tool slot of the
    /// existing buildings, soldiers in the warehouses
    constexpr unsigned CONSTRUCTION_RESERVE = 15;
    constexpr unsigned TOOL_RESERVE = 1;
    /// Soldiers wanted in the warehouses for the next military buildings
    constexpr unsigned SOLDIER_RESERVE = 20;
    /// The AI builds the cheapest military building about every this many minutes (flow of construction material)
    constexpr double EXPANSION_MINUTES = 2.;
    /// New buildings at the same time beyond the helpers in the warehouses (they become the workers)
    constexpr unsigned MIN_FREE_WORKERS = 2;
    /// With more pieces of a source per building using it (e.g. trees per woodcutter), half the renewers suffice
    constexpr double PLENTY_PER_BUILDING = 40.;
    /// Pieces of a source in the territory per building using it (estimate). More buildings take the next way to the
    /// ware. 0: no limit (no source, trees are planted again, water is plenty)
    const helpers::EnumArray<double, SourceKind> SOURCE_PER_BUILDING = {{0., 0., 15., 50., 40., 0., 8., 20., 10., 10.}};

    /// Cost of a ware in stock that cannot be made now (as input of other wares)
    constexpr double STOCK_COST = 1.;

    /// What a soldier needs to be recruited (see nobBaseWarehouse::CanRecruitSoldiers)
    constexpr std::array<GoodType, 3> SOLDIER_EQUIPMENT = {GoodType::Sword, GoodType::ShieldRomans, GoodType::Beer};

    const helpers::EnumArray<double, DemandKind> HORIZON_MINUTES = {{5., 5., 10., 10., 20., EXPANSION_MINUTES, 15.}};
    const helpers::EnumArray<const char*, DemandKind> KIND_NAMES = {
      {"Bau", "Werkzeug", "Forschung", "Soldaten", "Militär", "Expansion", "Vorrat"}};

    unsigned stockOf(const Inventory& inventory, const std::vector<GoodType>& wares)
    {
        unsigned result = 0;
        for(const GoodType good : wares)
            result += inventory.goods[good];
        return result;
    }

    void addItem(std::vector<DemandItem>& items, const DemandKind kind, const std::vector<GoodType>& wares,
                 const double amount)
    {
        if(amount > 0 && !wares.empty())
            items.push_back({kind, wares, amount});
    }

    /// Job of the building's worker
    std::optional<Job> getJob(const BuildingType bld)
    {
        if(isCustomBuilding(bld))
            return getCustomJob(getCustomBuildingIdx(bld));
        if(const auto& job = BLD_WORK_DESC[bld].job)
            return *job;
        return std::nullopt;
    }

    /// Tool needed to recruit the worker of the building (without the tool reform). Nothing: none
    GoodType getRecruitTool(const civ::Content& content, const Job job)
    {
        if(content.IsRecruitedWithoutTool(job))
            return GoodType::Nothing;
        if(isCustomJob(job))
        {
            const civ::CustomWorker* worker = content.GetWorker(job);
            return worker ? worker->tool : GoodType::Nothing;
        }
        return JOB_CONSTS[job].tool.value_or(GoodType::Nothing);
    }

    /// Tools for `count` new buildings: one per tool slot (tool reform) or to recruit the workers that are missing
    void addToolsForNewBuildings(std::vector<DemandItem>& items, const civ::Content& content,
                                 const Inventory& inventory, const BuildingType bld, const unsigned count)
    {
        if(content.HasToolReform() && !content.GetBuildingTools(bld).slots.empty())
        {
            for(const civ::BuildingTools::Slot& slot : content.GetBuildingTools(bld).slots)
                addItem(items, DemandKind::Tools, slot.wares, count);
            return;
        }
        const auto job = getJob(bld);
        if(!job)
            return;
        const GoodType tool = getRecruitTool(content, *job);
        if(tool != GoodType::Nothing && count > inventory.people[*job])
            addItem(items, DemandKind::Tools, {tool}, count - inventory.people[*job]);
    }

    unsigned getTotalCost(const BuildingType bld)
    {
        unsigned result = 0;
        for(const CostEntry& entry : BUILDING_COSTS[bld])
            result += entry.amount;
        return result;
    }

    std::string wareName(const GoodType good) { return WARE_NAMES[good]; }

    std::string waresName(const std::vector<GoodType>& wares)
    {
        std::string result;
        for(const GoodType good : wares)
            result += (result.empty() ? "" : "/") + wareName(good);
        return result;
    }

    std::string buildingName(const BuildingType bld)
    {
        if(isCustomBuilding(bld))
        {
            if(const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(bld))
                return building->name;
        }
        return BUILDING_NAMES[bld];
    }
} // namespace

double getHorizonMinutes(const DemandKind kind)
{
    return HORIZON_MINUTES[kind];
}

Demand collectDemand(const GameWorldBase& world, const unsigned playerId)
{
    Demand demand;
    std::vector<DemandItem>& items = demand.items;
    const GamePlayer& player = world.GetPlayer(playerId);
    const civ::Content& content = world.GetTechTree().GetContent();
    const Inventory& inventory = player.GetInventory();
    const BuildingRegister& buildingRegister = player.GetBuildingRegister();

    // Building sites: material not yet ordered (ordered wares already left the warehouses), tools for the finished
    // building
    helpers::EnumArray<unsigned, BuildingType> numSites{};
    for(const noBuildingSite* site : buildingRegister.GetBuildingSites())
    {
        const BuildingType bld = site->GetBuildingType();
        const BuildingCosts& costs = BUILDING_COSTS[bld];
        const auto& materials = site->GetMaterials();
        for(unsigned i = 0; i < costs.size() && i < materials.size(); ++i)
        {
            const noBuildingSite::Material& material = materials[i];
            const unsigned have = material.used + material.delivered.size() + material.ordered.size();
            if(costs[i].amount > have)
                addItem(items, DemandKind::Construction, costs[i].wares, costs[i].amount - have);
        }
        ++numSites[bld];
    }
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(numSites[bld] > 0 && BuildingProperties::IsUsual(bld))
            addToolsForNewBuildings(items, content, inventory, bld, numSites[bld]);
    }

    // Tool reform: empty tool slots of the buildings, and a reserve per kind of slot for worn out tools
    if(content.HasToolReform())
    {
        std::vector<std::vector<GoodType>> slotWares;
        for(const auto bld : helpers::enumRange<BuildingType>())
        {
            if(!BuildingProperties::IsUsual(bld))
                continue;
            const std::vector<civ::BuildingTools::Slot>& slots = content.GetBuildingTools(bld).slots;
            const std::list<nobUsual*>& buildings = buildingRegister.GetBuildings(bld);
            if(slots.empty() || buildings.empty())
                continue;
            for(const civ::BuildingTools::Slot& slot : slots)
            {
                if(!helpers::contains(slotWares, slot.wares))
                    slotWares.push_back(slot.wares);
            }
            for(const nobUsual* building : buildings)
            {
                const auto& toolSlots = building->GetToolSlots();
                for(unsigned i = 0; i < toolSlots.size() && i < slots.size(); ++i)
                {
                    if(toolSlots[i].tool == GoodType::Nothing && !toolSlots[i].ordered)
                        addItem(items, DemandKind::Tools, slots[i].wares, 1);
                }
            }
        }
        for(const std::vector<GoodType>& wares : slotWares)
            addItem(items, DemandKind::Reserve, wares, TOOL_RESERVE);
    }

    // Soldiers missing in the military buildings, coins and armor they take
    const bool wantsCoins = world.GetGGS().GetMaxMilitaryRank() > 0;
    const bool wantsArmor = leatheraddon::isAddonActive(world);
    unsigned missingTroops = 0, missingCoins = 0, missingArmor = 0;
    for(const nobMilitary* building : buildingRegister.GetMilitaryBuildings())
    {
        const unsigned required = building->CalcRequiredNumTroops();
        if(required > building->GetNumTroops())
            missingTroops += required - building->GetNumTroops();
        if(building->IsNewBuilt())
            continue;
        if(wantsCoins && !building->IsGoldDisabled() && building->GetMaxCoinCt() > building->GetNumCoins())
            missingCoins += building->GetMaxCoinCt() - building->GetNumCoins();
        if(wantsArmor && building->IsArmorAllowed() && building->GetMaxArmorCt() > building->GetNumArmor())
            missingArmor += building->GetMaxArmorCt() - building->GetNumArmor();
    }
    unsigned soldiersInStock = 0;
    for(const Job job : SOLDIER_JOBS)
        soldiersInStock += inventory.people[job];
    const unsigned urgentRecruits = missingTroops > soldiersInStock ? missingTroops - soldiersInStock : 0u;
    const unsigned allRecruits =
      missingTroops + SOLDIER_RESERVE > soldiersInStock ? missingTroops + SOLDIER_RESERVE - soldiersInStock : 0u;
    for(const GoodType good : SOLDIER_EQUIPMENT)
    {
        addItem(items, DemandKind::Soldiers, {good}, urgentRecruits);
        addItem(items, DemandKind::Reserve, {good}, allRecruits - urgentRecruits);
    }
    addItem(items, DemandKind::Military, {GoodType::Coins}, missingCoins);
    addItem(items, DemandKind::Military, {GoodType::Armor}, missingArmor);

    // Current research: wares still to pay and knowledge still missing
    const civ::PlayerResearch& research = player.GetResearch();
    if(const auto current = research.GetCurrent())
    {
        const civ::Technology& tech = world.GetTechTree().get(*current);
        const civ::PlayerResearch::Progress* progress = research.GetProgress(*current);
        for(unsigned i = 0; i < tech.cost.size(); ++i)
        {
            const unsigned paid = progress && i < progress->paid.size() ? progress->paid[i] : 0u;
            if(tech.cost[i].amount > paid)
                addItem(items, DemandKind::Research, {tech.cost[i].good}, tech.cost[i].amount - paid);
        }
        demand.knowledge = civ::getMissingKnowledge(world, playerId, *current);
    }
    for(unsigned i = 0; i < content.GetNumBuildings(); ++i)
    {
        const BuildingType bld = getCustomBuilding(i);
        if(content.GetBuilding(bld)->IsLibrary() && !buildingRegister.GetBuildings(bld).empty())
            demand.tenthsPerPoint = civ::MEDIA_IN_LIBRARY_TENTHS;
    }

    // Construction material for the expansion: the cheapest military building as flow, a reserve of each cost entry
    // of the military buildings
    std::vector<std::vector<GoodType>> costWares;
    const BuildingCosts* cheapestCosts = nullptr;
    const auto totalAmount = [](const BuildingCosts& costs) {
        unsigned result = 0;
        for(const CostEntry& entry : costs)
            result += entry.amount;
        return result;
    };
    for(const BuildingType bld : BuildingProperties::militaryBldTypes)
    {
        if(!player.IsBuildingEnabled(bld))
            continue;
        const BuildingCosts& costs = BUILDING_COSTS[bld];
        if(!cheapestCosts || totalAmount(costs) < totalAmount(*cheapestCosts))
            cheapestCosts = &costs;
        for(const CostEntry& entry : costs)
        {
            if(!helpers::contains(costWares, entry.wares))
                costWares.push_back(entry.wares);
        }
    }
    if(cheapestCosts)
    {
        for(const CostEntry& entry : *cheapestCosts)
            addItem(items, DemandKind::Expansion, entry.wares, entry.amount);
        // Each new military building takes at least one soldier
        for(const GoodType good : SOLDIER_EQUIPMENT)
            addItem(items, DemandKind::Expansion, {good}, 1);
    }
    for(const std::vector<GoodType>& wares : costWares)
        addItem(items, DemandKind::Reserve, wares, CONSTRUCTION_RESERVE);
    return demand;
}

std::vector<DemandItem> applyStock(std::vector<DemandItem> items, const Inventory& inventory)
{
    std::stable_sort(items.begin(), items.end(),
                     [](const DemandItem& lhs, const DemandItem& rhs) { return lhs.kind < rhs.kind; });
    std::vector<std::pair<std::vector<GoodType>, double>> stocks;
    for(DemandItem& item : items)
    {
        if(item.kind == DemandKind::Expansion)
            continue;
        auto it =
          std::find_if(stocks.begin(), stocks.end(), [&](const auto& stock) { return stock.first == item.wares; });
        if(it == stocks.end())
            it = stocks.insert(stocks.end(), {item.wares, static_cast<double>(stockOf(inventory, item.wares))});
        const double used = std::min(it->second, item.amount);
        it->second -= used;
        item.amount -= used;
    }
    helpers::erase_if(items, [](const DemandItem& item) { return item.amount < MIN_NEED; });
    return items;
}

ProductionPlan planProduction(const GameWorldBase& world, const unsigned playerId, const EconomyGraph& graph,
                              const MapProfile& map, const Demand& demand)
{
    ProductionPlan plan;
    std::fill(plan.unitCost.begin(), plan.unitCost.end(), INF);
    std::fill(plan.recipe.begin(), plan.recipe.end(), -1);
    std::fill(plan.mainWare.begin(), plan.mainWare.end(), GoodType::Nothing);

    const GamePlayer& player = world.GetPlayer(playerId);
    const civ::Content& content = world.GetTechTree().GetContent();
    const Inventory& inventory = player.GetInventory();
    const BuildingCount nums = player.GetBuildingRegister().GetBuildingNums();
    const ResourceProfile owned = map.GetTotalOwned();
    const std::vector<Recipe>& recipes = graph.GetRecipes();

    // The shipyard stays with the old AI (sea strategy)
    for(const Recipe& recipe : recipes)
        plan.managed[recipe.building] = recipe.building != BuildingType::Shipyard;
    for(const Renewer& renewer : graph.GetRenewers())
        plan.managed[renewer.building] = true;

    // 1. Cost of each ware with the cheapest recipe usable now (Bellman-Ford: costs only fall)
    std::vector<bool> usable(recipes.size());
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        const Recipe& recipe = recipes[i];
        usable[i] = plan.managed[recipe.building] && recipe.waresPerMinute > 0
                    && player.IsBuildingEnabled(recipe.building)
                    && (recipe.source.kind == SourceKind::None || owned.GetAmount(recipe.source) > 0);
    }
    const auto isAvailable = [&](const std::vector<GoodType>& wares) {
        return helpers::contains_if(
          wares, [&](const GoodType good) { return inventory.goods[good] > 0 || plan.unitCost[good] < INF; });
    };
    const auto cheapest = [&](const std::vector<GoodType>& wares) {
        GoodType result = GoodType::Nothing;
        for(const GoodType good : wares)
        {
            if(plan.unitCost[good] < INF
               && (result == GoodType::Nothing || plan.unitCost[good] < plan.unitCost[result]))
                result = good;
        }
        return result;
    };
    const auto inputCost = [&](const Recipe& recipe) {
        if(recipe.inputs.empty())
            return 0.;
        if(!recipe.consumeEach)
        {
            const GoodType input = cheapest(recipe.inputs);
            return input == GoodType::Nothing ? INF : plan.unitCost[input];
        }
        double sum = 0;
        for(const GoodType input : recipe.inputs)
            sum += plan.unitCost[input];
        return sum;
    };
    // Products of each recipe: its outputs and the byproducts whose technology is researched (one per piece)
    std::vector<std::vector<std::pair<GoodType, unsigned>>> byproductsOf(recipes.size());
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        for(unsigned j = 0; j < recipes[i].byproducts.size(); ++j)
        {
            const int tech = recipes[i].byproductTechs[j];
            if(tech < 0 || player.GetResearch().IsResearched(static_cast<civ::TechIdx>(tech)))
                byproductsOf[i].emplace_back(recipes[i].byproducts[j], recipes[i].byproductAmounts[j]);
        }
    }
    const auto productsOf = [&](const unsigned i) {
        std::vector<GoodType> result = recipes[i].outputs;
        for(const auto& [good, amount] : byproductsOf[i])
        {
            if(!helpers::contains(result, good))
                result.push_back(good);
        }
        return result;
    };

    // Cost of one ware of the recipe with the current costs of its inputs. Infinite: not usable now
    const auto recipeCost = [&](const unsigned i) {
        const Recipe& recipe = recipes[i];
        if(!usable[i])
            return INF;
        const BuildingCosts& costs = BUILDING_COSTS[recipe.building];
        if(!std::all_of(costs.begin(), costs.end(), [&](const CostEntry& entry) { return isAvailable(entry.wares); }))
            return INF;
        if(!std::all_of(recipe.requiredTools.begin(), recipe.requiredTools.end(), isAvailable))
            return INF;
        const double inputs = inputCost(recipe);
        if(inputs == INF)
            return INF;
        const double rate = recipe.waresPerMinute * EFFICIENCY;
        return (1. + getTotalCost(recipe.building) / BUILD_AMORTIZATION) / rate
               + inputs * recipe.stepsPerMinute / recipe.waresPerMinute;
    };
    const auto relaxCosts = [&]() {
        for(unsigned round = 0; round <= recipes.size(); ++round)
        {
            bool changed = false;
            for(unsigned i = 0; i < recipes.size(); ++i)
            {
                const double cost = recipeCost(i);
                for(const GoodType good : productsOf(i))
                {
                    if(cost < plan.unitCost[good] * (1. - 1e-9))
                    {
                        plan.unitCost[good] = cost;
                        plan.recipe[good] = static_cast<int>(i);
                        changed = true;
                    }
                }
            }
            if(!changed)
                break;
        }
    };
    relaxCosts();
    // Wares in stock that cannot be made now are inputs as well (e.g. bones for a carver), until they are used up
    bool fromStock = false;
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(plan.unitCost[good] == INF && inventory.goods[good] > 0)
        {
            plan.unitCost[good] = STOCK_COST;
            fromStock = true;
        }
    }
    if(fromStock)
        relaxCosts();
    // All usable ways to each ware, cheapest first, and how many buildings the source of each allows
    helpers::EnumArray<std::vector<unsigned>, GoodType> candidates;
    std::vector<double> costOfRecipe(recipes.size()), capacity(recipes.size(), INF);
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        costOfRecipe[i] = recipeCost(i);
        if(costOfRecipe[i] == INF)
            continue;
        for(const GoodType good : productsOf(i))
            candidates[good].push_back(i);
        const double perBuilding = SOURCE_PER_BUILDING[recipes[i].source.kind];
        if(perBuilding > 0)
            capacity[i] = std::max(1., owned.GetAmount(recipes[i].source) / perBuilding);
    }
    for(const auto good : helpers::enumRange<GoodType>())
    {
        std::stable_sort(candidates[good].begin(), candidates[good].end(),
                         [&](const unsigned lhs, const unsigned rhs) { return costOfRecipe[lhs] < costOfRecipe[rhs]; });
    }

    // Buildings that can be built and work now (for the knowledge they give): recipe usable with inputs, or renewer
    const auto canBuild = [&](const BuildingType bld) {
        const BuildingCosts& costs = BUILDING_COSTS[bld];
        return player.IsBuildingEnabled(bld) && std::all_of(costs.begin(), costs.end(), [&](const CostEntry& entry) {
                   return isAvailable(entry.wares);
               });
    };
    helpers::EnumArray<bool, BuildingType> ready{};
    for(unsigned i = 0; i < recipes.size(); ++i)
    {
        const Recipe& recipe = recipes[i];
        ready[recipe.building] = usable[i] && canBuild(recipe.building) && inputCost(recipe) < INF
                                 && std::all_of(recipe.requiredTools.begin(), recipe.requiredTools.end(), isAvailable);
    }
    for(const Renewer& renewer : graph.GetRenewers())
        ready[renewer.building] = canBuild(renewer.building);
    const civ::TechTree& techTree = world.GetTechTree();
    // Buildings without inputs work on their own
    helpers::EnumArray<bool, BuildingType> hasInputs{};
    for(const Recipe& recipe : recipes)
        hasInputs[recipe.building] = !recipe.inputs.empty();

    // Order of the wares: products before the inputs of all their ways (depth-first, cycles are cut)
    const auto inputsOf = [&](const unsigned i) {
        const Recipe& recipe = recipes[i];
        std::vector<GoodType> result;
        if(recipe.consumeEach)
            result = recipe.inputs;
        else if(!recipe.inputs.empty())
            result.push_back(cheapest(recipe.inputs));
        return result;
    };
    std::vector<GoodType> order;
    {
        helpers::EnumArray<uint8_t, GoodType> visited{};
        const auto visit = [&](const auto& self, const GoodType good) -> void {
            visited[good] = 1;
            for(const unsigned i : candidates[good])
            {
                for(const GoodType input : inputsOf(i))
                {
                    if(!visited[input])
                        self(self, input);
                }
            }
            order.push_back(good);
        };
        for(const auto good : helpers::enumRange<GoodType>())
        {
            if(!visited[good] && !candidates[good].empty())
                visit(visit, good);
        }
        std::reverse(order.begin(), order.end());
    }

    // 2. Backwards: wares per minute -> buildings -> inputs per minute
    const auto propagate = [&](const std::vector<DemandItem>& items, const civ::KnowledgePoints& mediaKnowledge) {
        plan.demandPerMinute = {};
        plan.perKind = {};
        plan.work = {};
        plan.unmet.clear();
        std::fill(plan.mainWare.begin(), plan.mainWare.end(), GoodType::Nothing);
        for(const DemandItem& item : items)
        {
            const GoodType good = cheapest(item.wares);
            if(good == GoodType::Nothing)
            {
                const std::string text = waresName(item.wares) + " (" + KIND_NAMES[item.kind] + ")";
                if(!helpers::contains(plan.unmet, text))
                    plan.unmet.push_back(text);
                continue;
            }
            const double rate = item.amount / HORIZON_MINUTES[item.kind];
            plan.demandPerMinute[good] += rate;
            plan.perKind[item.kind] += rate;
        }
        // Knowledge the buildings do not give (see below): the medium with the cheapest knowledge point of the kind
        for(const auto type : helpers::enumRange<civ::KnowledgeType>())
        {
            if(mediaKnowledge[type] <= 0)
                continue;
            GoodType best = GoodType::Nothing;
            double bestCost = INF;
            for(const GoodType medium : content.GetKnowledgeMedia())
            {
                const int points = content.GetKnowledgeOf(medium)[type];
                if(points > 0 && plan.unitCost[medium] / points < bestCost)
                {
                    best = medium;
                    bestCost = plan.unitCost[medium] / points;
                }
            }
            if(best == GoodType::Nothing)
            {
                plan.unmet.push_back(std::string("Wissen ") + civ::getShortName(type) + " (Forschung)");
                continue;
            }
            const double numMedia =
              static_cast<double>(mediaKnowledge[type]) / (content.GetKnowledgeOf(best)[type] * demand.tenthsPerPoint);
            const double rate = numMedia / HORIZON_MINUTES[DemandKind::Research];
            plan.demandPerMinute[best] += rate;
            plan.perKind[DemandKind::Research] += rate;
        }
        plan.needPerMinute = plan.demandPerMinute;
        helpers::EnumArray<double, BuildingType> mainShare{};
        std::vector<double> workOfRecipe(recipes.size(), 0.);
        // Byproducts of the buildings planned so far (wares per minute)
        helpers::EnumArray<double, GoodType> byproductSupply{};
        const auto assign = [&](const GoodType good, const unsigned i, const double buildings) {
            const Recipe& recipe = recipes[i];
            workOfRecipe[i] += buildings;
            for(const auto& [byproduct, amount] : byproductsOf[i])
            {
                if(byproduct != good)
                    byproductSupply[byproduct] += buildings * recipe.waresPerMinute * EFFICIENCY * amount;
            }
            plan.work[recipe.building] += buildings;
            if(buildings > mainShare[recipe.building])
            {
                mainShare[recipe.building] = buildings;
                plan.mainWare[recipe.building] = good;
            }
            const double steps = buildings * recipe.stepsPerMinute * EFFICIENCY;
            for(const GoodType input : inputsOf(i))
                plan.needPerMinute[input] += steps;
        };
        for(const GoodType good : order)
        {
            double need = plan.needPerMinute[good] - byproductSupply[good];
            if(need < MIN_NEED || candidates[good].empty())
                continue; // Nothing needed or only from the stock
            // The cheapest way as far as its source allows, then the next. Too little everywhere: the cheapest
            for(const unsigned i : candidates[good])
            {
                const double rate = recipes[i].waresPerMinute * EFFICIENCY;
                const double buildings = std::min(need / rate, capacity[i] - workOfRecipe[i]);
                if(buildings < MIN_NEED)
                    continue;
                assign(good, i, buildings);
                need -= buildings * rate;
                if(need < MIN_NEED)
                    break;
            }
            if(need >= MIN_NEED)
                assign(good, candidates[good].front(),
                       need / (recipes[candidates[good].front()].waresPerMinute * EFFICIENCY));
        }
        // Renewers (forester): as many as the source is used up, half of them if there is plenty of it
        for(const Renewer& renewer : graph.GetRenewers())
        {
            double usedPerMinute = 0, numUsers = 0;
            GoodType ware = GoodType::Nothing;
            for(const Recipe& recipe : recipes)
            {
                if(recipe.source.kind != renewer.source.kind || recipe.source.index != renewer.source.index
                   || plan.work[recipe.building] < MIN_NEED)
                    continue;
                usedPerMinute += plan.work[recipe.building] * recipe.stepsPerMinute * EFFICIENCY;
                numUsers += plan.work[recipe.building];
                ware = plan.mainWare[recipe.building];
            }
            if(usedPerMinute < MIN_NEED || renewer.perMinute <= 0)
                continue;
            const double share = owned.GetAmount(renewer.source) > PLENTY_PER_BUILDING * numUsers ? 0.5 : 1.;
            plan.work[renewer.building] += usedPerMinute / (renewer.perMinute * EFFICIENCY) * share;
            plan.mainWare[renewer.building] = ware;
        }
    };

    const auto numExisting = [&](const BuildingType bld) { return nums.buildings[bld] + nums.buildingSites[bld]; };
    const auto calcWanted = [&]() {
        for(const auto bld : helpers::enumRange<BuildingType>())
        {
            if(!plan.managed[bld])
                continue;
            const double work = plan.work[bld];
            const unsigned needed =
              work < MIN_NEED ? 0u : std::max(1u, static_cast<unsigned>(std::ceil(work - ROUND_SLACK)));
            plan.wanted[bld] = std::min(needed, nums.buildings[bld] + MAX_SITES_PER_TYPE);
        }
    };

    const civ::KnowledgePoints noKnowledge{};
    propagate(applyStock(demand.items, inventory), noKnowledge);
    calcWanted();

    // 3. Knowledge for the current research: finished buildings give knowledge points (knowledge.lua) as long as they
    // stand. Building sites and the buildings the plan adds count already. For the rest more buildings with the
    // cheapest knowledge point (construction wares per point, half for buildings which work on their own or are needed
    // anyway), what they cannot give at once comes from knowledge media
    civ::KnowledgePoints missing = demand.knowledge;
    const auto addKnowledgeOf = [&](const BuildingType bld, const unsigned count) {
        for(const auto type : helpers::enumRange<civ::KnowledgeType>())
            missing[type] -= static_cast<int>(count) * techTree.GetBuildingKnowledge(bld)[type] * civ::KNOWLEDGE_SCALE;
    };
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(plan.managed[bld])
            addKnowledgeOf(bld, nums.buildingSites[bld]
                                  + (plan.wanted[bld] > numExisting(bld) ? plan.wanted[bld] - numExisting(bld) : 0u));
    }
    // Number of buildings wanted including those for knowledge
    helpers::EnumArray<unsigned, BuildingType> knowledgeTarget{};
    for(const auto bld : helpers::enumRange<BuildingType>())
        knowledgeTarget[bld] = std::max(plan.wanted[bld], numExisting(bld));
    for(const auto type : helpers::enumRange<civ::KnowledgeType>())
    {
        while(missing[type] > 0)
        {
            BuildingType best = BuildingType::Headquarters;
            double bestCost = INF;
            for(const auto bld : helpers::enumRange<BuildingType>())
            {
                const int points = techTree.GetBuildingKnowledge(bld)[type];
                if(!ready[bld] || !plan.managed[bld] || points <= 0
                   || knowledgeTarget[bld] >= nums.buildings[bld] + MAX_SITES_PER_TYPE)
                    continue;
                const bool useful = !hasInputs[bld] || plan.work[bld] >= MIN_NEED;
                const double cost = static_cast<double>(getTotalCost(bld)) / points * (useful ? 0.5 : 1.);
                if(cost < bestCost)
                {
                    best = bld;
                    bestCost = cost;
                }
            }
            if(bestCost == INF)
                break; // All candidates have their building sites
            const int tenthsPerBuilding = techTree.GetBuildingKnowledge(best)[type] * civ::KNOWLEDGE_SCALE;
            const unsigned room = nums.buildings[best] + MAX_SITES_PER_TYPE - knowledgeTarget[best];
            const unsigned count =
              std::min(room, static_cast<unsigned>((missing[type] + tenthsPerBuilding - 1) / tenthsPerBuilding));
            knowledgeTarget[best] += count;
            plan.forKnowledge[best] += count;
            addKnowledgeOf(best, count);
        }
    }
    civ::KnowledgePoints mediaKnowledge{};
    for(const auto type : helpers::enumRange<civ::KnowledgeType>())
        mediaKnowledge[type] = std::max(0, missing[type]);

    // 4. The new buildings need construction material and tools as well, the media their production
    std::vector<DemandItem> items = demand.items;
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!plan.managed[bld] || knowledgeTarget[bld] <= numExisting(bld))
            continue;
        const unsigned count = knowledgeTarget[bld] - numExisting(bld);
        for(const CostEntry& entry : BUILDING_COSTS[bld])
            addItem(items, DemandKind::Construction, entry.wares, entry.amount * count);
        addToolsForNewBuildings(items, content, inventory, bld, count);
    }
    propagate(applyStock(std::move(items), inventory), mediaKnowledge);
    calcWanted();
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(plan.forKnowledge[bld] > 0)
        {
            plan.wanted[bld] = std::max(plan.wanted[bld], knowledgeTarget[bld]);
            plan.work[bld] = std::max(plan.work[bld], static_cast<double>(plan.wanted[bld]));
        }
    }

    // Not more new buildings than workers: the most needed first
    std::vector<std::pair<double, BuildingType>> additions;
    unsigned numAdditions = 0;
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(plan.managed[bld] && plan.wanted[bld] > numExisting(bld))
        {
            additions.emplace_back(plan.work[bld] - numExisting(bld), bld);
            numAdditions += plan.wanted[bld] - numExisting(bld);
        }
    }
    const unsigned maxAdditions = inventory.people[Job::Helper] + MIN_FREE_WORKERS;
    if(numAdditions > maxAdditions)
    {
        std::stable_sort(additions.begin(), additions.end(),
                         [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });
        // One building per type in each round, so every step of a chain gets started
        helpers::EnumArray<unsigned, BuildingType> granted{};
        unsigned budget = maxAdditions;
        bool grantedAny = true;
        while(budget > 0 && grantedAny)
        {
            grantedAny = false;
            for(const auto& addition : additions)
            {
                const BuildingType bld = addition.second;
                if(budget > 0 && numExisting(bld) + granted[bld] < plan.wanted[bld])
                {
                    ++granted[bld];
                    --budget;
                    grantedAny = true;
                }
            }
        }
        for(const auto& addition : additions)
            plan.wanted[addition.second] = numExisting(addition.second) + granted[addition.second];
    }
    return plan;
}

std::string ProductionPlan::GetReport() const
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    out << "  Bedarf je Minute:";
    for(const auto kind : helpers::enumRange<DemandKind>())
        out << " " << KIND_NAMES[kind] << " " << perKind[kind] << ";";
    out << "\n  Waren (direkt/gesamt je Minute):";
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(needPerMinute[good] >= MIN_NEED)
            out << " " << wareName(good) << " " << demandPerMinute[good] << "/" << needPerMinute[good] << ";";
    }
    out << "\n  Gebäude (gewollt, Bedarf):";
    for(const auto bld : helpers::enumRange<BuildingType>())
    {
        if(!managed[bld] || (wanted[bld] == 0 && work[bld] < MIN_NEED))
            continue;
        out << " " << buildingName(bld) << " " << wanted[bld] << " (" << work[bld];
        if(mainWare[bld] != GoodType::Nothing)
            out << " für " << wareName(mainWare[bld]);
        if(forKnowledge[bld] > 0)
            out << ", " << forKnowledge[bld] << " für Wissen";
        out << ");";
    }
    if(!unmet.empty())
    {
        out << "\n  Nicht machbar:";
        for(const std::string& text : unmet)
            out << " " << text << ";";
    }
    out << "\n";
    return out.str();
}

} // namespace civai
