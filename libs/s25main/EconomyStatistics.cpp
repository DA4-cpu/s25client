// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "EconomyStatistics.h"
#include "SerializedGameData.h"
#include "helpers/EnumRange.h"
#include "gameData/GameConsts.h"
#include <algorithm>
#include <chrono>
#include <limits>

namespace {
void addSaturated(uint16_t& value, const unsigned count)
{
    value = static_cast<uint16_t>(std::min<unsigned>(value + count, std::numeric_limits<uint16_t>::max()));
}

/// Entry types in the savegame
enum class EntryKind : uint8_t
{
    Gained,
    Used
};
constexpr auto maxEnumValue(EntryKind)
{
    return EntryKind::Used;
}
} // namespace

EconomyStatistics::EconomyStatistics() : history_(HISTORY_MINUTES) {}

unsigned EconomyStatistics::GetGFsPerMinute()
{
    return static_cast<unsigned>(duration_to_gfs(std::chrono::minutes(1)));
}

EconomyMinute& EconomyStatistics::Current(const unsigned gf)
{
    const unsigned minute = gf / GetGFsPerMinute();
    if(minute > newestMinute_)
    {
        // Start the minutes passed since the last count (at most the whole history)
        const unsigned numNew = std::min(minute - newestMinute_, HISTORY_MINUTES);
        for(unsigned m = minute - numNew + 1; m <= minute; ++m)
            history_[m % HISTORY_MINUTES] = EconomyMinute();
        newestMinute_ = minute;
    }
    return history_[newestMinute_ % HISTORY_MINUTES];
}

void EconomyStatistics::AddGained(const GoodType good, const unsigned count, const WareSource source, const unsigned gf)
{
    if(count == 0)
        return;
    addSaturated(Current(gf).gained[good][source], count);
    totalGained_[good][source] += count;
}

void EconomyStatistics::AddUsed(const GoodType good, const unsigned count, const WareUse use, const unsigned gf)
{
    if(count == 0)
        return;
    addSaturated(Current(gf).used[good][use], count);
    totalUsed_[good][use] += count;
}

void EconomyStatistics::AddHelpersCreated(const unsigned count, const unsigned gf)
{
    addSaturated(Current(gf).helpersCreated, count);
}

void EconomyStatistics::AddHelpersUsed(const unsigned count, const unsigned gf)
{
    addSaturated(Current(gf).helpersUsed, count);
}

const EconomyMinute* EconomyStatistics::GetMinute(const unsigned minutesAgo, const unsigned gf) const
{
    const unsigned queryMinute = gf / GetGFsPerMinute();
    if(minutesAgo >= HISTORY_MINUTES || minutesAgo > queryMinute)
        return nullptr;
    const unsigned minute = queryMinute - minutesAgo;
    // Not counted yet (nothing happened since then) or already dropped from the history
    if(minute > newestMinute_ || minute + HISTORY_MINUTES <= newestMinute_)
        return nullptr;
    return &history_[minute % HISTORY_MINUTES];
}

template<class T_Func>
unsigned EconomyStatistics::Sum(const unsigned minutes, const unsigned gf, const T_Func& get) const
{
    unsigned result = 0;
    for(unsigned i = 0; i < std::min(minutes, HISTORY_MINUTES); ++i)
    {
        if(const EconomyMinute* minute = GetMinute(i, gf))
            result += get(*minute);
    }
    return result;
}

unsigned EconomyStatistics::GetProduced(const GoodType good, const unsigned minutes, const unsigned gf) const
{
    return GetGained(good, WareSource::Production, minutes, gf);
}

unsigned EconomyStatistics::GetGained(const GoodType good, const WareSource source, const unsigned minutes,
                                      const unsigned gf) const
{
    return Sum(minutes, gf, [&](const EconomyMinute& minute) { return minute.gained[good][source]; });
}

unsigned EconomyStatistics::GetUsed(const GoodType good, const WareUse use, const unsigned minutes,
                                    const unsigned gf) const
{
    return Sum(minutes, gf, [&](const EconomyMinute& minute) { return minute.used[good][use]; });
}

unsigned EconomyStatistics::GetConsumed(const GoodType good, const unsigned minutes, const unsigned gf) const
{
    return Sum(minutes, gf, [&](const EconomyMinute& minute) {
        unsigned result = 0;
        for(const auto use : helpers::enumRange<WareUse>())
            result += minute.used[good][use];
        return result;
    });
}

int EconomyStatistics::GetBalance(const GoodType good, const unsigned minutes, const unsigned gf) const
{
    return static_cast<int>(GetProduced(good, minutes, gf)) - static_cast<int>(GetConsumed(good, minutes, gf));
}

double EconomyStatistics::GetCoveredMinutes(const unsigned minutes, const unsigned gf) const
{
    const unsigned gfsPerMinute = GetGFsPerMinute();
    const unsigned queryMinute = gf / gfsPerMinute;
    const unsigned numMinutes = std::max(1u, std::min(minutes, HISTORY_MINUTES));
    const unsigned firstMinute = queryMinute + 1 > numMinutes ? queryMinute + 1 - numMinutes : 0u;
    return static_cast<double>(gf + 1 - firstMinute * gfsPerMinute) / gfsPerMinute;
}

double EconomyStatistics::GetConsumptionPerMinute(const GoodType good, const unsigned minutes, const unsigned gf) const
{
    return GetConsumed(good, minutes, gf) / GetCoveredMinutes(minutes, gf);
}

unsigned EconomyStatistics::GetHelpersCreated(const unsigned minutes, const unsigned gf) const
{
    return Sum(minutes, gf, [](const EconomyMinute& minute) { return minute.helpersCreated; });
}

unsigned EconomyStatistics::GetHelpersUsed(const unsigned minutes, const unsigned gf) const
{
    return Sum(minutes, gf, [](const EconomyMinute& minute) { return minute.helpersUsed; });
}

void EconomyStatistics::Serialize(SerializedGameData& sgd) const
{
    // Only the counts that are not 0: most wares are not used in a minute
    const auto pushEntries = [&sgd](const auto& gained, const auto& used, auto pushValue) {
        unsigned num = 0;
        for(const auto good : helpers::enumRange<GoodType>())
        {
            for(const auto source : helpers::enumRange<WareSource>())
                num += gained[good][source] != 0;
            for(const auto use : helpers::enumRange<WareUse>())
                num += used[good][use] != 0;
        }
        sgd.PushVarSize(num);
        for(const auto good : helpers::enumRange<GoodType>())
        {
            for(const auto source : helpers::enumRange<WareSource>())
            {
                if(!gained[good][source])
                    continue;
                sgd.PushEnum<uint8_t>(EntryKind::Gained);
                sgd.PushEnum<uint8_t>(good);
                sgd.PushEnum<uint8_t>(source);
                pushValue(gained[good][source]);
            }
            for(const auto use : helpers::enumRange<WareUse>())
            {
                if(!used[good][use])
                    continue;
                sgd.PushEnum<uint8_t>(EntryKind::Used);
                sgd.PushEnum<uint8_t>(good);
                sgd.PushEnum<uint8_t>(use);
                pushValue(used[good][use]);
            }
        }
    };

    sgd.PushUnsignedInt(newestMinute_);
    const unsigned numStored = std::min(newestMinute_ + 1, HISTORY_MINUTES);
    for(unsigned i = numStored; i > 0; --i)
    {
        const EconomyMinute& minute = history_[(newestMinute_ + 1 - i) % HISTORY_MINUTES];
        pushEntries(minute.gained, minute.used, [&sgd](const uint16_t value) { sgd.PushUnsignedShort(value); });
        sgd.PushUnsignedShort(minute.helpersCreated);
        sgd.PushUnsignedShort(minute.helpersUsed);
    }
    pushEntries(totalGained_, totalUsed_, [&sgd](const uint32_t value) { sgd.PushUnsignedInt(value); });
}

void EconomyStatistics::Deserialize(SerializedGameData& sgd)
{
    const auto popEntries = [&sgd](auto& gained, auto& used, auto popValue) {
        const unsigned num = sgd.PopVarSize();
        for(unsigned i = 0; i < num; ++i)
        {
            const auto kind = sgd.Pop<EntryKind>();
            const auto good = sgd.Pop<GoodType>();
            if(kind == EntryKind::Gained)
            {
                const auto source = sgd.Pop<WareSource>();
                gained[good][source] = popValue();
            } else
            {
                const auto use = sgd.Pop<WareUse>();
                used[good][use] = popValue();
            }
        }
    };

    history_.assign(HISTORY_MINUTES, EconomyMinute());
    totalGained_ = {};
    totalUsed_ = {};
    newestMinute_ = sgd.PopUnsignedInt();
    const unsigned numStored = std::min(newestMinute_ + 1, HISTORY_MINUTES);
    for(unsigned i = numStored; i > 0; --i)
    {
        EconomyMinute& minute = history_[(newestMinute_ + 1 - i) % HISTORY_MINUTES];
        popEntries(minute.gained, minute.used, [&sgd]() { return sgd.PopUnsignedShort(); });
        minute.helpersCreated = sgd.PopUnsignedShort();
        minute.helpersUsed = sgd.PopUnsignedShort();
    }
    popEntries(totalGained_, totalUsed_, [&sgd]() { return sgd.PopUnsignedInt(); });
}
