// SH-601 Phase B / SH-801: NavigationFacade — the QObject wrapper ("exposed
// via QObject facades (controllers)", ISSUES.md §1.1) that bridges the
// pure-C++ NavigationViewModel to QML. Thin by design: every callback of
// the view model is mirrored to a Qt signal, every command is a Q_INVOKABLE
// forward; no navigation decisions are made here (stable Library root,
// show() as the one sibling command with atomic sibling replace, back only
// when a sibling sits on top — NavigationViewModel's documented rules).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include "presentation/viewmodels/NavigationViewModel.h"

namespace shanty {

class NavigationFacade : public QObject {
    Q_OBJECT

    /// "library" (stable root) | "settings" | "player" | "playlist".
    Q_PROPERTY(QString currentPage READ currentPage NOTIFY navigationChanged)
    /// Stack depth; 1 = bare root (no sibling page on top).
    Q_PROPERTY(int depth READ depth NOTIFY navigationChanged)
    /// True when back() is a valid command.
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationChanged)
    /// Player anchor string ("" when none) while the Player page is up.
    Q_PROPERTY(QString playerContext READ playerContext NOTIFY navigationChanged)
    /// Startup landing decision (SH-801 / UX-1): "player" when the
    /// landing says the Player page is pushed over the fresh Library root
    /// at startup (a queue exists — session queue, or a restored one once
    /// queue persistence lands), "library" for the bare root. Constant:
    /// the decision is made once, before the shell applies it.
    Q_PROPERTY(QString landing READ landing CONSTANT)

public:
    /// landingHasQueue: the startup queue presence feeding landingPage()
    /// (the app wiring passes the player's queue state after any restore).
    explicit NavigationFacade(NavigationViewModel& viewModel,
                              bool landingHasQueue = false,
                              QObject* parent = nullptr);

    // ---- QML commands (thin forwards; rejections come back through
    //      invalidNavigation(reason), surfaced as toasts by the shell) -----
    /// The ONE sibling command (SH-801): page is "player" | "settings" |
    /// "playlist" (lowercase view-model facade names). Valid from the root
    /// (push) and from any other sibling (atomic replace — the QML shell
    /// drives pageStack.replace from the navigationChanged fire); no-op
    /// when the page is already current or when page is "library" (the
    /// root is reached through back() only).
    Q_INVOKABLE void show(const QString& page, const QString& context = QString());
    Q_INVOKABLE void back();

    // ---- Q_PROPERTY accessors ----------------------------------------------
    QString currentPage() const;
    int depth() const;
    bool canGoBack() const;
    QString playerContext() const;
    QString landing() const;

signals:
    /// Fired after every actual push/pop/atomic sibling replace (view-model
    /// hook mirror).
    void navigationChanged();
    /// Fired for a command rejected as a documented no-op (reason is one of
    /// the NavigationViewModel catalogue strings, kept stable verbatim so
    /// the QML layer can map it to translated text; "show: unknown page"
    /// is a facade-level guard for malformed QML call sites).
    void invalidNavigation(const QString& reason);

private:
    NavigationViewModel& viewModel_;
    bool landingHasQueue_;
};

}  // namespace shanty
