// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "noDeposit.h"
#include "FOWObjects.h"
#include "SerializedGameData.h"
#include "civ/Graphics.h"

noDeposit::noDeposit(const unsigned objIdx, const unsigned state)
    : noBase(NodalObjectType::Deposit), objIdx(static_cast<uint8_t>(objIdx)), state(static_cast<uint8_t>(state))
{}

noDeposit::noDeposit(SerializedGameData& sgd, const unsigned obj_id)
    : noBase(sgd, obj_id), objIdx(sgd.PopUnsignedChar()), state(sgd.PopUnsignedChar())
{}

void noDeposit::Serialize(SerializedGameData& sgd) const
{
    noBase::Serialize(sgd);
    sgd.PushUnsignedChar(objIdx);
    sgd.PushUnsignedChar(state);
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
