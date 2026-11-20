// SH-1009 Phase B (UX-20): the QML artwork image provider — see the header.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include "ArtworkImageProvider.h"

#include <QByteArray>
#include <QMutexLocker>

namespace shanty {

ArtworkImageProvider::ArtworkImageProvider(IArtworkReader& reader)
    : QQuickImageProvider(QQuickImageProvider::Image), reader_(reader) {}

QImage ArtworkImageProvider::requestImage(const QString& id, QSize* size,
                                          const QSize& requestedSize) {
    if (size != nullptr) {
        *size = QSize();
    }

    // Qt Quick may keep the leading slash of the URL path; the token
    // itself is base64url (no slashes, no percent sequences).
    QString token = id;
    if (token.startsWith(QLatin1Char('/'))) {
        token.remove(0, 1);
    }

    // base64url token → the representative track path.
    const QByteArray raw =
        QByteArray::fromBase64(token.toLatin1(), QByteArray::Base64UrlEncoding);
    const QString path = QString::fromUtf8(raw);
    if (path.isEmpty()) {
        return QImage();
    }

    QMutexLocker locker(&mutex_);

    QImage image;
    const std::map<QString, QImage>::const_iterator cached = cache_.find(path);
    if (cached != cache_.end()) {
        image = cached->second;
    } else {
        const Artwork art = reader_.read(path.toStdString());
        if (art.isEmpty()) {
            return QImage();  // clean miss → the QML kind-icon fallback
        }
        image = QImage::fromData(
            reinterpret_cast<const uchar*>(art.data.data()),
            static_cast<int>(art.data.size()));
        if (image.isNull()) {
            return QImage();  // unreadable picture → same fallback
        }
        remember(path, image);
    }

    if (size != nullptr) {
        *size = image.size();
    }
    // The delegate asks for a small tile; scale a copy, keep the full
    // decode in the cache for other sizes.
    if (requestedSize.isValid()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    return image;
}

void ArtworkImageProvider::remember(const QString& path,
                                    const QImage& image) {
    if (cache_.size() >= kMaxCachedImages && !order_.empty()) {
        cache_.erase(order_.front());
        order_.pop_front();
    }
    cache_[path] = image;
    order_.push_back(path);
}

}  // namespace shanty
