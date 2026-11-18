// Copyright (C) 2026 DA4-cpu
//
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <boost/filesystem/path.hpp>
#include <limits>

/// Play a replay without user interface: the recorded commands of all players, GF by GF, with the checksums of the
/// game state compared (asynchronous: the game data differs from the recorded game). `runAIs`: the AI players of
/// the replay run as in the game (their commands are not executed, the recorded ones are) to find crashes of the AI.
/// Returns 0 if the replay was played to its end (or `maxGF`), 1 on an error, 2 if it got asynchronous
int playReplay(const boost::filesystem::path& path, unsigned maxGF = std::numeric_limits<unsigned>::max(),
               bool runAIs = false);
