// SH-102 Phase A: license hygiene test. Fails while files lack SPDX headers
// or the RPM spec lacks the GPL-3.0 license tag.
// Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSet>
#include <QString>

namespace {

bool hasSpdxHeader(const QString& absolutePath) {
    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTest::qFail(qPrintable(QStringLiteral("unreadable source file: %1")
                                    .arg(absolutePath)),
                     __FILE__, __LINE__);
        return false;
    }
    for (int i = 0; i < 10 && !file.atEnd(); ++i) {
        const QString line = QString::fromUtf8(file.readLine());
        if (line.contains("SPDX-License-Identifier: GPL-3.0-or-later")) return true;
    }
    return false;
}

}  // namespace

class TestLicense : public QObject {
    Q_OBJECT

private slots:
    void allCppAndHeaderFilesCarrySpdxHeader();
    void rpmSpecDeclaresGpl3();
};

void TestLicense::allCppAndHeaderFilesCarrySpdxHeader() {
    const QSet<QString> rootBuildTrees = {QStringLiteral("build-host")};
    QDirIterator it(QStringLiteral(SHANTY_ROOT),
                    QStringList{QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                    QDir::Files,
                    QDirIterator::Subdirectories);

    int checked = 0;
    while (it.hasNext()) {
        const QString path = it.next();
        // Skip generated build trees: everything under a root-level
        // build-* directory is produced, not authored.
        const QString relative =
            QDir(QStringLiteral(SHANTY_ROOT)).relativeFilePath(path);
        const QString top = relative.section(QChar::fromLatin1('/'), 0, 0);
        if (top.startsWith(QStringLiteral("build-")) ||
            rootBuildTrees.contains(relative)) {
            continue;
        }
        QVERIFY2(hasSpdxHeader(path),
                 qPrintable(QStringLiteral("missing SPDX GPL-3.0 header: %1").arg(relative)));
        ++checked;
    }
    QVERIFY2(checked > 10, "expected to check more than one file");
}

void TestLicense::rpmSpecDeclaresGpl3() {
    QFile spec(QStringLiteral(SHANTY_ROOT) + QStringLiteral("/rpm/shanty.spec"));
    QVERIFY2(spec.open(QIODevice::ReadOnly | QIODevice::Text),
             "rpm/shanty.spec must exist");
    const QString content = QString::fromUtf8(spec.readAll());
    QVERIFY2(content.contains(QRegularExpression(QStringLiteral("License:\\s*GPL-3\\.0"))),
             "rpm/shanty.spec must declare License: GPL-3.0");
}

QTEST_MAIN(TestLicense)
#include "tst_license.moc"
