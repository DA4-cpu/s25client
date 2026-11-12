// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ai/civ/CivAI.h"
#include "GamePlayer.h"
#include "civ/Content.h"
#include "civ/TechTree.h"
#include "helpers/EnumRange.h"
#include "helpers/containerUtils.h"
#include "world/GameWorldBase.h"
#include "gameData/BuildingConsts.h"
#include "gameData/GoodConsts.h"
#include "s25util/Log.h"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace civai {

namespace {
    using Clock = std::chrono::steady_clock;

    double elapsedUs(const Clock::time_point start)
    {
        return std::chrono::duration<double, std::micro>(Clock::now() - start).count();
    }

    template<class T_Func>
    auto timed(double& us, T_Func&& func)
    {
        const auto start = Clock::now();
        auto result = func();
        us = elapsedUs(start);
        return result;
    }

    /// Wares which can be made with the given sources (fixpoint over the production graph).
    /// One-time needs (construction material, tools) may also come from the stock
    helpers::EnumArray<bool, GoodType> findReachable(const EconomyGraph& graph, const ResourceProfile& sources,
                                                     const GamePlayer& player, const bool allTechnologies)
    {
        helpers::EnumArray<bool, GoodType> reachable{};
        const auto inStockOrReachable = [&](const std::vector<GoodType>& wares) {
            return helpers::contains_if(
              wares, [&](const GoodType good) { return reachable[good] || player.GetInventory().goods[good] > 0u; });
        };
        bool changed = true;
        while(changed)
        {
            changed = false;
            for(const Recipe& recipe : graph.GetRecipes())
            {
                if(!allTechnologies && !player.IsBuildingEnabled(recipe.building))
                    continue;
                if(sources.GetAmount(recipe.source) == 0)
                    continue;
                if(recipe.consumeEach)
                {
                    if(!std::all_of(recipe.inputs.begin(), recipe.inputs.end(),
                                    [&](const GoodType good) { return reachable[good]; }))
                        continue;
                } else if(!recipe.inputs.empty()
                          && !helpers::contains_if(recipe.inputs, [&](const GoodType good) { return reachable[good]; }))
                    continue;
                if(!std::all_of(BUILDING_COSTS[recipe.building].begin(), BUILDING_COSTS[recipe.building].end(),
                                [&](const CostEntry& entry) { return inStockOrReachable(entry.wares); }))
                    continue;
                if(!std::all_of(recipe.requiredTools.begin(), recipe.requiredTools.end(), inStockOrReachable))
                    continue;
                const auto markReachable = [&](const std::vector<GoodType>& wares) {
                    for(const GoodType good : wares)
                    {
                        if(!reachable[good])
                        {
                            reachable[good] = true;
                            changed = true;
                        }
                    }
                };
                markReachable(recipe.outputs);
                for(unsigned i = 0; i < recipe.byproducts.size(); ++i)
                {
                    const int tech = recipe.byproductTechs[i];
                    if(allTechnologies || tech < 0
                       || player.GetResearch().IsResearched(static_cast<civ::TechIdx>(tech)))
                        markReachable({recipe.byproducts[i]});
                }
            }
        }
        return reachable;
    }

    std::string wareName(const GoodType good) { return WARE_NAMES[good]; }
} // namespace

helpers::EnumArray<WareAnalysis, GoodType> analyzeEconomy(const GameWorldBase& world, const unsigned playerId,
                                                          const EconomyGraph& graph, const MapProfile& map)
{
    const GamePlayer& player = world.GetPlayer(playerId);
    const ResourceProfile owned = map.GetTotalOwned();
    const ResourceProfile all = map.GetTotal();
    const auto now = findReachable(graph, owned, player, false);
    const auto onMap = findReachable(graph, all, player, false);
    const auto afterResearch = findReachable(graph, all, player, true);

    helpers::EnumArray<WareAnalysis, GoodType> result;
    const civ::Content& content = world.GetTechTree().GetContent();
    for(const auto good : helpers::enumRange<GoodType>())
    {
        if(content.GetTags(good).empty())
            continue; // No usable ware (unused slot, other nations' shields ...)
        WareAnalysis& analysis = result[good];
        if(now[good])
            analysis.state = WareState::Producible;
        else if(onMap[good])
            analysis.state = WareState::NeedsExpansion;
        else if(afterResearch[good])
            analysis.state = WareState::NeedsResearch;
        else
        {
            analysis.state = WareState::Unreachable;
            // Why: the first missing thing of each building making it
            std::vector<std::string> reasons;
            for(const unsigned idx : graph.GetProducers(good))
            {
                const Recipe& recipe = graph.GetRecipes()[idx];
                std::string reason;
                if(all.GetAmount(recipe.source) == 0)
                    reason = "fehlt " + graph.GetSourceName(recipe.source);
                else
                {
                    for(const GoodType input : recipe.inputs)
                    {
                        if(!afterResearch[input])
                        {
                            reason = "braucht " + wareName(input);
                            break;
                        }
                    }
                    if(reason.empty())
                        reason = "Baumaterial oder Werkzeug fehlt";
                }
                if(!helpers::contains(reasons, reason))
                    reasons.push_back(reason);
            }
            if(reasons.empty())
                analysis.reason = "kein Gebäude stellt sie her";
            else
            {
                for(unsigned i = 0; i < reasons.size(); ++i)
                    analysis.reason += (i ? ", " : "") + reasons[i];
            }
        }
    }
    return result;
}

CivAI::CivAI(const GameWorldBase& world, const unsigned playerId)
    : world_(world), playerId_(playerId), graph_(timed(times_.graphUs, [&world]() { return EconomyGraph(world); })),
      map_(timed(times_.initialScanUs, [&world, playerId]() { return MapProfile(world, playerId); }))
{
    Analyze();
    LOG.write("CIVAI player %u: %s") % playerId_ % GetReport();
}

void CivAI::RunGF(const unsigned gf, const bool isNWF)
{
    if(isNWF)
    {
        const auto start = Clock::now();
        map_.Update(REGIONS_PER_NWF);
        const double us = elapsedUs(start);
        times_.updateTotalUs += us;
        times_.updateMaxUs = std::max(times_.updateMaxUs, us);
        ++times_.numUpdates;
    }
    if(gf >= lastAnalysisGF_ + ANALYSIS_INTERVAL)
    {
        lastAnalysisGF_ = gf;
        const auto before = analysis_;
        Analyze();
        bool changed = false;
        for(const auto good : helpers::enumRange<GoodType>())
            changed |= before[good].state != analysis_[good].state;
        if(changed)
            LOG.write("CIVAI player %u, GF %u: %s") % playerId_ % gf % GetReport();
    }
    if((gf + playerId_ * 37) % PLAN_INTERVAL == 0)
    {
        Plan();
        if((gf + playerId_ * 37) % PLAN_LOG_INTERVAL == 0)
            LOG.write("CIVAI player %u, GF %u, Plan:\n%s") % playerId_ % gf % plan_.GetReport();
    }
    if(gf > 0 && gf % TIMES_INTERVAL == 0)
    {
        LOG.write("CIVAI player %u, GF %u: Analyse %u us, Plan %u us (max %u), Aktualisierung je NWF %u us (max %u)\n")
          % playerId_ % gf % static_cast<unsigned>(times_.analysisUs) % static_cast<unsigned>(times_.planUs)
          % static_cast<unsigned>(times_.planMaxUs)
          % static_cast<unsigned>(times_.numUpdates ? times_.updateTotalUs / times_.numUpdates : 0.)
          % static_cast<unsigned>(times_.updateMaxUs);
    }
}

void CivAI::Refresh()
{
    map_.Update(map_.GetNumRegions());
    Analyze();
    Plan();
}

void CivAI::Plan()
{
    const auto start = Clock::now();
    plan_ = planProduction(world_, playerId_, graph_, map_, collectDemand(world_, playerId_));
    hasPlan_ = true;
    times_.planUs = elapsedUs(start);
    times_.planMaxUs = std::max(times_.planMaxUs, times_.planUs);
}

void CivAI::Analyze()
{
    const auto start = Clock::now();
    analysis_ = analyzeEconomy(world_, playerId_, graph_, map_);
    times_.analysisUs = elapsedUs(start);
}

std::string CivAI::GetReport() const
{
    std::ostringstream out;
    out << graph_.GetRecipes().size() << " Rezepte, " << map_.GetNumRegions() << " Regionen\n";

    // Resources: in the territory / on the map, only those any building needs
    const ResourceProfile owned = map_.GetTotalOwned();
    const ResourceProfile all = map_.GetTotal();
    std::vector<std::string> sourceNames;
    out << "  Rohstoffe (Gebiet/Karte):";
    for(const Recipe& recipe : graph_.GetRecipes())
    {
        if(recipe.source.kind == SourceKind::None)
            continue;
        const std::string name = graph_.GetSourceName(recipe.source);
        if(helpers::contains(sourceNames, name))
            continue;
        sourceNames.push_back(name);
        out << " " << name << " " << owned.GetAmount(recipe.source) << "/" << all.GetAmount(recipe.source) << ";";
    }
    out << "\n";

    const helpers::EnumArray<const char*, WareState> stateNames = {
      {"herstellbar", "nach Expansion", "nach Forschung", "nicht erreichbar"}};
    const civ::Content& content = world_.GetTechTree().GetContent();
    for(const auto state : helpers::enumRange<WareState>())
    {
        out << "  " << stateNames[state] << ":";
        for(const auto good : helpers::enumRange<GoodType>())
        {
            if(content.GetTags(good).empty() || analysis_[good].state != state)
                continue;
            out << " " << wareName(good);
            if(state == WareState::Unreachable)
                out << " (" << analysis_[good].reason << ")";
            out << ";";
        }
        out << "\n";
    }
    out << "  Zeiten: Graph " << static_cast<unsigned>(times_.graphUs) << " us, Karte "
        << static_cast<unsigned>(times_.initialScanUs) << " us, Analyse " << static_cast<unsigned>(times_.analysisUs)
        << " us, Plan " << static_cast<unsigned>(times_.planUs) << " us, Aktualisierung je NWF "
        << static_cast<unsigned>(times_.numUpdates ? times_.updateTotalUs / times_.numUpdates : 0.) << " us (max "
        << static_cast<unsigned>(times_.updateMaxUs) << ")\n";
    return out.str();
}

} // namespace civai
