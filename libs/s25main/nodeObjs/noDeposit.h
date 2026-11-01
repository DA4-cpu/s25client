// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "noBase.h"
class FOWObject;
class SerializedGameData;

/// Object on the map defined in lua (rttr:AddMapObject, civilization mode) which a workshop collects like the
/// stonemason the granite, e.g. a loam deposit. Each collected ware makes it one size smaller.
class noDeposit final : public noBase
{
    /// Index of the map object (civ::Content::GetMapObject)
    uint8_t objIdx;
    /// 0 = smallest size (last ware) .. sizes - 1
    uint8_t state;

public:
    noDeposit(unsigned objIdx, unsigned state);
    noDeposit(SerializedGameData& sgd, unsigned obj_id);

    void Destroy() override {}
    void Serialize(SerializedGameData& sgd) const override;

    GO_Type GetGOT() const final { return GO_Type::Deposit; }

    void Draw(DrawPoint drawPt) override;

    BlockingManner GetBM() const override { return BlockingManner::FlagsAround; }

    std::unique_ptr<FOWObject> CreateFOWObject() const override;

    unsigned GetObjectIdx() const { return objIdx; }
    /// Current size: 0 = smallest (one ware left)
    unsigned GetSize() const { return state; }
    /// True if only one ware is left (the object disappears after collecting it)
    bool IsSmall() const { return state == 0; }
    /// Collect one ware: one size smaller
    void Collect();
};
