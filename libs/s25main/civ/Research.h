// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "civ/Knowledge.h"
#include "civ/TechTree.h"
#include <optional>
#include <vector>

class GamePlayer;
class GameWorld;
class GameWorldBase;

/// Game logic of the civilization mode (tech tree & research)
namespace civ {

/// Interval in game frames in which research costs are paid (2s at normal speed)
constexpr unsigned RESEARCH_UPDATE_INTERVAL = 40;

/// True if the civilization mode (tech tree) is active in this game
bool isEnabled(const GameWorldBase& world);

enum class TechState
{
    /// Not part of this game (required addon disabled)
    Unavailable,
    Researched,
    InProgress,
    Available,
    MissingRequirements,
    /// Exclusive technology already researched by another player
    TakenByOther
};

/// Player who researched the exclusive technology or nullopt if none (or it is not exclusive)
std::optional<unsigned> getExclusiveOwner(const GameWorldBase& world, TechIdx tech);
TechState getTechState(const GameWorldBase& world, unsigned playerId, TechIdx tech);
/// True if the player can start researching this technology
bool canResearch(const GameWorldBase& world, unsigned playerId, TechIdx tech);

/// Enable/disable buildings of the player according to its research. Call at game start and after loading
void applyBuildingAvailability(GamePlayer& player);
/// Initialize the civilization mode for all players at game start (new game or loaded). Noop if not enabled
void startGame(GameWorld& world);

/// Set the current research of the player (nullopt to stop researching). Returns false if not possible
bool setResearch(GameWorld& world, unsigned playerId, std::optional<TechIdx> tech);
/// Pay research costs from the warehouses and complete technologies. Called every RESEARCH_UPDATE_INTERVAL GFs
void update(GameWorld& world, unsigned numGFs = RESEARCH_UPDATE_INTERVAL);
/// Complete the technology for the player (unlocks buildings, handles exclusivity)
void completeResearch(GameWorld& world, unsigned playerId, TechIdx tech);

/// True if the remaining costs of the technology can be paid from the current stock of the warehouses
/// Knowledge (tenths) still missing to complete the technology: cost minus available, at least 0 per kind
KnowledgePoints getMissingKnowledge(const GameWorldBase& world, unsigned playerId, TechIdx tech);
/// True if the player has enough available knowledge points to complete the technology
bool hasEnoughKnowledge(const GameWorldBase& world, unsigned playerId, TechIdx tech);

/// True if all remaining ware costs are in stock and enough knowledge points are available
bool isAffordable(const GameWorldBase& world, unsigned playerId, TechIdx tech);
/// Sum of all wares already paid for the technology
unsigned getAmountPaid(const GameWorldBase& world, unsigned playerId, TechIdx tech);

/// Byproducts which the worker of a building of type `bld` brings to the flag together with `product`
/// (rttr:AddByproduct), each byproduct once per piece. Empty if the civilization mode is off
std::vector<GoodType> getByproducts(const GameWorldBase& world, unsigned playerId, BuildingType bld, GoodType product);

/// Choose a technology for the AI to research (ignoring `exclude`):
/// Affordable ones first, then highest priority, then cheapest, then lowest era
std::optional<TechIdx> chooseResearchForAI(const GameWorldBase& world, unsigned playerId,
                                           std::optional<TechIdx> exclude = std::nullopt);

} // namespace civ
