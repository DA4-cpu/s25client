// Copyright (C) 2005 - 2026 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <s25util/warningSuppression.h>
#include <RTTR_Assert.h>
#include <array>
#include <cstdint>

enum class Job : uint8_t
{
    Helper,            // 0
    Woodcutter,        // 1
    Fisher,            // 2
    Forester,          // 3
    Carpenter,         // 4
    Stonemason,        // 5
    Hunter,            // 6
    Farmer,            // 7
    Miller,            // 8
    Baker,             // 9
    Butcher,           // 10
    Miner,             // 11
    Brewer,            // 12
    PigBreeder,        // 13
    DonkeyBreeder,     // 14
    IronFounder,       // 15
    Minter,            // 16
    Metalworker,       // 17
    Armorer,           // 18
    Builder,           // 19
    Planer,            // 20
    Private,           // 21
    PrivateFirstClass, // 22
    Sergeant,          // 23
    Officer,           // 24
    General,           // 25
    Geologist,         // 26
    Shipwright,        // 27
    Scout,             // 28
    PackDonkey,        // 29
    BoatCarrier,       // 30
    CharBurner,        // 31
    Winegrower,        // 32
    Vintner,           // 33
    TempleServant,     // 34
    Skinner,           // 35
    Tanner,            // 36
    LeatherWorker,     // 37
    // Workers of buildings defined in lua by the civilization mode (see civ/Content.h). Unused unless defined.
    Custom0,                   // 38
    CustomLast = Custom0 + 31, // 69
};

constexpr auto maxEnumValue(Job)
{
    return Job::CustomLast;
}

/// Number of job slots which can be defined in lua (civilization mode), one per custom building
constexpr unsigned NUM_CUSTOM_JOBS = static_cast<unsigned>(Job::CustomLast) - static_cast<unsigned>(Job::Custom0) + 1u;
/// Number of built-in jobs (before the custom slots). Used e.g. for loading older savegames
constexpr unsigned NUM_BUILTIN_JOBS = static_cast<unsigned>(Job::Custom0);
constexpr bool isCustomJob(Job job)
{
    return job >= Job::Custom0 && job <= Job::CustomLast;
}
/// Index of a custom job slot (0..NUM_CUSTOM_JOBS-1)
constexpr unsigned getCustomJobIdx(Job job)
{
    return static_cast<unsigned>(job) - static_cast<unsigned>(Job::Custom0);
}
constexpr Job getCustomJob(unsigned idx)
{
    return Job(static_cast<unsigned>(Job::Custom0) + idx);
}

/// Job types of soldiers, weak ones first
constexpr std::array SUPPRESS_UNUSED SOLDIER_JOBS = {Job::Private, Job::PrivateFirstClass, Job::Sergeant, Job::Officer,
                                                     Job::General};
constexpr bool isSoldierJob(const Job job)
{
    return job >= Job::Private && job <= Job::General;
}

constexpr unsigned NUM_SOLDIER_RANKS = SOLDIER_JOBS.size();
constexpr unsigned getSoldierRank(Job soldierJob)
{
    return static_cast<uint8_t>(soldierJob) - static_cast<uint8_t>(Job::Private);
}

enum class ArmoredSoldier : uint8_t
{
    Private,
    PrivateFirstClass,
    Sergeant,
    Officer,
    General,
};

constexpr auto maxEnumValue(ArmoredSoldier)
{
    return ArmoredSoldier::General;
}

constexpr bool isSoldier(const Job job)
{
    return job >= Job::Private && job <= Job::General;
}

inline ArmoredSoldier jobEnumToAmoredSoldierEnum(const Job job)
{
    RTTR_Assert(isSoldier(job));
    return static_cast<ArmoredSoldier>(getSoldierRank(job));
}

inline Job amoredEnumToSoldierEnum(const ArmoredSoldier armoredSoldier)
{
    return Job(SOLDIER_JOBS[static_cast<uint8_t>(armoredSoldier) - static_cast<uint8_t>(ArmoredSoldier::Private)]);
}
