// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "noBase.h"
class FOWObject;
class GameEvent;
class SerializedGameData;

/// Object on the map defined in lua (rttr:AddMapObject, civilization mode) which a workshop collects like the
/// stonemason the granite, e.g. a loam deposit. Each collected ware makes it one size smaller.
/// Objects planted by a plantation grow first: They start with the smallest size and get one size bigger in equal
/// steps until they reach the planted size. Only then they can be collected.
class noDeposit final : public noBase
{
    /// Index of the map object (civ::Content::GetMapObject)
    uint8_t objIdx;
    /// 0 = smallest size (last ware) .. sizes - 1
    uint8_t state;
    /// Size the object grows to (only while growing)
    uint8_t grownState;
    /// Next growth step or nullptr if grown
    const GameEvent* growEvent;

public:
    noDeposit(unsigned objIdx, unsigned state);
    noDeposit(SerializedGameData& sgd, unsigned obj_id);
    /// New planted object of the map object (index): Smallest size, grows to its plantSize within its growTime
    static noDeposit* CreatePlanted(unsigned objIdx);

    void Destroy() override;
    void Serialize(SerializedGameData& sgd) const override;
    void HandleEvent(unsigned id) override;

    GO_Type GetGOT() const final { return GO_Type::Deposit; }

    void Draw(DrawPoint drawPt) override;

    BlockingManner GetBM() const override { return BlockingManner::FlagsAround; }

    std::unique_ptr<FOWObject> CreateFOWObject() const override;

    unsigned GetObjectIdx() const { return objIdx; }
    /// Current size: 0 = smallest (one ware left)
    unsigned GetSize() const { return state; }
    /// True if only one ware is left (the object disappears after collecting it)
    bool IsSmall() const { return state == 0; }
    /// True while a planted object grows (it can not be collected yet)
    bool IsGrowing() const { return growEvent != nullptr; }
    /// Collect one ware: one size smaller
    void Collect();
};
