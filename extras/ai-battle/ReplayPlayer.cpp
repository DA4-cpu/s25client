// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ReplayPlayer.h"
#include "EventManager.h"
#include "Game.h"
#include "GamePlayer.h"
#include "ILocalGameState.h"
#include "PlayerInfo.h"
#include "Replay.h"
#include "ai/AIPlayer.h"
#include "factories/AIFactory.h"
#include "network/PlayerGameCommands.h"
#include "random/Random.h"
#include "variant.h"
#include "world/GameWorld.h"
#include "world/MapLoader.h"
#include "gameTypes/MapInfo.h"
#include "s25util/tmpFile.h"
#include <boost/nowide/iostream.hpp>
#include <chrono>
#include <memory>
#include <vector>

namespace bfs = boost::filesystem;
namespace bnw = boost::nowide;

namespace {
struct ReplayLocalState : ILocalGameState
{
    unsigned GetPlayerId() const override { return 0; }
    bool IsHost() const override { return true; }
    std::string FormatGFTime(unsigned) const override { return ""; }
    void SystemChat(const std::string&) override {}
};
} // namespace

int playReplay(const bfs::path& path, const unsigned maxGF, const bool runAIs)
{
    Replay replay;
    if(!replay.LoadHeader(path))
    {
        bnw::cerr << "Could not read the replay " << path << ": " << replay.GetLastErrorMsg() << std::endl;
        return 1;
    }
    MapInfo mapInfo;
    if(!replay.LoadGameData(mapInfo))
    {
        bnw::cerr << "Could not read the game data of the replay: " << replay.GetLastErrorMsg() << std::endl;
        return 1;
    }
    if(mapInfo.savegame)
    {
        bnw::cerr << "The replay starts from a savegame (not supported)" << std::endl;
        return 1;
    }
    bnw::cout << "Replay " << path << ": map " << replay.GetMapName() << ", version " << replay.GetMajorVersion() << "."
              << replay.GetMinorVersion() << ", last GF " << replay.GetLastGF() << ", seed " << replay.getSeed()
              << std::endl;
    std::vector<PlayerInfo> players;
    for(unsigned i = 0; i < replay.GetNumPlayers(); i++)
    {
        players.emplace_back(replay.GetPlayer(i));
        bnw::cout << "  player " << i << ": " << players.back().name
                  << (players.back().ps == PlayerState::AI ? " (AI)" : "") << std::endl;
    }
    Game game(replay.ggs, std::make_unique<EventManager>(0), players);
    RANDOM.Init(replay.getSeed());
    GameWorld& world = game.world_;
    for(unsigned i = 0; i < world.GetNumPlayers(); ++i)
        world.GetPlayer(i).MakeStartPacts();
    TmpFile mapFile(".swd");
    mapFile.close();
    if(!mapInfo.mapData.DecompressToFile(mapFile.filePath))
    {
        bnw::cerr << "Could not write the map of the replay" << std::endl;
        return 1;
    }
    MapLoader loader(world);
    ReplayLocalState localState;
    TmpFile luaFile(".lua");
    luaFile.close();
    if(!loader.Load(mapFile.filePath))
    {
        bnw::cerr << "Could not load the map of the replay" << std::endl;
        return 1;
    }
    if(mapInfo.luaData.uncompressedLength)
    {
        if(!mapInfo.luaData.DecompressToFile(luaFile.filePath) || !loader.LoadLuaScript(game, localState, luaFile.filePath))
        {
            bnw::cerr << "Could not load the lua script of the replay" << std::endl;
            return 1;
        }
    }
    MapLoader::SetupResources(world, replay.GetMinorVersion() >= 3);
    world.InitAfterLoad();

    std::vector<std::unique_ptr<AIPlayer>> ais;
    if(runAIs)
    {
        for(unsigned i = 0; i < world.GetNumPlayers(); ++i)
        {
            if(world.GetPlayer(i).ps == PlayerState::AI)
                ais.push_back(AIFactory::Create(world.GetPlayer(i).aiInfo, i, world));
        }
    }

    game.Start(false);
    auto nextGF = replay.ReadGF();
    const auto startTime = std::chrono::steady_clock::now();
    auto nextReport = startTime + std::chrono::seconds(5);
    EventManager& em = *static_cast<EventManager*>(game.em_.get());
    unsigned numCommands = 0;
    while(em.GetCurrentGF() < maxGF && !game.IsGameFinished())
    {
        const unsigned curGF = em.GetCurrentGF();
        if(nextGF && *nextGF < curGF)
        {
            bnw::cerr << "Replay out of order: command for GF " << *nextGF << " at GF " << curGF << std::endl;
            return 1;
        }
        const AsyncChecksum checksum = (nextGF && *nextGF == curGF) ? AsyncChecksum::create(game) : AsyncChecksum();
        while(nextGF && *nextGF == curGF)
        {
            const auto cmd = replay.ReadCommand();
            if(const auto* gameCmd = boost::variant2::get_if<Replay::GameCommand>(&cmd))
            {
                if(gameCmd->cmds.checksum.randChecksum != 0 && !(checksum == gameCmd->cmds.checksum))
                {
                    bnw::cerr << "Asynchronous at GF " << curGF << " (player " << unsigned(gameCmd->player)
                              << "): the game data differs from the recorded game" << std::endl
                              << "  played:   " << checksum << std::endl
                              << "  recorded: " << gameCmd->cmds.checksum << std::endl;
                    return 2;
                }
                for(const gc::GameCommandPtr& gc : gameCmd->cmds.gcs)
                    gc->Execute(world, gameCmd->player);
                ++numCommands;
            }
            nextGF = replay.ReadGF();
        }
        for(auto& ai : ais)
        {
            ai->RunGF(curGF, curGF % 20 == 0);
            ai->FetchGameCommands();
        }
        game.RunGF();
        if(std::chrono::steady_clock::now() > nextReport)
        {
            nextReport += std::chrono::seconds(5);
            bnw::cout << "GF " << em.GetCurrentGF() << " of " << replay.GetLastGF() << std::endl;
        }
        if(!nextGF && em.GetCurrentGF() > replay.GetLastGF())
            break;
    }
    bnw::cout << "Replay played to GF " << em.GetCurrentGF() << " (" << numCommands << " command packets)" << std::endl;
    return 0;
}
