#include "database/DatabaseManager.h"
#include "database/ImageRepository.h"
#include "database/RuleRepository.h"
#include "services/RuleEngine.h"
#include "TestDbUtils.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

class RuleEngineTests : public QObject {
    Q_OBJECT

private slots:
    void recalculationStoresMatchesAndImageStatuses();
    void ancestorRulesDoNotConflictAndAllowConflictSuppressesSiblingConflict();
    void childMatchWithoutMatchedAncestorIsAConflict();
};

using TestDbUtils::makeImage;
using TestDbUtils::makeRule;
using TestDbUtils::temporaryDatabasePath;

void RuleEngineTests::recalculationStoresMatchesAndImageStatuses()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("rule_engine_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    ImageRepository images(manager.db());
    QVERIFY2(images.upsertImages({
        makeImage(QStringLiteral("hero")),
        makeImage(QStringLiteral("villain")),
        makeImage(QStringLiteral("unused"))
    }), qPrintable(images.lastError()));

    RuleRepository rules(manager.db());
    const int heroRuleId = rules.addRule(makeRule(QStringLiteral("主角"), QStringLiteral("hero")));
    QVERIFY2(heroRuleId > 0, qPrintable(rules.lastError()));
    const int villainRuleId = rules.addRule(makeRule(QStringLiteral("反派"), QStringLiteral("villain")));
    QVERIFY2(villainRuleId > 0, qPrintable(rules.lastError()));

    RuleEngine engine(&images, &rules);
    QSignalSpy finishedSpy(&engine, &RuleEngine::finished);
    QSignalSpy failedSpy(&engine, &RuleEngine::failed);
    engine.recalculate();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);

    const QVector<ImageRecord> fetched = images.fetchAllImages();
    QCOMPARE(fetched.size(), 3);
    for (const ImageRecord& image : fetched) {
        if (image.fileStem == QStringLiteral("hero")) {
            QCOMPARE(image.status, ImageStatus::Classified);
            QCOMPARE(engine.matchedRulesForImage(image.id), QVector<int>({ heroRuleId }));
        } else if (image.fileStem == QStringLiteral("villain")) {
            QCOMPARE(image.status, ImageStatus::Classified);
            QCOMPARE(engine.matchedRulesForImage(image.id), QVector<int>({ villainRuleId }));
        } else if (image.fileStem == QStringLiteral("unused")) {
            QCOMPARE(image.status, ImageStatus::Unclassified);
            QVERIFY(engine.matchedRulesForImage(image.id).isEmpty());
        }
    }
}

void RuleEngineTests::ancestorRulesDoNotConflictAndAllowConflictSuppressesSiblingConflict()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("rule_engine_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    ImageRepository images(manager.db());
    QVERIFY2(images.upsertImages({
        makeImage(QStringLiteral("hero")),
        makeImage(QStringLiteral("bonus"))
    }), qPrintable(images.lastError()));

    RuleRepository rules(manager.db());
    RuleRecord parent = makeRule(QStringLiteral("主角组"), QStringLiteral("hero"));
    const int parentId = rules.addRule(parent);
    QVERIFY2(parentId > 0, qPrintable(rules.lastError()));

    const int childId = rules.addRule(makeRule(QStringLiteral("主角"), QStringLiteral("hero"), parentId));
    QVERIFY2(childId > 0, qPrintable(rules.lastError()));

    RuleRecord siblingA = makeRule(QStringLiteral("奖励A"), QStringLiteral("bonus"));
    siblingA.allowConflict = true;
    const int siblingAId = rules.addRule(siblingA);
    QVERIFY2(siblingAId > 0, qPrintable(rules.lastError()));

    const int siblingBId = rules.addRule(makeRule(QStringLiteral("奖励B"), QStringLiteral("bonus")));
    QVERIFY2(siblingBId > 0, qPrintable(rules.lastError()));

    RuleEngine engine(&images, &rules);
    QSignalSpy finishedSpy(&engine, &RuleEngine::finished);
    QSignalSpy failedSpy(&engine, &RuleEngine::failed);
    engine.recalculate();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(engine.isAncestorRule(parentId, childId));
    QVERIFY(!engine.isConflictBetweenRules(parentId, childId));
    QVERIFY(!engine.isConflictBetweenRules(siblingAId, siblingBId));

    const QVector<ImageRecord> fetched = images.fetchAllImages();
    for (const ImageRecord& image : fetched) {
        if (image.fileStem == QStringLiteral("hero")) {
            QCOMPARE(image.status, ImageStatus::Classified);
            const QVector<int> matched = engine.matchedRulesForImage(image.id);
            QVERIFY(matched.contains(parentId));
            QVERIFY(matched.contains(childId));
        } else if (image.fileStem == QStringLiteral("bonus")) {
            QCOMPARE(image.status, ImageStatus::MultiMatch);
            const QVector<int> matched = engine.matchedRulesForImage(image.id);
            QVERIFY(matched.contains(siblingAId));
            QVERIFY(matched.contains(siblingBId));
        }
    }
}

void RuleEngineTests::childMatchWithoutMatchedAncestorIsAConflict()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("rule_engine_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    ImageRepository images(manager.db());
    QVERIFY2(images.upsertImages({ makeImage(QStringLiteral("hero")) }), qPrintable(images.lastError()));

    RuleRepository rules(manager.db());
    const int parentId = rules.addRule(makeRule(QStringLiteral("父规则"), QStringLiteral("parent_only")));
    QVERIFY2(parentId > 0, qPrintable(rules.lastError()));
    const int childId = rules.addRule(makeRule(QStringLiteral("子规则"), QStringLiteral("hero"), parentId));
    QVERIFY2(childId > 0, qPrintable(rules.lastError()));

    RuleEngine engine(&images, &rules);
    QSignalSpy finishedSpy(&engine, &RuleEngine::finished);
    QSignalSpy failedSpy(&engine, &RuleEngine::failed);
    engine.recalculate();

    QCOMPARE(failedSpy.count(), 0);
    QCOMPARE(finishedSpy.count(), 1);

    const ImageRecord image = images.fetchAllImages().first();
    QCOMPARE(image.status, ImageStatus::Conflict);
    QCOMPARE(engine.matchedRulesForImage(image.id), QVector<int>({ childId }));
}

QTEST_MAIN(RuleEngineTests)

#include "RuleEngineTests.moc"
