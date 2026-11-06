// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "noDeposit.h"
#include "EventManager.h"
#include "FOWObjects.h"
#include "SerializedGameData.h"
#include "civ/Content.h"
#include "civ/Graphics.h"
#include <algorithm>

noDeposit::noDeposit(const unsigned objIdx, const unsigned state)
    : noBase(NodalObjectType::Deposit), objIdx(static_cast<uint8_t>(objIdx)), state(static_cast<uint8_t>(state)),
      grownState(static_cast<uint8_t>(state)), growEvent(nullptr)
{}

noDeposit::noDeposit(SerializedGameData& sgd, const unsigned obj_id)
    : noBase(sgd, obj_id), objIdx(sgd.PopUnsignedChar()), state(sgd.PopUnsignedChar()), grownState(state),
      growEvent(nullptr)
{
    if(sgd.GetGameDataVersion() >= 21)
    {
        grownState = sgd.PopUnsignedChar();
        growEvent = sgd.PopEvent();
    }
}

namespace {
/// GFs per growth step of a planted object
unsigned getGrowStepGFs(unsigned objIdx, unsigned grownState)
{
    const civ::CustomMapObject* obj = civ::getActiveContent().GetMapObject(objIdx);
    const unsigned growGFs = obj ? obj->growGFs : 1u;
    return std::max(1u, growGFs / std::max(1u, grownState));
}
} // namespace

noDeposit* noDeposit::CreatePlanted(const unsigned objIdx)
{
    const civ::CustomMapObject* obj = civ::getActiveContent().GetMapObject(objIdx);
    const unsigned plantSize = obj ? std::max(1u, obj->plantSize) : 1u;
    auto* deposit = new noDeposit(objIdx, 0);
    deposit->grownState = static_cast<uint8_t>(plantSize - 1u);
    deposit->growEvent = GetEvMgr().AddEvent(deposit, getGrowStepGFs(objIdx, deposit->grownState));
    return deposit;
}

void noDeposit::Destroy()
{
    GetEvMgr().RemoveEvent(growEvent);
}

void noDeposit::Serialize(SerializedGameData& sgd) const
{
    noBase::Serialize(sgd);
    sgd.PushUnsignedChar(objIdx);
    sgd.PushUnsignedChar(state);
    sgd.PushUnsignedChar(grownState);
    sgd.PushEvent(growEvent);
}

void noDeposit::HandleEvent(const unsigned /*id*/)
{
    growEvent = nullptr;
    if(state < grownState)
        ++state;
    if(state < grownState)
        growEvent = GetEvMgr().AddEvent(this, getGrowStepGFs(objIdx, grownState));
}

void noDeposit::Draw(DrawPoint drawPt)
{
    civ::drawMapObject(objIdx, state, drawPt);
}

std::unique_ptr<FOWObject> noDeposit::CreateFOWObject() const
{
    return std::make_unique<fowDeposit>(objIdx, state);
}

void noDeposit::Collect()
{
    if(state)
        --state;
}
