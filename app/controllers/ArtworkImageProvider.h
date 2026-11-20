// SH-1009 (UX-20): QML image provider for library artwork tiles.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
//
// QML asks for image://shantyart/<token>, where <token> is the base64url
// of a representative track path (LibraryBrowserFacade::rowToMap). The
// token is URL-safe, so no percent-encoding ambiguity arises between Qt's
// QML image loader and this provider.
//
// The provider owns a small in-memory cache of DECODED images (bounded;
// FIFO eviction): list scrolling and mode switches must not re-read and
// re-decode the same cover. Absent pictures decode to a null image and the
// QML side falls back to its kind icon. Nothing is written to disk.
#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

#include <cstddef>
#include <deque>
#include <map>

#include "core/ports/IArtworkReader.h"

namespace shanty {

class ArtworkImageProvider : public QQuickImageProvider {
public:
    explicit ArtworkImageProvider(IArtworkReader& reader);

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

private:
    void remember(const QString& path, const QImage& image);

    IArtworkReader& reader_;
    /// Qt Quick may call requestImage from its pixmap thread as well as
    /// the GUI thread; the cache is the only shared mutable state.
    mutable QMutex mutex_;
    std::map<QString, QImage> cache_;
    std::deque<QString> order_;
    static const std::size_t kMaxCachedImages = 64;
};

}  // namespace shanty
