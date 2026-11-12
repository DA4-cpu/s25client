// Copyright (C) 2005 - 2021 Settlers Freaks (sf-team at siedler25.org)
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "nofStonemason.h"

#include "GameInterface.h"
#include "GamePlayer.h"
#include "Loader.h"
#include "SoundManager.h"
#include "network/GameClient.h"
#include "ogl/glArchivItem_Bitmap_Player.h"
#include "world/GameWorld.h"
#include "nodeObjs/noGranite.h"

nofStonemason::nofStonemason(const MapPoint pos, const unsigned char player, nobUsual* workplace)
    : nofFarmhand(Job::Stonemason, pos, player, workplace)
{}

nofStonemason::nofStonemason(SerializedGameData& sgd, const unsigned obj_id) : nofFarmhand(sgd, obj_id) {}

/// Malt den Arbeiter beim Arbeiten
void nofStonemason::DrawWorking(DrawPoint drawPt)
{
    unsigned now_id;

    // Stein hauen
    LOADER.GetPlayerImage("rom_bobs", 40 + (now_id = GAMECLIENT.Interpolate(64, current_ev)) % 8)
      ->DrawFull(drawPt, COLOR_WHITE, world->GetPlayer(player).color);

    if(now_id % 8 == 5)
    {
        world->GetSoundMgr().playNOSound(56, *this, now_id);
        was_sounding = true;
    }
}

unsigned short nofStonemason::GetCarryID() const
{
    return 63;
}

/// Abgeleitete Klasse informieren, wenn sie anfängt zu arbeiten (Vorbereitungen)
void nofStonemason::WorkStarted() {}

/// Abgeleitete Klasse informieren, wenn fertig ist mit Arbeiten
void nofStonemason::WorkFinished()
{
    HewStone();
    // Stein in die Hand nehmen
    ware = GoodType::Stones;
}

helpers::OptionalEnum<GoodType> nofStonemason::ProduceExtraWare(const GoodType firstWare)
{
    if(!HewStone())
        return std::nullopt;
    return firstWare;
}

bool nofStonemason::HewStone()
{
    auto* granite = world->GetSpecObj<noGranite>(pos);
    if(!granite)
        return false;
    // Stein abhauen (wenn er nur noch ganz klein ist, dann wird er von der Landkarte getilgt)
    if(granite->IsSmall())
    {
        // Granitklötzchen löschen
        world->DestroyNO(pos);

        // Minimap Bescheid geben (Granitglötzchen muss weg)
        if(world->GetGameInterface())
            world->GetGameInterface()->GI_UpdateMinimap(pos);

        // Drumherum BQ neu berechnen, da diese sich ja jetzt hätten ändern können
        world->RecalcBQAroundPoint(pos);
    } else
        // ansonsten wird er um 1 kleiner
        granite->Hew();
    return true;
}

/// Returns the quality of this working point or determines if the worker can work here at all
nofFarmhand::PointQuality nofStonemason::GetPointQuality(const MapPoint pt, bool /* isBeforeWork */) const
{
    // An dieser Position muss es nur Stein geben
    return ((world->GetNO(pt)->GetType() == NodalObjectType::Granite) ? PointQuality::Class1 :
                                                                        PointQuality::NotPossible);
}
