// SH-1009 Phase B (UX-20): taglib embedded-artwork extraction — MP3 ID3v2
// APIC, FLAC PICTURE, Ogg Vorbis Xiph METADATA_BLOCK_PICTURE. Front cover
// wins over other picture types; the first picture is the fallback. Every
// failure path is the documented clean miss (never throws).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "TagLibArtworkReader.h"

#include <taglib/attachedpictureframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/tfile.h>
#include <taglib/tstring.h>
#include <taglib/vorbisfile.h>
#include <taglib/xiphcomment.h>

namespace shanty {
namespace {

Artwork artworkFrom(const TagLib::ByteVector& bytes,
                    const TagLib::String& mimeType) {
    Artwork art;
    art.mimeType = mimeType.toCString(true);  // UTF-8 (usually ASCII)
    art.data.assign(bytes.data(), bytes.size());
    return art;
}

/// Picks the front cover out of a picture list, else the first picture.
template <typename PictureList>
Artwork pickPicture(const PictureList& pictures) {
    for (auto* picture : pictures) {
        if (picture != nullptr &&
            picture->type() == TagLib::FLAC::Picture::FrontCover) {
            return artworkFrom(picture->data(), picture->mimeType());
        }
    }
    for (auto* picture : pictures) {
        if (picture != nullptr) {
            return artworkFrom(picture->data(), picture->mimeType());
        }
    }
    return Artwork();
}

}  // namespace

Artwork TagLibArtworkReader::read(const std::string& path) {
    // No audio properties needed — only the tag blocks are of interest.
    TagLib::FileRef ref(path.c_str(), false);
    if (ref.isNull() || ref.file() == nullptr) {
        return Artwork();  // clean miss: absent/unreadable/unsupported
    }
    TagLib::File* file = ref.file();

    // MP3: ID3v2 attached pictures (APIC frames).
    if (auto* mpeg = dynamic_cast<TagLib::MPEG::File*>(file)) {
        TagLib::ID3v2::Tag* tag = mpeg->ID3v2Tag(false);
        if (tag == nullptr) {
            return Artwork();
        }
        const TagLib::ID3v2::FrameList& frames = tag->frameList("APIC");
        TagLib::ID3v2::AttachedPictureFrame* first = nullptr;
        for (auto* frame : frames) {
            auto* picture =
                dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frame);
            if (picture == nullptr) {
                continue;
            }
            if (picture->type() ==
                TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
                return artworkFrom(picture->picture(), picture->mimeType());
            }
            if (first == nullptr) {
                first = picture;
            }
        }
        return first != nullptr
                   ? artworkFrom(first->picture(), first->mimeType())
                   : Artwork();
    }

    // FLAC: native PICTURE metadata blocks.
    if (auto* flac = dynamic_cast<TagLib::FLAC::File*>(file)) {
        return pickPicture(flac->pictureList());
    }

    // Ogg Vorbis: the Xiph METADATA_BLOCK_PICTURE comment, decoded by
    // taglib into the same FLAC::Picture value type.
    if (auto* vorbis = dynamic_cast<TagLib::Ogg::Vorbis::File*>(file)) {
        TagLib::Ogg::XiphComment* tag = vorbis->tag();
        if (tag == nullptr) {
            return Artwork();
        }
        return pickPicture(tag->pictureList());
    }

    return Artwork();  // container without a cover contract yet
}

}  // namespace shanty
