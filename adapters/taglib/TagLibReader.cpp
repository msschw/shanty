// SH-1001 Phase B (SH-E10 UX-12): the concrete taglib reader. See the
// header for the contract (clean miss, UTF-8 boundary, 2.x union API).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "TagLibReader.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>

namespace shanty {

TagMetadata TagLibReader::read(const std::string& path) {
    TagMetadata md;

    // FileRef dispatches by extension; a missing file, a directory or
    // an unparseable file yields a null/invalid ref — the documented
    // clean miss (TagMetadata::isEmpty()).
    TagLib::FileRef ref(path.c_str());
    if (ref.isNull() || !ref.file() || !ref.file()->isValid() || !ref.tag()) {
        return md;
    }

    const TagLib::Tag* tag = ref.tag();
    md.title = tag->title().toCString(true);    // true = UTF-8
    md.artist = tag->artist().toCString(true);
    md.album = tag->album().toCString(true);
    md.genre = tag->genre().toCString(true);

    // Duration is audio data, not a tag: present even for a file with
    // no tags at all (the untagged fixture's case).
    if (const TagLib::AudioProperties* properties = ref.audioProperties()) {
        md.durationSeconds = properties->lengthInSeconds();
    }
    return md;
}

}  // namespace shanty
