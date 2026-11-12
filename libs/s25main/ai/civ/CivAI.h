// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "ai/civ/EconomyGraph.h"
#include "ai/civ/MapProfile.h"
#include "ai/civ/ProductionPlanner.h"
#include "helpers/EnumArray.h"
#include "gameTypes/GoodTypes.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GameWorldBase;

namespace civai {

/// Whether the player can make a ware with what there is
enum class WareState : uint8_t
{
    /// With the resources in its territory and the buildings it may build now
    Producible,
    /// The resources are on the map, but not in its territory
    NeedsExpansion,
    /// Only after research
    NeedsResearch,
    /// Not at all on this map (a source is missing, or nothing makes it)
    Unreachable
};
constexpr auto maxEnumValue(WareState)
{
    return WareState::Unreachable;
}

struct WareAnalysis
{
    WareState state = WareState::Unreachable;
    /// Unreachable: what is missing, e.g. "fehlt: Steinhaufen"
    std::string reason;
};

/// Which wares the player can make (from the production graph and the resources)
helpers::EnumArray<WareAnalysis, GoodType> analyzeEconomy(const GameWorldBase& world, unsigned playerId,
                                                          const EconomyGraph& graph, const MapProfile& map);

/// Duration of the parts of the AI (microseconds) to keep it cheap
struct AITimes
{
    double graphUs = 0, initialScanUs = 0, analysisUs = 0, planUs = 0, planMaxUs = 0;
    double updateTotalUs = 0, updateMaxUs = 0;
    unsigned numUpdates = 0;
};

/// New AI of the civilization mode (see DESIGN.md, "Neue KI"): perception, economic model (step 1) and the plan of the
/// production buildings (step 2). It gives no commands itself, the old AI (AIJH) builds what the plan wants
class CivAI
{
public:
    CivAI(const GameWorldBase& world, unsigned playerId);

    /// Called every GF by the AI player
    void RunGF(unsigned gf, bool isNWF);

    const EconomyGraph& GetGraph() const { return graph_; }
    const MapProfile& GetMap() const { return map_; }
    const helpers::EnumArray<WareAnalysis, GoodType>& GetAnalysis() const { return analysis_; }
    const AITimes& GetTimes() const { return times_; }
    /// Latest plan of the production buildings, nullptr before the first one
    const ProductionPlan* GetPlan() const { return hasPlan_ ? &plan_ : nullptr; }

    /// Text of the current state: resources, wares per state, times
    std::string GetReport() const;
    /// Scan the whole map again, analyze and plan (otherwise it is done bit by bit)
    void Refresh();
    /// Plan the production buildings for the current demand
    void Plan();

    /// Regions scanned again per NWF (the whole map of 512x512 nodes takes 4096 regions)
    static constexpr unsigned REGIONS_PER_NWF = 16;
    /// GFs between two analyses
    static constexpr unsigned ANALYSIS_INTERVAL = 3000;
    /// GFs between two log lines with the times
    static constexpr unsigned TIMES_INTERVAL = 36000;
    /// GFs between two plans (the AIs plan in different GFs) and between two plans in the log
    static constexpr unsigned PLAN_INTERVAL = 200;
    static constexpr unsigned PLAN_LOG_INTERVAL = 6000;

private:
    void Analyze();

    const GameWorldBase& world_;
    const unsigned playerId_;
    AITimes times_;
    EconomyGraph graph_;
    MapProfile map_;
    helpers::EnumArray<WareAnalysis, GoodType> analysis_;
    unsigned lastAnalysisGF_ = 0;
    ProductionPlan plan_;
    bool hasPlan_ = false;
};

} // namespace civai
