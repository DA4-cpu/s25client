// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "helpers/EnumArray.h"
#include "gameTypes/GoodTypes.h"
#include <cstdint>
#include <vector>

class SerializedGameData;

/// Why a ware left the stock of a player. Every DecreaseInventoryWare names one
enum class WareUse : uint8_t
{
    /// Input of a building, taken for a work step
    Production,
    /// Delivered to a building site (also the material of an expedition founding a harbor)
    Construction,
    /// Tool given to a worker when he is recruited, or put into a tool slot of a building (tool reform)
    Tool,
    /// Sword, shield and beer of a recruited soldier
    Soldiers,
    /// Coin used to promote a soldier, armor put on a soldier
    Military,
    /// Paid for research
    Research,
    /// Sent to another player with a trade caravan
    Trade,
    /// Destroyed: lost on the way, burned with a building, captured by an enemy
    Lost
};
constexpr auto maxEnumValue(WareUse)
{
    return WareUse::Lost;
}

/// Where a ware came from. Every IncreaseInventoryWare names one
enum class WareSource : uint8_t
{
    /// Made by a worker of a building (also byproducts)
    Production,
    /// Back from a demolished building site or building, or paid research back after aborting it
    Refund,
    /// Tool of a worker coming back to a warehouse (tool reform)
    Returned,
    /// Taken over with a captured building
    Captured,
    /// Received from another player with a trade caravan
    Trade,
    /// Start wares, lua scripts, cheats
    Added
};
constexpr auto maxEnumValue(WareSource)
{
    return WareSource::Added;
}

/// Counts of one game minute
struct EconomyMinute
{
    helpers::EnumArray<helpers::EnumArray<uint16_t, WareSource>, GoodType> gained{};
    helpers::EnumArray<helpers::EnumArray<uint16_t, WareUse>, GoodType> used{};
    /// Helpers created by the warehouses
    uint16_t helpersCreated = 0;
    /// Helpers who became a worker or a soldier
    uint16_t helpersRecruited = 0;
};

/// Exact economy statistic of a player: every ware that enters or leaves his stock, counted at the moment it happens,
/// per game minute for the last HISTORY_MINUTES minutes and in total since the start.
/// "The last n minutes" are the current (not yet finished) minute and the n-1 minutes before
class EconomyStatistics
{
public:
    /// Length of the history. Older minutes are dropped
    static constexpr unsigned HISTORY_MINUTES = 240;

    EconomyStatistics();

    void AddGained(GoodType good, unsigned count, WareSource source, unsigned gf);
    void AddUsed(GoodType good, unsigned count, WareUse use, unsigned gf);
    void AddHelpersCreated(unsigned count, unsigned gf);
    void AddHelpersRecruited(unsigned count, unsigned gf);

    /// Made in the last n minutes (n = 1..HISTORY_MINUTES)
    unsigned GetProduced(GoodType good, unsigned minutes, unsigned gf) const;
    /// Gained from the source in the last n minutes
    unsigned GetGained(GoodType good, WareSource source, unsigned minutes, unsigned gf) const;
    /// Used for the purpose in the last n minutes
    unsigned GetUsed(GoodType good, WareUse use, unsigned minutes, unsigned gf) const;
    /// Used for any purpose in the last n minutes
    unsigned GetConsumed(GoodType good, unsigned minutes, unsigned gf) const;
    /// Balance of the last n minutes: produced minus consumed (gains other than production do not count)
    int GetBalance(GoodType good, unsigned minutes, unsigned gf) const;
    /// Consumed per minute in the last n minutes (divided by the time actually covered, shorter at the start)
    double GetConsumptionPerMinute(GoodType good, unsigned minutes, unsigned gf) const;
    unsigned GetHelpersCreated(unsigned minutes, unsigned gf) const;
    unsigned GetHelpersRecruited(unsigned minutes, unsigned gf) const;

    /// Game minutes covered by the last n minutes at the given GF (the current minute counts as the part passed)
    double GetCoveredMinutes(unsigned minutes, unsigned gf) const;
    /// Counts of a minute of the history: 0 = current minute, 1 = the one before ... Nullptr if it is not stored
    const EconomyMinute* GetMinute(unsigned minutesAgo, unsigned gf) const;

    /// Totals since the start of the game
    uint32_t GetTotalGained(GoodType good, WareSource source) const { return totalGained_[good][source]; }
    uint32_t GetTotalUsed(GoodType good, WareUse use) const { return totalUsed_[good][use]; }

    void Serialize(SerializedGameData& sgd) const;
    void Deserialize(SerializedGameData& sgd);

    /// Game frames per game minute
    static unsigned GetGFsPerMinute();

private:
    /// Minute of the history at the GF to count into (starts the new minutes on the way)
    EconomyMinute& Current(unsigned gf);
    /// Sum over the minutes of the window
    template<class T_Func>
    unsigned Sum(unsigned minutes, unsigned gf, const T_Func& get) const;

    /// Ring buffer, entry i holds the minute m with m % HISTORY_MINUTES == i
    std::vector<EconomyMinute> history_;
    /// Newest minute (game minute since the start) in the history
    unsigned newestMinute_ = 0;
    helpers::EnumArray<helpers::EnumArray<uint32_t, WareSource>, GoodType> totalGained_{};
    helpers::EnumArray<helpers::EnumArray<uint32_t, WareUse>, GoodType> totalUsed_{};
};
