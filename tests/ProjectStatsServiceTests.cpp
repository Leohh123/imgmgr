#include "database/DatabaseManager.h"
#include "database/ImageRepository.h"
#include "database/RuleRepository.h"
#include "services/ProjectStatsService.h"
#include "services/RuleEngine.h"
#include "TestDbUtils.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class ProjectStatsServiceTests : public QObject {
    Q_OBJECT

private slots:
    void statsTextSummarizesImageStatusesRulesAndTransparency();
};

using TestDbUtils::makeImage;
using TestDbUtils::makeRule;
using TestDbUtils::temporaryDatabasePath;

void ProjectStatsServiceTests::statsTextSummarizesImageStatusesRulesAndTransparency()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("project_stats_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    ImageRepository images(manager.db());
    QVERIFY2(images.upsertImages({
        makeImage(QStringLiteral("hero"), true),
        makeImage(QStringLiteral("bonus")),
        makeImage(QStringLiteral("conflict")),
        makeImage(QStringLiteral("unused"))
    }), qPrintable(images.lastError()));

    RuleRepository rules(manager.db());
    QVERIFY(rules.addRule(makeRule(QStringLiteral("主角"), QStringLiteral("hero"))) > 0);

    RuleRecord bonusA = makeRule(QStringLiteral("奖励A"), QStringLiteral("bonus"));
    bonusA.allowConflict = true;
    QVERIFY(rules.addRule(bonusA) > 0);
    QVERIFY(rules.addRule(makeRule(QStringLiteral("奖励B"), QStringLiteral("bonus"))) > 0);

    QVERIFY(rules.addRule(makeRule(QStringLiteral("冲突A"), QStringLiteral("conflict"))) > 0);
    QVERIFY(rules.addRule(makeRule(QStringLiteral("冲突B"), QStringLiteral("conflict"))) > 0);

    RuleRecord disabled = makeRule(QStringLiteral("禁用"), QStringLiteral("unused"));
    disabled.enabled = false;
    QVERIFY(rules.addRule(disabled) > 0);

    RuleEngine engine(&images, &rules);
    QSignalSpy finishedSpy(&engine, &RuleEngine::finished);
    QSignalSpy failedSpy(&engine, &RuleEngine::failed);
    engine.recalculate();
    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);

    const QString stats = ProjectStatsService::buildStatsText(manager.db(), images);
    QVERIFY(stats.contains(QStringLiteral("总图片数：4")));
    QVERIFY(stats.contains(QStringLiteral("已分类图片数：1")));
    QVERIFY(stats.contains(QStringLiteral("未分类图片数：1")));
    QVERIFY(stats.contains(QStringLiteral("冲突图片数：1")));
    QVERIFY(stats.contains(QStringLiteral("多重命中图片数：1")));
    QVERIFY(stats.contains(QStringLiteral("规则数量：6")));
    QVERIFY(stats.contains(QStringLiteral("启用规则数量：5")));
    QVERIFY(stats.contains(QStringLiteral("禁用规则数量：1")));
    QVERIFY(stats.contains(QStringLiteral("透明图片数量：1")));
    QVERIFY(stats.contains(QStringLiteral("不透明图片数量：3")));
}

QTEST_MAIN(ProjectStatsServiceTests)

#include "ProjectStatsServiceTests.moc"
