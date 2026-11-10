// SH-605 test double (stub logic): the IAutoEqReader port replaced by a
// canned result so the SettingsViewModel IMPORT FLOW is asserted while the
// real parser stays covered by tests/autoeq (SH-503). Records every call's
// exact arguments; the tests decide pass/fail expectations, so the fake is
// only programmable, never trusted for parsing.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <vector>

#include "core/domain/AutoEqParseResult.h"
#include "core/domain/EqualizerProfile.h"
#include "core/ports/IAutoEqReader.h"

class FakeAutoEqReader : public shanty::IAutoEqReader {
public:
    /// The result every read() returns verbatim (fresh fixture per test
    /// method, so one canned result per test is enough).
    shanty::AutoEqParseResult next;

    int readCalls = 0;
    std::vector<std::string> readData;
    std::vector<std::string> readFilePaths;

    shanty::AutoEqParseResult read(const std::string& data,
                                   const std::string& filePath) override {
        ++readCalls;
        readData.push_back(data);
        readFilePaths.push_back(filePath);
        return next;
    }
};
