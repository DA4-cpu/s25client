// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nofCustomPlanter.h"
#include "GameInterface.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "SerializedGameData.h"
#include "SoundManager.h"
#include "buildings/nobUsual.h"
#include "civ/Climate.h"
#include "civ/Content.h"
#include "civ/CustomGraphics.h"
#include "civ/TechTree.h"
#include "network/GameClient.h"
#include "ogl/glArchivItem_Bitmap_Player.h"
#include "world/GameWorld.h"
#include "nodeObjs/noDeposit.h"
#include "gameData/JobConsts.h"
#include "gameData/MapGenDesc.h"
#include <algorithm>
#include <vector>

nofCustomPlanter::nofCustomPlanter(const Job job, const MapPoint pos, const unsigned char player, nobUsual* workplace)
    : nofFarmhand(job, pos, player, workplace), harvest_(false)
{
    RTTR_Assert(isCustomJob(job));
}

nofCustomPlanter::nofCustomPlanter(SerializedGameData& sgd, const unsigned obj_id)
    : nofFarmhand(sgd, obj_id), harvest_(sgd.PopBool())
{}

void nofCustomPlanter::Serialize(SerializedGameData& sgd) const
{
    nofFarmhand::Serialize(sgd);
    sgd.PushBool(harvest_);
}

const civ::CustomBuilding& nofCustomPlanter::GetBuildingDesc() const
{
    const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(getCustomBuilding(getCustomJobIdx(job_)));
    RTTR_Assert(building && building->IsPlanter());
    return *building;
}

unsigned nofCustomPlanter::GetPlantRadius(const Job job)
{
    if(!isCustomJob(job))
        return 0;
    const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(getCustomBuilding(getCustomJobIdx(job)));
    return (building && building->IsPlanter()) ? building->plantRadius : 0u;
}

bool nofCustomPlanter::GrowsNaturallyAt(const GameWorldBase& world, const MapPoint pt, const unsigned objIdx)
{
    const civ::CustomMapObject* obj = world.GetTechTree().GetContent().GetMapObject(objIdx);
    if(!obj)
        return false;
    const civ::ClimateMap& climate = world.GetClimate();
    // The rules of the map generator are part of the description of the world (world/mapgen.lua), the content of the
    // civilization mode is loaded without them
    bool hasRule = false;
    for(const MapResourceDesc& rule : world.GetDescription().mapGen.resources)
    {
        if(rule.object != obj->name)
            continue;
        if(climate.AllowsNaturally(pt, rule))
            return true;
        hasRule = true;
    }
    return !hasRule && (climate.GetTerrain(pt) & toFlag(MapGenTerrain::Land)) != 0;
}

bool nofCustomPlanter::CanPlantAt(const GameWorldBase& world, const MapPoint pt, const civ::CustomBuilding& building)
{
    if(!building.IsPlanter())
        return false;
    // Only decoration and signs may be removed
    const NodalObjectType noType = world.GetNO(pt)->GetType();
    if(noType != NodalObjectType::Environment && noType != NodalObjectType::Nothing)
        return false;
    if(world.GetNode(pt).boundary_stones[BorderStonePos::OnPoint])
        return false;
    for(const auto dir : helpers::EnumRange<Direction>{})
    {
        if(world.GetPointRoad(pt, dir) != PointRoad::None)
            return false;
    }
    // Not next to buildings or other fields and objects which block the way: There must stay a way between them.
    // Flags are allowed next to it like next to stones
    for(const MapPoint nb : world.GetNeighbours(pt))
    {
        switch(world.GetNO(nb)->GetType())
        {
            case NodalObjectType::Building:
            case NodalObjectType::Buildingsite:
            case NodalObjectType::Grainfield:
            case NodalObjectType::Grapefield:
            case NodalObjectType::CharburnerPile:
            case NodalObjectType::Deposit: return false;
            default: break;
        }
    }
    return GrowsNaturallyAt(world, pt, static_cast<unsigned>(building.plantObject));
}

bool nofCustomPlanter::HasWaresToPlant() const
{
    return GetBuildingDesc().workDesc.waresNeeded.empty() || workplace->WaresAvailable();
}

bool nofCustomPlanter::AreWaresAvailable() const
{
    // A plantation that only plants needs its wares for every trip
    return GetBuildingDesc().HarvestsPlants() || HasWaresToPlant();
}

nofFarmhand::PointQuality nofCustomPlanter::GetPointQuality(const MapPoint pt, const bool isBeforeWork) const
{
    const civ::CustomBuilding& building = GetBuildingDesc();
    if(const auto* deposit = world->GetSpecObj<noDeposit>(pt))
    {
        // Harvest grown objects of the own kind (also natural ones) first
        if(building.HarvestsPlants() && static_cast<int>(deposit->GetObjectIdx()) == building.plantObject
           && !deposit->IsGrowing())
            return PointQuality::Class1;
        return PointQuality::NotPossible;
    }
    // Plant: The wares (if any) are consumed when leaving
    if(isBeforeWork && !HasWaresToPlant())
        return PointQuality::NotPossible;
    if(!CanPlantAt(*world, pt, building))
        return PointQuality::NotPossible;
    return PointQuality::Class2;
}

void nofCustomPlanter::WalkingStarted()
{
    harvest_ = world->GetSpecObj<noDeposit>(dest) != nullptr;
    if(!harvest_ && !GetBuildingDesc().workDesc.waresNeeded.empty())
        workplace->ConsumeWares();
}

void nofCustomPlanter::DrawWorking(DrawPoint drawPt)
{
    const unsigned playerColor = world->GetPlayer(player).color;
    const Job look = civ::getLook(job_);
    const civ::WorkAnimation anim = harvest_ ? civ::getLookWorkAnimation(look) : civ::getLookPlantAnimation(look);
    const civ::CustomGraphics& gfx = LOADER.GetCivGraphics();
    const std::vector<glArchivItem_Bitmap_Player*>& ownFrames =
      harvest_ ? gfx.GetWorkFrames(job_) : gfx.GetPlantFrames(job_);
    const unsigned numFrames = ownFrames.empty() ? anim.numImages : static_cast<unsigned>(ownFrames.size());
    const unsigned nowId = GAMECLIENT.Interpolate(anim.interpolateSteps, current_ev);
    const unsigned frame = nowId % numFrames;
    if(ownFrames.empty())
        LOADER.GetPlayerImage("rom_bobs", anim.firstImage + frame)
          ->DrawFull(drawPt + anim.offset, COLOR_WHITE, playerColor);
    else
        ownFrames[frame]->DrawFull(drawPt, COLOR_WHITE, playerColor);
    if(anim.soundVolume > 0 && frame == std::min(anim.soundImage, numFrames - 1))
    {
        world->GetSoundMgr().playNOSound(anim.sound, *this, nowId, anim.soundVolume);
        was_sounding = true;
    }
}

unsigned short nofCustomPlanter::GetCarryID() const
{
    // Carry the product like a carrier (graphics of the ware)
    return CARRY_ID_CARRIER_OFFSET + static_cast<unsigned short>(ware ? *ware : GetBuildingDesc().product);
}

helpers::OptionalEnum<GoodType> nofCustomPlanter::ProduceExtraWare(const GoodType firstWare)
{
    if(!harvest_ || !HarvestPiece())
        return std::nullopt;
    return firstWare;
}

bool nofCustomPlanter::HarvestPiece()
{
    auto* deposit = world->GetSpecObj<noDeposit>(pos);
    if(!deposit || deposit->IsGrowing() || static_cast<int>(deposit->GetObjectIdx()) != GetBuildingDesc().plantObject)
        return false;
    if(deposit->IsSmall())
    {
        // Last ware: The object disappears
        world->DestroyNO(pos);
        world->RecalcBQAroundPoint(pos);
    } else
        deposit->Collect();
    return true;
}

void nofCustomPlanter::WorkFinished()
{
    const civ::CustomBuilding& building = GetBuildingDesc();
    ware = std::nullopt;
    if(harvest_)
    {
        if(HarvestPiece())
            ware = building.product;
    } else
    {
        // The point may have become unusable meanwhile (e.g. a flag next to it): Then the wares are lost
        if(!CanPlantAt(*world, pos, building))
            return;
        const auto objIdx = static_cast<unsigned>(building.plantObject);
        world->DestroyNO(pos, false);
        world->SetNO(pos, noDeposit::CreatePlanted(objIdx));
        world->RecalcBQAroundPoint(pos);
    }
    if(world->GetGameInterface())
        world->GetGameInterface()->GI_UpdateMinimap(pos);
}
