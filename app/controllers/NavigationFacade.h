// SH-601 Phase B: NavigationFacade — the QObject wrapper ("exposed via
// QObject facades (controllers)", ISSUES.md §1.1) that bridges the pure-C++
// NavigationViewModel to QML. Thin by design: every callback of the view
// model is mirrored to a Qt signal, every command is a Q_INVOKABLE forward;
// no navigation decisions are made here (pushes only from the root, back
// only when a sibling sits on top — NavigationViewModel's documented rules).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include "presentation/viewmodels/NavigationViewModel.h"

namespace shanty {

class NavigationFacade : public QObject {
    Q_OBJECT

    /// "library" (root) | "settings" | "player".
    Q_PROPERTY(QString currentPage READ currentPage NOTIFY navigationChanged)
    /// Stack depth; 1 = bare root (no sibling page on top).
    Q_PROPERTY(int depth READ depth NOTIFY navigationChanged)
    /// True when back() is a valid command.
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationChanged)
    /// Player anchor string ("" when none) while the Player page is up.
    Q_PROPERTY(QString playerContext READ playerContext NOTIFY navigationChanged)

public:
    explicit NavigationFacade(NavigationViewModel& viewModel,
                              QObject* parent = nullptr);

    // ---- QML commands (thin forwards; rejections come back through
    //      invalidNavigation(reason), surfaced as toasts by the shell) -----
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void openPlayer(const QString& context = QString());
    Q_INVOKABLE void back();

    // ---- Q_PROPERTY accessors ----------------------------------------------
    QString currentPage() const;
    int depth() const;
    bool canGoBack() const;
    QString playerContext() const;

signals:
    /// Fired after every actual push/pop (view-model hook mirror).
    void navigationChanged();
    /// Fired for a command rejected as a documented no-op (reason is one of
    /// the NavigationViewModel catalogue strings, kept stable verbatim so
    /// the QML layer can map it to translated text).
    void invalidNavigation(const QString& reason);

private:
    NavigationViewModel& viewModel_;
};

}  // namespace shanty
