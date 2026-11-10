// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "SettingsService.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace shanty {
// SH-501 Phase B: named equalizer profile persistence over ISettingsStore.
//
// Persistence wire format decision (design D5 — profiles are data through an
// abstraction; the port offers only a string->string map, so the encoding is
// our choice, recorded here per the ticket note):
//
//   Key:    "equalizer/profiles" — a SINGLE store key holds every profile.
//   Value:  line-based text, one profile per line:
//               <name>\t<g1>,<g2>,...,<gN>
//
//   - Names are framed by the tab separator, so name characters that would
//     break the framing are backslash-escaped: '\' -> "\\", tab -> "\t",
//     newline -> "\n", carriage return -> "\r". Anything else is verbatim.
//   - Gains are formatted with std::max_digits10 (17) significant digits,
//     which round-trips every double exactly; parsed back with strtod.
//   - Records are separated by '\n'. On read, empty or malformed lines are
//     skipped, and profiles that fail validation are dropped — corrupted or
//     forward-incompatible data degrades to "profile missing", never to a
//     crash or to feeding invalid gains to the equalizer (SH-502).
//   - Rewriting the single value on each save/delete keeps every store
//     update atomic: readers never observe a torn multi-profile state.
//   - The "equalizer/" key namespace leaves room for other settings domains
//     (player options in SH-605 etc.).
//
// Core stays pure C++ (no Qt): formatting via <sstream>/<iomanip> only.

namespace {

const char kProfilesKey[] = "equalizer/profiles";

std::string escapeName(const std::string& name) {
    std::string escaped;
    escaped.reserve(name.size());
    for (char ch : name) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '\t': escaped += "\\t";  break;
            case '\n': escaped += "\\n";  break;
            case '\r': escaped += "\\r";  break;
            default:   escaped += ch;     break;
        }
    }
    return escaped;
}

/// Lenient inverse of escapeName(): unknown escape sequences decode to the
/// escaped character itself, so hand-edited values still round trip names
/// that only contain escaped framing characters ('\t', '\\', '\n', '\r').
std::string unescapeName(const std::string& escaped) {
    std::string name;
    name.reserve(escaped.size());
    for (std::size_t i = 0; i < escaped.size(); ++i) {
        if (escaped[i] != '\\' || i + 1 >= escaped.size()) {
            name += escaped[i];
            continue;
        }
        switch (escaped[++i]) {
            case 't': name += '\t'; break;
            case 'n': name += '\n'; break;
            case 'r': name += '\r'; break;
            default:  name += escaped[i]; break;  // covers '\\' too
        }
    }
    return name;
}

}  // namespace

namespace {

/// The port has no single-key read, so pick the profiles value out of
/// readAll(); missing key -> empty value.
std::string readProfileValue(ISettingsStore& store) {
    const std::map<std::string, std::string> all = store.readAll();
    const auto it = all.find(kProfilesKey);
    return it == all.end() ? std::string() : it->second;
}

/// Parses the wire format; malformed lines are skipped (see format notes).
std::vector<EqualizerProfile> parseProfiles(const std::string& value) {
    std::vector<EqualizerProfile> profiles;
    std::istringstream stream(value);
    std::string line;
    while (std::getline(stream, line)) {
        const std::size_t nameEnd = line.find('\t');
        if (nameEnd == std::string::npos) {
            continue;  // no gains field at all: not a valid record
        }
        const std::string name = unescapeName(line.substr(0, nameEnd));

        std::vector<double> gains;
        std::istringstream gainsStream(line.substr(nameEnd + 1));
        std::string token;
        bool lineOk = true;
        while (std::getline(gainsStream, token, ',')) {
            char* parseEnd = nullptr;
            const double gain = std::strtod(token.c_str(), &parseEnd);
            if (parseEnd == token.c_str() || *parseEnd != '\0') {
                lineOk = false;  // not a clean double: skip the whole line
                break;
            }
            gains.push_back(gain);
        }
        if (!lineOk) {
            continue;
        }

        const EqualizerProfile profile = EqualizerProfile::make(name, gains);
        if (profile.isValid()) {
            profiles.push_back(profile);
        }
    }
    return profiles;
}

/// Serializes to the wire format (see format notes above the helpers).
std::string serializeProfiles(const std::vector<EqualizerProfile>& profiles) {
    std::ostringstream out;
    // max_digits10 guarantees exact double round trips through the text form.
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    bool firstRecord = true;
    for (const EqualizerProfile& profile : profiles) {
        if (!firstRecord) {
            out << '\n';
        }
        firstRecord = false;
        out << escapeName(profile.name()) << '\t';
        bool firstGain = true;
        for (double gain : profile.gains()) {
            if (!firstGain) {
                out << ',';
            }
            firstGain = false;
            out << gain;
        }
    }
    return out.str();
}

}  // namespace

SettingsService::SettingsService(ISettingsStore& store) : store_(store) {}

bool SettingsService::saveProfile(const EqualizerProfile& profile) {
    if (!profile.isValid()) {
        return false;  // rejected before touching the store: no write happens
    }

    std::vector<EqualizerProfile> profiles =
        parseProfiles(readProfileValue(store_));

    // Overwrite semantics: an existing profile with the same name is
    // replaced, not appended twice.
    profiles.erase(
        std::remove_if(profiles.begin(), profiles.end(),
                       [&profile](const EqualizerProfile& stored) {
                           return stored.name() == profile.name();
                       }),
        profiles.end());
    profiles.push_back(profile);

    store_.write(kProfilesKey, serializeProfiles(profiles));
    return true;
}

std::vector<EqualizerProfile> SettingsService::loadProfiles() const {
    return parseProfiles(readProfileValue(store_));
}

bool SettingsService::deleteProfile(const std::string& name) {
    const std::vector<EqualizerProfile> profiles =
        parseProfiles(readProfileValue(store_));

    std::vector<EqualizerProfile> remaining;
    bool found = false;
    for (const EqualizerProfile& profile : profiles) {
        if (profile.name() == name) {
            found = true;
            continue;
        }
        remaining.push_back(profile);
    }
    if (!found) {
        return false;
    }

    if (remaining.empty()) {
        store_.remove(kProfilesKey);  // drop the key once nothing is left
    } else {
        store_.write(kProfilesKey, serializeProfiles(remaining));
    }
    return true;
}

}  // namespace shanty
