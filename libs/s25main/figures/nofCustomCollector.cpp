// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nofCustomCollector.h"
#include "GameInterface.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "SoundManager.h"
#include "buildings/nobUsual.h"
#include "civ/Content.h"
#include "civ/CustomGraphics.h"
#include "network/GameClient.h"
#include "ogl/glArchivItem_Bitmap_Player.h"
#include "ogl/glSmartBitmap.h"
#include "world/GameWorld.h"
#include "nodeObjs/noDeposit.h"
#include "gameData/JobConsts.h"
#include <algorithm>
#include <vector>

nofCustomCollector::nofCustomCollector(const Job job, const MapPoint pos, const unsigned char player,
                                       nobUsual* workplace)
    : nofFarmhand(job, pos, player, workplace)
{
    RTTR_Assert(isCustomJob(job));
}

nofCustomCollector::nofCustomCollector(SerializedGameData& sgd, const unsigned obj_id) : nofFarmhand(sgd, obj_id) {}

const civ::CustomBuilding& nofCustomCollector::GetBuildingDesc() const
{
    const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(getCustomBuilding(getCustomJobIdx(job_)));
    RTTR_Assert(building && building->IsCollector());
    return *building;
}

unsigned nofCustomCollector::GetCollectRadius(const Job job)
{
    if(!isCustomJob(job))
        return 0;
    const civ::CustomBuilding* building = civ::getActiveContent().GetBuilding(getCustomBuilding(getCustomJobIdx(job)));
    return (building && building->IsCollector()) ? building->collectRadius : 0u;
}

void nofCustomCollector::DrawWorking(DrawPoint drawPt)
{
    const unsigned playerColor = world->GetPlayer(player).color;
    // Like the look job (e.g. dig with the shovel like the planer) or the own images of the worker
    const civ::WorkAnimation anim = civ::getLookWorkAnimation(civ::getLook(job_));
    const std::vector<glArchivItem_Bitmap_Player*>& ownFrames = LOADER.GetCivGraphics().GetWorkFrames(job_);
    const unsigned numFrames = ownFrames.empty() ? anim.numImages : static_cast<unsigned>(ownFrames.size());
    const unsigned nowId = GAMECLIENT.Interpolate(anim.interpolateSteps, current_ev);
    const unsigned frame = nowId % numFrames;
    if(ownFrames.empty())
        LOADER.GetPlayerImage("rom_bobs", anim.firstImage + frame)
          ->DrawFull(drawPt + anim.offset, COLOR_WHITE, playerColor);
    else
        ownFrames[frame]->DrawFull(drawPt, COLOR_WHITE, playerColor);
    if(frame == std::min(anim.soundImage, numFrames - 1))
    {
        world->GetSoundMgr().playNOSound(anim.sound, *this, nowId, anim.soundVolume);
        was_sounding = true;
    }
}

unsigned short nofCustomCollector::GetCarryID() const
{
    // Carry the product like a carrier (graphics of the ware)
    return CARRY_ID_CARRIER_OFFSET + static_cast<unsigned short>(ware ? *ware : GetBuildingDesc().product);
}

void nofCustomCollector::WorkFinished()
{
    auto* deposit = world->GetSpecObj<noDeposit>(pos);
    if(!deposit)
        return; // Gone (should not happen as the point is reserved)
    if(deposit->IsSmall())
    {
        // Last ware: The object disappears
        world->DestroyNO(pos);
        if(world->GetGameInterface())
            world->GetGameInterface()->GI_UpdateMinimap(pos);
        world->RecalcBQAroundPoint(pos);
    } else
        deposit->Collect();

    ware = GetBuildingDesc().product;
}

nofFarmhand::PointQuality nofCustomCollector::GetPointQuality(const MapPoint pt, bool /* isBeforeWork */) const
{
    const auto* deposit = world->GetSpecObj<noDeposit>(pt);
    if(deposit && static_cast<int>(deposit->GetObjectIdx()) == GetBuildingDesc().collectObject)
        return PointQuality::Class1;
    return PointQuality::NotPossible;
}
