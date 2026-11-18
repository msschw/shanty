// SH-702 Phase A: packaging & release lint — extends the SH-102
// spec-lint pattern from "license tag present" to the full SDK-required
// template surface. Cases: spec template keys, %changelog, %files
// install-set coverage, shipped icon sizes, DISTFILES/TRANSLATIONS
// existence, and a SOFT .ts-vs-QML parity check (decision below).
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

// Case roster (1–2 red at Phase A, 3–5 contract locks green by design):
//   1. rpmSpecDeclaresSdkTemplateKeys          (License/Source0/Requires + canonical BRs)
//   2. rpmSpecChangelogDocumentsInitialRelease (%changelog with 0.1-1 + GPL notice)
//   3. rpmSpecFilesCoverInstallSetFromAppTemplate (%files covers the qmake install set)
//   4. declaredIconSizesShipPngFiles           (SAILFISHAPP_ICONS -> icons/<size>/shanty.png)
//   5. distfilesAndTranslationSourcesExist     (DISTFILES/TRANSLATIONS paths on disk)
//   6. qmlUserVisibleStringsHaveCatalogueEntries (SOFT, see the case)
//   7. rpmSpecDeclaresEqualizerDependencies    (SH-901: the gst engine contract)
//   8. rpmSpecDeclaresTaglibDependencies       (SH-1001: the tag reader contract)
//   9. settingsEntryBindsTheSystemSettingsPageToTheDesktopFile (SH-1008)
//  10. settingsPluginProjectDeclaresTheQmlModule                (SH-1008)
//  11. settingsPageManagesTheLibraryConfigThroughThePlugin      (SH-1008)
//  12. specFilesShipTheSettingsEntryAndTheQmlPlugin             (SH-1008)

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

namespace {

QString readFileOrThrow(const QString& absolutePath) {
    QFile file(absolutePath);
    return file.open(QIODevice::ReadOnly | QIODevice::Text)
               ? QString::fromUtf8(file.readAll())
               : QString();
}

// Read a file and require success; reports failure location on the caller
// via QTest::qFail (message names the file).
bool requireReadable(const QString& absolutePath, const QString& what) {
    QFile file(absolutePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) return true;
    QTest::qFail(qPrintable(QStringLiteral("unreadable %1: %2")
                                .arg(what, absolutePath)),
                 __FILE__, __LINE__);
    return false;
}

// Spec section text: from the line that begins `sectionName` up to the
// next "%" directive line or EOF.
QString specSection(const QString& spec, const QString& sectionName) {
    const QList<QString> lines = spec.split(QChar::fromLatin1('\n'));
    QString section;
    int at = -1;
    for (int i = 0; i < lines.count(); ++i) {
        if (lines.at(i).trimmed().startsWith(sectionName)) {
            at = i;
            break;
        }
    }
    if (at == -1) return section;
    // A NEW section header ends the current one; %defattr and %doc live
    // inside %files and must not terminate it.
    static const QRegularExpression sectionHeader(
        QStringLiteral("^%(files|description|changelog|prep|build|install|"
                       "check|clean|package|pre|post|preun|postun|"
                       "posttrans)\\b"));
    for (int i = at + 1; i < lines.count(); ++i) {
        const QString trimmed = lines.at(i).trimmed();
        if (sectionHeader.match(trimmed).hasMatch()) break;
        if (!trimmed.isEmpty() && !trimmed.startsWith(QChar::fromLatin1('#')))
            section += lines.at(i) + QChar::fromLatin1('\n');
    }
    return section;
}

// Multi-line qmake variable (backslash continuation) tokens, e.g.
// DISTFILES += a \\\n    b \\\n    c — tokens only, continuation marks
// and the trailing '='-part stripped.
QStringList qmakeVariableTokens(const QString& projectText,
                                const QString& variable) {
    QStringList tokens;
    const QList<QString> lines = projectText.split(QChar::fromLatin1('\n'));
    bool collecting = false;
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (!collecting) {
            if (!line.startsWith(variable)) continue;
            collecting = true;
            const int eq = line.indexOf(QChar::fromLatin1('='));
            tokens << line.mid(eq + 1).split(QChar::fromLatin1(' '),
                                             Qt::SkipEmptyParts);
        } else {
            tokens << line.split(QChar::fromLatin1(' ')
                                 , Qt::SkipEmptyParts);
        }
        if (!rawLine.trimmed().endsWith(QChar::fromLatin1('\\'))) break;
    }
    for (QString& token : tokens) {
        while (token.endsWith(QChar::fromLatin1('\\')))
            token.chop(1);
        token = token.trimmed();
    }
    tokens.removeAll(QString());
    tokens.removeAll(QStringLiteral("+="));
    tokens.removeAll(QStringLiteral("="));
    return tokens;
}

// Every qsTr("…") string in one QML chunk.
QSet<QString> qsTrStringsIn(const QString& qml) {
    QSet<QString> strings;
    QRegularExpression re(QStringLiteral("qsTr\\(\\s*\"([^\"]*)\""));
    QRegularExpressionMatchIterator it = re.globalMatch(qml);
    while (it.hasNext()) strings.insert(it.next().captured(1));
    return strings;
}

// <source> strings from a .ts catalogue (any context).
QSet<QString> tsSourceStrings(const QString& ts) {
    QSet<QString> strings;
    QRegularExpression re(QStringLiteral("<source>([^<]*)</source>"));
    QRegularExpressionMatchIterator it = re.globalMatch(ts);
    while (it.hasNext()) strings.insert(it.next().captured(1));
    return strings;
}

}  // namespace

class TestPackaging : public QObject {
    Q_OBJECT

private slots:
    void rpmSpecDeclaresSdkTemplateKeys();
    void rpmSpecChangelogDocumentsInitialRelease();
    void rpmSpecFilesCoverInstallSetFromAppTemplate();
    void declaredIconSizesShipPngFiles();
    void distfilesAndTranslationSourcesExist();
    void qmlUserVisibleStringsHaveCatalogueEntries();
    void rpmSpecDeclaresEqualizerDependencies();
    void rpmSpecDeclaresTaglibDependencies();

    // ---- SH-1008 (UX-19): library config in the system Settings app --------
    void settingsEntryBindsTheSystemSettingsPageToTheDesktopFile();
    void settingsPluginProjectDeclaresTheQmlModule();
    void settingsPageManagesTheLibraryConfigThroughThePlugin();
    void specFilesShipTheSettingsEntryAndTheQmlPlugin();

private:
    static QString specPath() {
        return QStringLiteral(SHANTY_ROOT) +
               QStringLiteral("/rpm/shanty.spec");
    }
    static QString appProPath() {
        return QStringLiteral(SHANTY_ROOT) +
               QStringLiteral("/app/app.pro");
    }
    // Template artifacts live inside app/ (sailfishapp.prf contract):
    // desktop file, qml/ tree and icons/<size>/.
    static QString appPath() {
        return QStringLiteral(SHANTY_ROOT) + QStringLiteral("/app");
    }
    // TARGET of the shipped application; empty string reported as failure.
    QString appTarget();
};

QString TestPackaging::appTarget() {
    const QString appPro = readFileOrThrow(appProPath());
    QRegularExpression re(QStringLiteral("^TARGET\\s*=\\s*(\\S+)"),
                          QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = re.match(appPro);
    if (!match.hasMatch()) {
        QTest::qFail("app/app.pro must define TARGET (shipped binary name)",
                     __FILE__, __LINE__);
        return QString();
    }
    return match.captured(1);
}

// Case 1 — the spec must carry the canonical Sailfish SDK template keys.
// Rationale: mb2/sfdk (SDK "build in the SDK" path, SH-702 Phase B)
// resolves Source0 = %{name}-%{version}.tar.bz2 from this file. The
// RUNTIME Requires set must be device-installable: packages named
// pkgconfig(...) are -devel packages that exist only in the SDK —
// caught by the first emulator install (rpm -Uvh refused with
// "pkgconfig(Qt5Core) is needed by shanty"). Qt libs therefore come in
// through automatic soname dependencies; sailfishsilica-qt5 (UI
// framework, transitively pulls Qt5Core/Qml/Quick) is the only declared
// runtime dep. The pkgconfig() entries belong in BuildRequires, where
// the SDK build needs them.
void TestPackaging::rpmSpecDeclaresSdkTemplateKeys() {
    QVERIFY(requireReadable(specPath(), "rpm/shanty.spec"));
    const QString spec = readFileOrThrow(specPath());
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("Name:\\s*shanty\\s*$"),
                 QRegularExpression::MultilineOption)),
             "spec must keep Name: shanty");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("Version:\\s*0\\.1\\s*$"),
                 QRegularExpression::MultilineOption)),
             "spec must keep Version: 0.1");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("License:\\s*GPL-3\\.0-or-later"))),
             "spec must declare License: GPL-3.0-or-later (SH-102)");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("Source0:\\s*%\\{name\\}-%\\{version\\}"
                                "\\.tar\\.bz2"))),
             "spec must use the canonical Source0: %{name}-%{version}.tar.bz2");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("^Requires:\\s*sailfishsilica-qt5"),
                 QRegularExpression::MultilineOption)),
             "spec must declare runtime Requires: sailfishsilica-qt5");
    // Runtime Requires must NOT name pkgconfig() packages: those resolve
    // to -devel packages absent from devices/emulators (the emulator
    // install failed exactly on pkgconfig(Qt5Core) etc.). Qt libraries
    // are pulled in automatically via soname dependencies.
    {
        const QRegularExpression runtimePkgconfig(
            QStringLiteral("^Requires:\\s*pkgconfig\\("),
            QRegularExpression::MultilineOption);
        QString what;
        QRegularExpressionMatchIterator it =
            runtimePkgconfig.globalMatch(spec);
        while (it.hasNext())
            what += it.next().captured(0).trimmed() + QStringLiteral("; ");
        QVERIFY2(what.isEmpty(),
                 qPrintable(QStringLiteral(
                                "runtime Requires must not name "
                                "pkgconfig() packages (device "
                                "installability): %1").arg(what)));
    }
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("BuildRequires:\\s*pkgconfig\\(Qt5Core\\)"))),
             "spec must declare BuildRequires: pkgconfig(Qt5Core)");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("BuildRequires:\\s*pkgconfig\\(Qt5Qml\\)"))),
             "spec must declare BuildRequires: pkgconfig(Qt5Qml)");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("BuildRequires:\\s*pkgconfig\\(Qt5Quick\\)"))),
             "spec must declare BuildRequires: pkgconfig(Qt5Quick)");
}

// Case 2 — release bookkeeping: the spec must carry an RPM %changelog
// block whose newest entry documents this release (version-release
// 0.1-1, matching Name/Version above) with the GPL-3.0 notice so the
// shipped RPM per-package changelog states the licence. Phase A red
// reason: template spec ships no %changelog at all.
void TestPackaging::rpmSpecChangelogDocumentsInitialRelease() {
    QVERIFY(requireReadable(specPath(), "rpm/shanty.spec"));
    const QString spec = readFileOrThrow(specPath());
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("^%changelog\\s*$"),
                 QRegularExpression::MultilineOption)),
             "rpm/shanty.spec must have a %changelog section");
    const QString changelog = specSection(spec, QStringLiteral("%changelog"));
    QVERIFY2(!changelog.isEmpty(),
             "%changelog section must contain at least one entry");
    QVERIFY2(changelog.contains(QRegularExpression(
                 QStringLiteral("0\\.1-1\\b"))),
             "%changelog newest entry must document 0.1-1");
    QVERIFY2(changelog.contains(QRegularExpression(
                 QStringLiteral("GPL-3\\.0"))),
             "%changelog entry must carry the GPL-3.0 notice");
    // The SDK changelog template (rpm/shanty.changes.in) must exist; the
    // UUID-style changes.run.in hook stays as shipped so sfdk changelog
    // generation keeps working.
    QVERIFY(requireReadable(QStringLiteral(SHANTY_ROOT) +
                                QStringLiteral("/rpm/shanty.changes.in"),
                            "rpm/shanty.changes.in"));
    QVERIFY(requireReadable(QStringLiteral(SHANTY_ROOT) +
                                QStringLiteral("/rpm/shanty.changes.run.in"),
                            "rpm/shanty.changes.run.in"));
}

// Case 3 — %files must cover the install set that the qmake rules in
// app/app.pro CONFIG (sailfishapp, sailfishapp_i18n) actually produce.
// Derived from the template install paths:
//   - %{_bindir}/%{name}                     (TARGET binary)
//   - %{_datadir}/applications/%{name}.desktop (desktop file install)
//   - %{_datadir}/icons/hicolor/*/apps/%{name}.png (SAILFISHAPP_ICONS set)
//   - %{_datadir}/%{name}                    (qml/ + sailfishapp_i18n .qm)
// Cross-checked (not guessed): desktop Icon=/Exec= must reference the app
// .pro TARGET so the desktop entry, binary name and icon all agree.
void TestPackaging::rpmSpecFilesCoverInstallSetFromAppTemplate() {
    QVERIFY(requireReadable(specPath(), "rpm/shanty.spec"));
    QVERIFY(requireReadable(appProPath(), "app/app.pro"));
    const QString target = appTarget();
    QVERIFY(!target.isEmpty());
    const QString desktop = readFileOrThrow(
        QStringLiteral(SHANTY_ROOT) + QStringLiteral("/app/shanty.desktop"));
    QVERIFY2(desktop.contains(QRegularExpression(
                 QStringLiteral("Exec=%1\\s*$").arg(target),
                 QRegularExpression::MultilineOption)),
             "shanty.desktop Exec must equal app.pro TARGET");
    QVERIFY2(desktop.contains(QRegularExpression(
                 QStringLiteral("Icon=%1\\s*$").arg(target),
                 QRegularExpression::MultilineOption)),
             "shanty.desktop Icon must equal app.pro TARGET");

    const QString files = specSection(readFileOrThrow(specPath()),
                                      QStringLiteral("%files"));
    QVERIFY2(!files.isEmpty(),
             "rpm/shanty.spec must have a populated %files section");
    QVERIFY2(files.contains(QStringLiteral("%{_bindir}/%{name}")),
             "%files must cover the installed binary %{_bindir}/%{name}");
    QVERIFY2(files.contains(
                 QStringLiteral("%{_datadir}/applications/%{name}.desktop")),
             "%files must cover the desktop file");
    QVERIFY2(files.contains(QStringLiteral("icons/hicolor")) &&
                 files.contains(QStringLiteral("apps/%{name}.png")),
             "%files must cover icons/hicolor/*/apps/%{name}.png");
    QVERIFY2(files.contains(QStringLiteral("%{_datadir}/%{name}")),
             "%files must cover %{_datadir}/%{name} (qml + .qm payload)");
}

// Case 4 — every icon size the app template declares must exist as an
// actual file. app.pro declares SAILFISHAPP_ICONS = 86x86 108x108
// 128x128 172x172 and the template ships placeholder PNGs; %files uses
// the wildcard, so a missing size file only breaks the RPM at SDK build
// time — this case catches it on the host.
void TestPackaging::declaredIconSizesShipPngFiles() {
    QVERIFY(requireReadable(appProPath(), "app/app.pro"));
    const QString target = appTarget();
    QVERIFY(!target.isEmpty());
    const QString appPro = readFileOrThrow(appProPath());
    QRegularExpression re(
        QStringLiteral("SAILFISHAPP_ICONS\\s*=\\s*([^\n]*)"));
    const QRegularExpressionMatch match = re.match(appPro);
    QVERIFY2(match.hasMatch(),
             "app/app.pro must declare SAILFISHAPP_ICONS");
    const QStringList sizes =
        match.captured(1).split(QChar::fromLatin1(' '),
                                Qt::SkipEmptyParts);
    QVERIFY2(sizes.count() >= 3,
             "expected at least 3 declared icon sizes");
    for (const QString& size : sizes) {
        const QString path = appPath() +
                             QStringLiteral("/icons/%1/%2.png").arg(size,
                                                                    target);
        QVERIFY2(QFileInfo::exists(path),
                 qPrintable(QStringLiteral(
                                "declared icon size missing on disk: %1")
                                .arg(path)));
    }
    // The hicolor wildcard in %files is only valid while the sizes stay
    // in the icons/ tree layout above.
    QVERIFY2(QDir(appPath() + QStringLiteral("/icons"))
                 .entryList(QDir::Dirs | QDir::NoDotAndDotDot)
                 .count() >= sizes.count(),
             "app/icons/ tree holds fewer size dirs than SAILFISHAPP_ICONS");
}

// Case 5 — every QML/translation/packaging file the app template lists in
// DISTFILES (and each TRANSLATIONS source) must exist on disk. The RPM
// source tarball is assembled from the repo tree; a DISTFILES entry that
// 404s silently shrinks the shipped payload. Expansions `../translations/
// *.ts` are checked as "directory contains ≥ 1 match".
void TestPackaging::distfilesAndTranslationSourcesExist() {
    QVERIFY(requireReadable(appProPath(), "app/app.pro"));
    QStringList checked;
    const QStringList tokens =
        qmakeVariableTokens(readFileOrThrow(appProPath()),
                            QStringLiteral("DISTFILES"));
    for (const QString& token : tokens) {
        // qmake resolves each DISTFILES token relative to app/; "../"
        // entries climb to the repo root.
        const QString absDir = token.startsWith(QStringLiteral("../"))
            ? QStringLiteral(SHANTY_ROOT)
            : appPath();
        const QString relative = token.startsWith(QStringLiteral("../"))
            ? token.mid(2)
            : token;
        if (relative.contains(QChar::fromLatin1('*'))) {
            const QString dir =
                relative.section(QChar::fromLatin1('/'), 0, -2);
            const QString pattern = relative.section(QChar::fromLatin1('/'),
                                                     -1);
            QVERIFY2(!QDir(absDir + QChar::fromLatin1('/') + dir)
                          .entryList(QStringList{pattern}, QDir::Files)
                          .isEmpty(),
                     qPrintable(QStringLiteral(
                                     "DISTFILES expansion empty: %1")
                                     .arg(token)));
        } else {
            QVERIFY2(QFileInfo::exists(absDir + QChar::fromLatin1('/') +
                                       relative),
                     qPrintable(QStringLiteral(
                                    "DISTFILES path missing on disk: %1")
                                     .arg(token)));
        }
        checked << relative;
    }
    QVERIFY2(checked.count() >= 5,
             qPrintable(QStringLiteral(
                            "expected ≥5 DISTFILES paths, parsed %1")
                            .arg(checked.count())));

    for (const QString& token :
         qmakeVariableTokens(readFileOrThrow(appProPath()),
                             QStringLiteral("TRANSLATIONS"))) {
        if (!token.startsWith(QStringLiteral("../"))) continue;
        QVERIFY2(QFileInfo::exists(QStringLiteral(SHANTY_ROOT) +
                                   QChar::fromLatin1('/') + token.mid(2)),
                 qPrintable(QStringLiteral(
                                "TRANSLATIONS source missing: %1").arg(token)));
    }
}

// Case 6 — RPM-embedded translations vs shipped QML strings
// (SOFT-SKIP decision, recorded in ISSUES.md ticket-comment style):
//
//   Decision SH-702 Phase A: the template ships translations/shanty.ts
//   (base catalogue) and translations/shanty-de.ts, both generated from
//   the template QML. SH-E6 Phase B (Silica UI) will replace the template
//   pages, so the catalogue will be regenerated with lupdate only after
//   the final QML strings exist. Enforcing .ts↔QML parity NOW would make
//   every E6 page change red with no route to green (the old .ts entries
//   are simply not maintained). Therefore:
//     - HARD contract: translations/shanty.ts must exist and parse (the
//       template ships it; deleting it would silently break i18n, and
//       app.pro CONFIG += sailfishapp_i18n expects it).
//     - SOFT contract: every qsTr("…") string in qml/** must appear as a
//       <source> in the catalogue. Mismatches are REPORTED (qWarning),
//       not failed, until translations are finalised post E6.
//   Upgrade path: when SH-E6 lands, tighten the qWarning to QFAIL and
//   regenerate the catalogue in the same change.
void TestPackaging::qmlUserVisibleStringsHaveCatalogueEntries() {
    const QString cataloguePath = QStringLiteral(SHANTY_ROOT) +
                                  QStringLiteral("/translations/shanty.ts");
    if (!QFileInfo::exists(cataloguePath))
        QSKIP("no translations/shanty.ts shipped — .ts-vs-QML string "
              "parity deferred until translations exist (SH-702 decision)");
    QVERIFY(requireReadable(cataloguePath, "translations/shanty.ts"));
    QVERIFY(requireReadable(appProPath(), "app/app.pro"));
    const QSet<QString> catalogue = tsSourceStrings(
        readFileOrThrow(cataloguePath));

    QSet<QString> qmlStrings;
    int qmlFilesChecked = 0;
    QDirIterator it(
        appPath() + QStringLiteral("/qml"),
        QStringList{QStringLiteral("*.qml")}, QDir::Files,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QVERIFY(requireReadable(it.next(), "qml file"));
        qmlStrings.unite(qsTrStringsIn(readFileOrThrow(it.filePath())));
        ++qmlFilesChecked;
    }
    QVERIFY2(qmlFilesChecked > 0, "expected to scan qml/ files");

    // Soft parity: report missing catalogue entries without failing.
    const QSet<QString> missing = qmlStrings - catalogue;
    for (const QString& s : missing)
        QTest::qWarn(qPrintable(QStringLiteral(
            "[soft] qsTr string has no shanty.ts entry (parity enforced "
            "once translations are finalised, see decision above): '%1'")
            .arg(s)));
}

// ---- SH-901 (SH-E9): the gst engine's dependency contract --------------------
// The device engine moves to GStreamer (playbin + audio-filter=
// equalizer-10bands; docs/eq-investigation.md). Consequences the spec
// must encode:
//   - runtime Requires: gstreamer1.0-plugins-good — the package that owns
//     libgstequalizer.so (the equalizer element; verified on the
//     emulator). Explicit, not "hope it's on the image": the app's core
//     feature now contractually depends on the element. (libgstreamer's
//     own soname dependency is automatic once we link it.)
//   - BuildRequires: pkgconfig(gstreamer-1.0) — gst core headers/libs are
//     ALL the compile surface we need (elements are created by factory
//     name at runtime; no plugins-base API is called directly).
//   - Runtime Requires still must NOT use pkgconfig() (the standing
//     rule from the failed emulator install).

void TestPackaging::rpmSpecDeclaresEqualizerDependencies() {
    QVERIFY(requireReadable(specPath(), "rpm/shanty.spec"));
    const QString spec = readFileOrThrow(specPath());

    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("^Requires:\\s*gstreamer1\\.0-plugins-good"),
                 QRegularExpression::MultilineOption)),
             "runtime Requires must declare gstreamer1.0-plugins-good "
             "(the equalizer element's owner, SH-901)");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("^BuildRequires:\\s*pkgconfig\\("
                                "gstreamer-1\\.0\\)"),
                 QRegularExpression::MultilineOption)),
             "BuildRequires must declare pkgconfig(gstreamer-1.0) "
             "(the gst core compile surface, SH-901)");
}

// ---- SH-1001 (SH-E10 UX-12): the tag reader's dependency contract -----------
// The metadata capability moves to taglib (SH-1001, UX.md §1 owner
// direction). Consequences the spec must encode:
//   - runtime Requires: taglib — the runtime lib ships on the images
//     (taglib 2.0.2 verified on the emulator), but the app now
//     contractually links it: declare the plain package name explicitly.
//     (pkgconfig() in runtime Requires stays banned — standing rule from
//     the failed emulator install.)
//   - BuildRequires: pkgconfig(taglib) — the compile surface (headers
//     + -ltag via pkg-config; taglib-devel in the SDK target).

void TestPackaging::rpmSpecDeclaresTaglibDependencies() {
    QVERIFY(requireReadable(specPath(), "rpm/shanty.spec"));
    const QString spec = readFileOrThrow(specPath());

    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("^Requires:\\s*taglib\\b"),
                 QRegularExpression::MultilineOption)),
             "runtime Requires must declare taglib (the metadata "
             "reader's runtime lib, SH-1001)");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("^BuildRequires:\\s*pkgconfig\\(taglib\\)"),
                 QRegularExpression::MultilineOption)),
             "BuildRequires must declare pkgconfig(taglib) "
             "(the tag reader compile surface, SH-1001)");
}

// ---- SH-1008 (UX-19): library config in the system Settings app ------------
// Phase 0 (verified on the emulator 2026-11-18) fixed the mechanism: a JSON
// file in /usr/share/jolla-settings/entries/ with "type": "application",
// "path": "applications/<desktop-file-name>.desktop" (the .desktop suffix is
// REQUIRED — without it the tile silently opens the generic permissions
// page) and params.source = a QML page whose root is com.jolla.settings'
// ApplicationSettings. The app's view models reach that page through a QML
// plugin module (Shanty.Settings), because the page runs in the
// jolla-settings engine, not the app's.
//
// The four cases below pin the packaging contract; the runtime contract
// (the page writes the same ~/.config/shanty/library.conf the app reads) is
// device-verified by the ticket's acceptance drive.

void TestPackaging::settingsEntryBindsTheSystemSettingsPageToTheDesktopFile() {
    const QString target = appTarget();
    QVERIFY(!target.isEmpty());

    // Source location (installed by app.pro's INSTALLS rule under the
    // literal name the Settings tree looks for: <name>.json).
    const QString jsonPath = QStringLiteral(SHANTY_ROOT) +
                             QStringLiteral("/app/jolla-settings/shanty.json");
    QVERIFY2(requireReadable(jsonPath, "the jolla-settings entry JSON"),
             "SH-1008: app/jolla-settings/shanty.json must exist");

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(readFileOrThrow(jsonPath).toUtf8(),
                                &parseError);
    QVERIFY2(parseError.error == QJsonParseError::NoError,
             qPrintable(QStringLiteral("entries JSON does not parse: %1")
                            .arg(parseError.errorString())));
    QVERIFY2(document.isObject(), "entries JSON must be an object");

    const QJsonArray entries = document.object().value(QStringLiteral("entries"))
                                   .toArray();
    QVERIFY2(!entries.isEmpty(),
             "entries JSON must carry at least one entry");

    QJsonObject applicationEntry;
    bool foundApplication = false;
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("type")).toString() ==
            QStringLiteral("application")) {
            applicationEntry = entry;
            foundApplication = true;
            break;
        }
    }
    QVERIFY2(foundApplication,
             "entries JSON must carry a \"type\": \"application\" entry");

    // Phase 0 binding rule: the FULL desktop-file name (with .desktop),
    // otherwise the Settings app falls back to the generic page.
    QCOMPARE(applicationEntry.value(QStringLiteral("path")).toString(),
             QStringLiteral("applications/%1.desktop").arg(target));

    const QString source =
        applicationEntry.value(QStringLiteral("params")).toObject()
            .value(QStringLiteral("source")).toString();
    const QString installedPrefix =
        QStringLiteral("/usr/share/%1/qml/").arg(target);
    QVERIFY2(source.startsWith(installedPrefix),
             qPrintable(QStringLiteral(
                            "params.source must be the installed app QML path "
                            "(under %1), got '%2'")
                            .arg(installedPrefix, source)));

    // The packaged page must exist in the repo (app/qml/...) so the
    // sailfishapp install actually ships it.
    const QString pagePath = appPath() + QStringLiteral("/qml/") +
                             source.mid(installedPrefix.size());
    QVERIFY2(QFileInfo::exists(pagePath),
             qPrintable(QStringLiteral(
                            "params.source has no source file on disk: %1")
                            .arg(pagePath)));
    const QString pageQml = readFileOrThrow(pagePath);
    QVERIFY2(pageQml.contains(QRegularExpression(
                 QStringLiteral("^import com\\.jolla\\.settings 1\\.0\\s*$"),
                 QRegularExpression::MultilineOption)),
             "the Settings page must import the com.jolla.settings module");
    QVERIFY2(pageQml.contains(QRegularExpression(
                 QStringLiteral("^ApplicationSettings\\s*\\{"),
                 QRegularExpression::MultilineOption)),
             "the Settings page's root must be ApplicationSettings");

    // app.pro owns the install rule (qmake INSTALLS → the Settings tree).
    const QString appPro = readFileOrThrow(appProPath());
    QVERIFY2(appPro.contains(
                 QStringLiteral("$$[QT_INSTALL_DATA]/jolla-settings/entries")),
             "app/app.pro must install the entry into "
             "$$[QT_INSTALL_DATA]/jolla-settings/entries");
    QVERIFY2(appPro.contains(QStringLiteral("jolla-settings/shanty.json")),
             "app/app.pro must install app/jolla-settings/shanty.json");
}

void TestPackaging::settingsPluginProjectDeclaresTheQmlModule() {
    const QString pluginDir = appPath() +
                              QStringLiteral("/settings-plugin");
    QVERIFY2(QFileInfo(pluginDir).isDir(),
             "SH-1008: app/settings-plugin/ must exist");

    const QString projectPath =
        pluginDir + QStringLiteral("/settings-plugin.pro");
    QVERIFY(requireReadable(projectPath, "the settings plugin project"));
    const QString project = readFileOrThrow(projectPath);
    QVERIFY2(project.contains(QRegularExpression(
                 QStringLiteral("^TEMPLATE\\s*=\\s*lib\\s*$"),
                 QRegularExpression::MultilineOption)),
             "settings-plugin.pro must be TEMPLATE = lib");
    QVERIFY2(project.contains(QRegularExpression(
                 QStringLiteral("CONFIG\\s*\\+=\\s*[^\\n]*\\bplugin\\b"))),
             "settings-plugin.pro must declare CONFIG += plugin");
    QVERIFY2(project.contains(QRegularExpression(
                 QStringLiteral("^TARGET\\s*=\\s*shantysettings\\s*$"),
                 QRegularExpression::MultilineOption)),
             "settings-plugin.pro must build TARGET = shantysettings");
    QVERIFY2(project.contains(
                 QStringLiteral("$$[QT_INSTALL_QML]/Shanty/Settings")),
             "settings-plugin.pro must install under "
             "$$[QT_INSTALL_QML]/Shanty/Settings");

    const QString qmldirPath = pluginDir + QStringLiteral("/qmldir");
    QVERIFY(requireReadable(qmldirPath, "the settings plugin qmldir"));
    const QString qmldir = readFileOrThrow(qmldirPath);
    QVERIFY2(qmldir.contains(QRegularExpression(
                 QStringLiteral("^module Shanty\\.Settings\\s*$"),
                 QRegularExpression::MultilineOption)),
             "qmldir must declare 'module Shanty.Settings'");
    QVERIFY2(qmldir.contains(QRegularExpression(
                 QStringLiteral("^plugin shantysettings\\s*$"),
                 QRegularExpression::MultilineOption)),
             "qmldir must name the plugin binary (plugin shantysettings)");

    // registerTypes() is where a QQmlExtensionPlugin declares its module.
    bool registersModule = false;
    QDirIterator it(pluginDir, QStringList{QStringLiteral("*.cpp")},
                    QDir::Files);
    while (it.hasNext()) {
        const QString source = readFileOrThrow(it.next());
        if (source.contains(QStringLiteral("qmlRegisterSingletonType")) &&
            source.contains(QStringLiteral("\"Shanty.Settings\""))) {
            registersModule = true;
            break;
        }
    }
    QVERIFY2(registersModule,
             "the plugin must register its singleton under \"Shanty.Settings\"");

    // The top-level project must build the plugin on the device.
    const QString topLevel =
        readFileOrThrow(QStringLiteral(SHANTY_ROOT) +
                        QStringLiteral("/shanty.pro"));
    QVERIFY2(topLevel.contains(QStringLiteral("app/settings-plugin")),
             "shanty.pro must build app/settings-plugin in the device branch");
}

void TestPackaging::settingsPageManagesTheLibraryConfigThroughThePlugin() {
    const QString pagePath =
        appPath() + QStringLiteral("/qml/pages/SettingsEntryPage.qml");
    QVERIFY(requireReadable(pagePath, "the system Settings page"));
    const QString page = readFileOrThrow(pagePath);

    QVERIFY2(page.contains(QStringLiteral("import Shanty.Settings 1.0")),
             "the Settings page must import the Shanty.Settings plugin");
    // The page binds the plugin singleton's library surface — the same
    // commands the in-app page used to expose (SH-1004 UI, relocated).
    const QStringList members{
        QStringLiteral("libraryFolders"),
        QStringLiteral("libraryWhitelist"),
        QStringLiteral("libraryBlacklist"),
        QStringLiteral("addFolder"),
        QStringLiteral("removeFolder"),
        QStringLiteral("addWhitelistEntry"),
        QStringLiteral("removeWhitelistEntry"),
        QStringLiteral("addBlacklistEntry"),
        QStringLiteral("removeBlacklistEntry"),
        QStringLiteral("listDirectory"),
        QStringLiteral("homeDir"),
    };
    for (const QString& member : members) {
        QVERIFY2(page.contains(QStringLiteral("ShantySettings.") + member),
                 qPrintable(QStringLiteral(
                                "the Settings page must use "
                                "ShantySettings.%1").arg(member)));
    }
    // Shared, facade-agnostic pickers live in the same QML directory.
    QVERIFY2(page.contains(QStringLiteral("FilePickerPage.qml")),
             "the Settings page must reuse FilePickerPage.qml");
    QVERIFY2(page.contains(QStringLiteral("PromptPage.qml")),
             "the Settings page must reuse PromptPage.qml");

    // The app's own Settings page keeps the equalizer only: no library
    // management (owner direction SH-1008).
    const QString inAppPage =
        readFileOrThrow(appPath() + QStringLiteral("/qml/pages/SettingsPage.qml"));
    const QStringList removed{
        QStringLiteral("libraryFolders"),
        QStringLiteral("libraryWhitelist"),
        QStringLiteral("libraryBlacklist"),
        QStringLiteral("addFolder"),
        QStringLiteral("removeFolder"),
        QStringLiteral("addWhitelistEntry"),
        QStringLiteral("removeWhitelistEntry"),
        QStringLiteral("addBlacklistEntry"),
        QStringLiteral("removeBlacklistEntry"),
        QStringLiteral("pushFolderPicker"),
        QStringLiteral("pushFilterPrompt"),
    };
    for (const QString& member : removed) {
        QVERIFY2(!inAppPage.contains(member),
                 qPrintable(QStringLiteral(
                                "SettingsPage.qml must no longer reference "
                                "%1 (library config moved to the system "
                                "Settings app)").arg(member)));
    }
    QVERIFY2(inAppPage.contains(QStringLiteral("settings.profiles")),
             "SettingsPage.qml must keep the equalizer profile UI");
}

void TestPackaging::specFilesShipTheSettingsEntryAndTheQmlPlugin() {
    QVERIFY(requireReadable(specPath(), "rpm/shanty.spec"));
    const QString spec = readFileOrThrow(specPath());
    const QString files = specSection(spec, QStringLiteral("%files"));
    QVERIFY2(!files.isEmpty(), "rpm/shanty.spec must have a %files section");
    QVERIFY2(files.contains(QStringLiteral(
                 "%{_datadir}/jolla-settings/entries/%{name}.json")),
             "%files must ship the jolla-settings entry JSON");
    QVERIFY2(files.contains(
                 QStringLiteral("%{_libdir}/qt5/qml/Shanty/Settings")),
             "%files must ship the Shanty.Settings QML plugin");
    QVERIFY2(spec.contains(QRegularExpression(
                 QStringLiteral("^Requires:\\s*jolla-settings\\b"),
                 QRegularExpression::MultilineOption)),
             "runtime Requires must declare jolla-settings (the owner of "
             "the entries tree the packaged page loads into)");
}

QTEST_MAIN(TestPackaging)
#include "tst_packaging.moc"
