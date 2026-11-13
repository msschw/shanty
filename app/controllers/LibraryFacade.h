// SH-602 Phase B: LibraryFacade — the QObject wrapper bridging the pure-C++
// LibraryViewModel to QML. Thin: config mutations delegate to the view model
// and reach QML only through the mirrored onConfigChanged -> changed()
// signal (so rejected/no-op commands cause no UI churn); selection and scan
// carry no view-model event by contract, so those Q_INVOKABLEs publish the
// changed() signal themselves after the call.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "presentation/viewmodels/LibraryViewModel.h"

namespace shanty {

class LibraryFacade : public QObject {
    Q_OBJECT

    // Config lists and the filtered listing, exposed as plain string lists
    // (std::string paths converted at this boundary; D3 — the view model
    // deals in plain UTF-8 paths only).
    Q_PROPERTY(QVariantList folders READ folders NOTIFY changed)
    Q_PROPERTY(QVariantList whitelist READ whitelist NOTIFY changed)
    Q_PROPERTY(QVariantList blacklist READ blacklist NOTIFY changed)
    /// Last scan() result (empty until the first scan).
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY changed)
    /// Selected indices of the current listing (ascending ints).
    Q_PROPERTY(QVariantList selection READ selection NOTIFY changed)

public:
    explicit LibraryFacade(LibraryViewModel& viewModel,
                           QObject* parent = nullptr);

    // ---- configuration (folders / whitelist / blacklist) -------------------
    // Rejections come back through invalidReason(reason) with the view
    // model's catalogue strings (surfaced as toasts by the shell).
    Q_INVOKABLE void addFolder(const QString& path);
    Q_INVOKABLE void removeFolder(const QString& path);
    Q_INVOKABLE void addWhitelistEntry(const QString& path);
    Q_INVOKABLE void removeWhitelistEntry(const QString& path);
    Q_INVOKABLE void addBlacklistEntry(const QString& path);
    Q_INVOKABLE void removeBlacklistEntry(const QString& path);
    Q_INVOKABLE void reload();

    // ---- listing / selection ------------------------------------------------
    /// Runs one view-model scan() with the current config; publishes
    /// changed() (the listing + selection properties update in QML).
    Q_INVOKABLE QVariantList scan();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    /// Out-of-range indices surface the view model's catalogue reason.
    Q_INVOKABLE void setSelected(int index, bool selected);
    /// Selected tracks' paths, ascending by listing order (queue-ready).
    Q_INVOKABLE QVariantList selectionPaths();

    // ---- SH-809 (UX-8): row context-menu catalogue ---------------------------
    /// The pressAndHold menu commands for the track row `index`, in stable
    /// QML string form (mirror of LibraryViewModel::rowCommands): an empty
    /// list is the "do not open" signal (no row), otherwise the documented
    /// library-row catalogue ["play", "addToQueue"]. The QML menu items map
    /// 1:1 onto these; the bridges own the rejection semantics.
    Q_INVOKABLE QVariantList rowCommands(int index) const;

    /// Home directory (start point for the QML folder picker page; the
    /// picker itself stays facade-agnostic, so the facades that can open it
    /// each carry this same one-line hook).
    Q_INVOKABLE QString homeDir() const;
    /// Directory listing for FilePickerPage (see PickerSupport.h); the
    /// library page passes this to its folder picker.
    Q_INVOKABLE QVariantList listDirectory(const QString& path,
                                           const QVariantList& nameFilters)
        const;

    // ---- Q_PROPERTY accessors ------------------------------------------------
    QVariantList folders() const;
    QVariantList whitelist() const;
    QVariantList blacklist() const;
    QVariantList tracks() const;
    QVariantList selection() const;

signals:
    /// Fired after each effective, persisted config mutation (view-model
    /// onConfigChanged mirror) and after scan/selection changes (published
    /// by this facade — see the class comment).
    void changed();
    /// Rejected add / invalid selection index (view-model onInvalid mirror).
    void invalidReason(const QString& reason);

private:
    static QVariantList toStringList(const std::vector<std::string>& paths);

    LibraryViewModel& viewModel_;
};

}  // namespace shanty
