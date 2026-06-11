#include "services/ProjectPathService.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class ProjectPathServiceTests : public QObject {
    Q_OBJECT

private slots:
    void projectInsideResourceDirWouldTouchResourceDir();
    void siblingDirWithSharedPrefixDoesNotTouchResourceDir();
    void missingPathDoesNotTouchResourceDir();
};

void ProjectPathServiceTests::projectInsideResourceDirWouldTouchResourceDir()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString resourceDir = QDir(temp.path()).filePath(QStringLiteral("assets"));
    const QString projectDir = QDir(resourceDir).filePath(QStringLiteral("project"));
    QVERIFY(QDir().mkpath(projectDir));

    QVERIFY(ProjectPathService::projectWritesWouldTouchResourceDir(projectDir, resourceDir));
}

void ProjectPathServiceTests::siblingDirWithSharedPrefixDoesNotTouchResourceDir()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString resourceDir = QDir(temp.path()).filePath(QStringLiteral("assets"));
    const QString projectDir = QDir(temp.path()).filePath(QStringLiteral("assets_db"));
    QVERIFY(QDir().mkpath(resourceDir));
    QVERIFY(QDir().mkpath(projectDir));

    QVERIFY(!ProjectPathService::projectWritesWouldTouchResourceDir(projectDir, resourceDir));
}

void ProjectPathServiceTests::missingPathDoesNotTouchResourceDir()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString resourceDir = QDir(temp.path()).filePath(QStringLiteral("assets"));
    const QString projectDir = QDir(temp.path()).filePath(QStringLiteral("missing_project"));
    QVERIFY(QDir().mkpath(resourceDir));

    QVERIFY(!ProjectPathService::projectWritesWouldTouchResourceDir(projectDir, resourceDir));
}

QTEST_APPLESS_MAIN(ProjectPathServiceTests)

#include "ProjectPathServiceTests.moc"
