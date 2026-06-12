#include "database/DatabaseManager.h"
#include "database/ImageRepository.h"
#include "database/RuleRepository.h"
#include "database/SqlUtils.h"
#include "TestDbUtils.h"
#include "utils/RuleUtils.h"

#include <QSqlQuery>
#include <QtTest/QtTest>

class DatabaseRepositoryTests : public QObject {
    Q_OBJECT

private slots:
    void databaseInitializeCreatesSchemaAndIsIdempotent();
    void imageRepositoryUpsertsFetchesAndUpdatesThumbnail();
    void imageRepositoryFiltersByPatternStatusAndRule();
    void imageRepositoryAppliesLimitAfterPostQueryFilters();
    void imageRepositoryFetchesRuleEvaluationMetadataWithoutMatchAggregation();
    void ruleRepositoryPersistsHierarchyAndRemovesChildrenRecursively();
    void replaceRulesPreservesHierarchyAndRollsBackOnFailure();
};

using TestDbUtils::makeImage;
using TestDbUtils::makeRule;
using TestDbUtils::temporaryDatabasePath;

void DatabaseRepositoryTests::databaseInitializeCreatesSchemaAndIsIdempotent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("database_repository_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));
    QVERIFY2(manager.initialize(), qPrintable(manager.lastError()));

    QSqlQuery query(manager.db());
    QString error;
    QVERIFY2(SqlUtils::exec(&query, QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' AND name IN ('images','rules','image_rule_matches') "
        "ORDER BY name"), &error), qPrintable(error));

    QStringList tables;
    while (query.next())
        tables << query.value(0).toString();
    QCOMPARE(tables, QStringList({ QStringLiteral("image_rule_matches"), QStringLiteral("images"), QStringLiteral("rules") }));
}

void DatabaseRepositoryTests::imageRepositoryUpsertsFetchesAndUpdatesThumbnail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("database_repository_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    ImageRepository images(manager.db());
    const ImageRecord original = makeImage(
        QStringLiteral("D:/assets/characters/hero.png"),
        QStringLiteral("characters/hero.png"),
        QStringLiteral("hero.png"));
    QVERIFY2(images.upsertImages({ original }), qPrintable(images.lastError()));
    QCOMPARE(images.imageCount(), 1);

    QVector<ImageRecord> fetched = images.fetchAllImages();
    QCOMPARE(fetched.size(), 1);
    QCOMPARE(fetched.first().relativePath, original.relativePath);
    QCOMPARE(fetched.first().fileStem, QStringLiteral("hero"));
    QCOMPARE(fetched.first().status, ImageStatus::Unclassified);
    QVERIFY(fetched.first().hasAlpha);

    const int imageId = fetched.first().id;
    QVERIFY2(images.updateThumbnail(imageId, QStringLiteral("thumbs/hero.png"), 128, 128), qPrintable(images.lastError()));
    ImageRecord withThumbnail = images.fetchImage(imageId);
    QVERIFY(withThumbnail.thumbnailReady);
    QCOMPARE(withThumbnail.thumbnailPath, QStringLiteral("thumbs/hero.png"));

    ImageRecord updated = original;
    updated.fileSize = original.fileSize + 1;
    updated.modifiedTime = original.modifiedTime + 1;
    updated.width = 128;
    QVERIFY2(images.upsertImages({ updated }), qPrintable(images.lastError()));
    QCOMPARE(images.imageCount(), 1);

    ImageRecord afterUpdate = images.fetchImage(imageId);
    QCOMPARE(afterUpdate.width, 128);
    QVERIFY(!afterUpdate.thumbnailReady);
}

void DatabaseRepositoryTests::imageRepositoryFiltersByPatternStatusAndRule()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("database_repository_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    ImageRepository images(manager.db());
    const ImageRecord hero = makeImage(
        QStringLiteral("D:/assets/characters/hero.png"),
        QStringLiteral("characters/hero.png"),
        QStringLiteral("hero.png"));
    const ImageRecord enemy = makeImage(
        QStringLiteral("D:/assets/enemies/enemy.png"),
        QStringLiteral("enemies/enemy.png"),
        QStringLiteral("enemy.png"));
    QVERIFY2(images.upsertImages({ hero, enemy }), qPrintable(images.lastError()));

    RuleRepository rules(manager.db());
    const int heroRuleId = rules.addRule(makeRule(QStringLiteral("主角"), QStringLiteral("hero")));
    QVERIFY2(heroRuleId > 0, qPrintable(rules.lastError()));
    const int childRuleId = rules.addRule(makeRule(QStringLiteral("主角头像"), QStringLiteral("hero_avatar"), heroRuleId));
    QVERIFY2(childRuleId > 0, qPrintable(rules.lastError()));

    const QVector<ImageRecord> allImages = images.fetchAllImages();
    const int heroImageId = allImages.first().fileStem == QStringLiteral("hero") ? allImages.first().id : allImages.last().id;
    QSqlQuery insertMatch(manager.db());
    insertMatch.prepare(QStringLiteral(
        "INSERT INTO image_rule_matches (image_id, rule_id, is_conflict, created_at) VALUES (?,?,?,0)"));
    insertMatch.addBindValue(heroImageId);
    insertMatch.addBindValue(heroRuleId);
    insertMatch.addBindValue(0);
    QString error;
    QVERIFY2(SqlUtils::exec(&insertMatch, &error), qPrintable(error));

    ImageFilter relativePathFilter;
    relativePathFilter.pattern = QStringLiteral("characters/*");
    relativePathFilter.matchTarget = RuleUtils::relativePathTarget();
    QVector<ImageRecord> filtered = images.fetchAllImages(relativePathFilter);
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().fileStem, QStringLiteral("hero"));

    ImageFilter classifiedOnly;
    classifiedOnly.onlyUnclassified = false;
    classifiedOnly.onlyConflict = false;
    classifiedOnly.onlyMultiMatch = false;
    filtered = images.fetchAllImages(classifiedOnly);
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().status, ImageStatus::Classified);

    ImageFilter currentRule;
    currentRule.currentRuleId = heroRuleId;
    currentRule.onlyCurrentRule = true;
    filtered = images.fetchAllImages(currentRule);
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().id, heroImageId);

    QSqlQuery insertChildMatch(manager.db());
    insertChildMatch.prepare(QStringLiteral(
        "INSERT INTO image_rule_matches (image_id, rule_id, is_conflict, created_at) VALUES (?,?,?,0)"));
    insertChildMatch.addBindValue(heroImageId);
    insertChildMatch.addBindValue(childRuleId);
    insertChildMatch.addBindValue(0);
    QVERIFY2(SqlUtils::exec(&insertChildMatch, &error), qPrintable(error));
    QVERIFY(images.fetchAllImages(currentRule).isEmpty());
}

void DatabaseRepositoryTests::imageRepositoryAppliesLimitAfterPostQueryFilters()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("database_repository_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    ImageRepository images(manager.db());
    const ImageRecord unclassified = makeImage(
        QStringLiteral("D:/assets/a_unused.png"),
        QStringLiteral("a_unused.png"),
        QStringLiteral("a_unused.png"));
    const ImageRecord classified = makeImage(
        QStringLiteral("D:/assets/z_target_01.png"),
        QStringLiteral("z_target_01.png"),
        QStringLiteral("z_target_01.png"));
    QVERIFY2(images.upsertImages({ unclassified, classified }), qPrintable(images.lastError()));

    const QVector<ImageRecord> allImages = images.fetchAllImages();
    int classifiedImageId = 0;
    for (const ImageRecord& image : allImages) {
        if (image.fileStem == QStringLiteral("z_target_01"))
            classifiedImageId = image.id;
    }
    QVERIFY(classifiedImageId > 0);

    RuleRepository rules(manager.db());
    const int ruleId = rules.addRule(makeRule(QStringLiteral("目标"), QStringLiteral("z_target_01")));
    QVERIFY2(ruleId > 0, qPrintable(rules.lastError()));

    QSqlQuery insertMatch(manager.db());
    insertMatch.prepare(QStringLiteral(
        "INSERT INTO image_rule_matches (image_id, rule_id, is_conflict, created_at) VALUES (?,?,?,0)"));
    insertMatch.addBindValue(classifiedImageId);
    insertMatch.addBindValue(ruleId);
    insertMatch.addBindValue(0);
    QString error;
    QVERIFY2(SqlUtils::exec(&insertMatch, &error), qPrintable(error));

    ImageFilter classifiedOnly;
    classifiedOnly.onlyUnclassified = false;
    classifiedOnly.onlyConflict = false;
    classifiedOnly.onlyMultiMatch = false;
    QVector<ImageRecord> filtered = images.fetchImages(classifiedOnly, 1);
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().fileStem, QStringLiteral("z_target_01"));

    ImageFilter regexFilter;
    regexFilter.ruleType = RuleUtils::regexRuleType();
    regexFilter.pattern = QStringLiteral("z_target_\\d+");
    regexFilter.matchTarget = RuleUtils::fileNameStemTarget();
    filtered = images.fetchImages(regexFilter, 1);
    QCOMPARE(filtered.size(), 1);
    QCOMPARE(filtered.first().fileStem, QStringLiteral("z_target_01"));
}

void DatabaseRepositoryTests::imageRepositoryFetchesRuleEvaluationMetadataWithoutMatchAggregation()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("database_repository_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    ImageRepository images(manager.db());
    const ImageRecord image = makeImage(
        QStringLiteral("D:/assets/characters/hero.png"),
        QStringLiteral("characters/hero.png"),
        QStringLiteral("hero.png"));
    QVERIFY2(images.upsertImages({ image }), qPrintable(images.lastError()));
    const int imageId = images.fetchAllImages().first().id;

    RuleRepository rules(manager.db());
    const int ruleId = rules.addRule(makeRule(QStringLiteral("主角"), QStringLiteral("hero")));
    QVERIFY2(ruleId > 0, qPrintable(rules.lastError()));

    QSqlQuery insertMatch(manager.db());
    insertMatch.prepare(QStringLiteral(
        "INSERT INTO image_rule_matches (image_id, rule_id, is_conflict, created_at) VALUES (?,?,?,0)"));
    insertMatch.addBindValue(imageId);
    insertMatch.addBindValue(ruleId);
    insertMatch.addBindValue(0);
    QString error;
    QVERIFY2(SqlUtils::exec(&insertMatch, &error), qPrintable(error));

    const QVector<ImageRecord> records = images.fetchImagesForRuleEvaluation();
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().id, imageId);
    QCOMPARE(records.first().relativePath, QStringLiteral("characters/hero.png"));
    QCOMPARE(records.first().fileStem, QStringLiteral("hero"));
    QCOMPARE(records.first().matchCount, 0);
    QCOMPARE(records.first().status, ImageStatus::Unclassified);
}

void DatabaseRepositoryTests::ruleRepositoryPersistsHierarchyAndRemovesChildrenRecursively()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("database_repository_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    RuleRepository rules(manager.db());
    const int parentId = rules.addRule(makeRule(QStringLiteral("角色"), QStringLiteral("*")));
    QVERIFY2(parentId > 0, qPrintable(rules.lastError()));
    const int childId = rules.addRule(makeRule(QStringLiteral("主角"), QStringLiteral("hero"), parentId));
    QVERIFY2(childId > 0, qPrintable(rules.lastError()));
    const int grandchildId = rules.addRule(makeRule(QStringLiteral("主角头像"), QStringLiteral("hero_avatar"), childId));
    QVERIFY2(grandchildId > 0, qPrintable(rules.lastError()));

    RuleRecord disabled = makeRule(QStringLiteral("禁用规则"), QStringLiteral("disabled"));
    disabled.enabled = false;
    const int disabledId = rules.addRule(disabled);
    QVERIFY2(disabledId > 0, qPrintable(rules.lastError()));

    QCOMPARE(rules.fetchRule(childId).parentId, parentId);
    QCOMPARE(rules.fetchRules(false).size(), 4);
    QCOMPARE(rules.fetchRules(true).size(), 3);

    const QVector<int> descendants = rules.childRuleIdsRecursive(parentId);
    QVERIFY(descendants.contains(childId));
    QVERIFY(descendants.contains(grandchildId));

    ImageRepository images(manager.db());
    const ImageRecord image = makeImage(
        QStringLiteral("D:/assets/characters/hero.png"),
        QStringLiteral("characters/hero.png"),
        QStringLiteral("hero.png"));
    QVERIFY2(images.upsertImages({ image }), qPrintable(images.lastError()));
    const int imageId = images.fetchAllImages().first().id;

    QSqlQuery insertMatch(manager.db());
    insertMatch.prepare(QStringLiteral(
        "INSERT INTO image_rule_matches (image_id, rule_id, is_conflict, created_at) VALUES (?,?,?,0)"));
    insertMatch.addBindValue(imageId);
    insertMatch.addBindValue(childId);
    insertMatch.addBindValue(0);
    QString error;
    QVERIFY2(SqlUtils::exec(&insertMatch, &error), qPrintable(error));
    QCOMPARE(rules.matchCounts().value(childId), 1);

    QVERIFY2(rules.removeRuleRecursive(childId), qPrintable(rules.lastError()));
    QVERIFY(rules.fetchRule(childId).id == 0);
    QVERIFY(rules.fetchRule(grandchildId).id == 0);
    QVERIFY(rules.fetchRule(parentId).id == parentId);
    QVERIFY(!rules.matchCounts().contains(childId));
}

void DatabaseRepositoryTests::replaceRulesPreservesHierarchyAndRollsBackOnFailure()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    DatabaseManager manager(TestDbUtils::uniqueConnectionName(QStringLiteral("database_repository_test")));
    QVERIFY2(manager.openProject(temporaryDatabasePath(&dir)), qPrintable(manager.lastError()));

    RuleRepository rules(manager.db());
    RuleRecord parent = makeRule(QStringLiteral("角色"), QStringLiteral("*"));
    parent.id = 10;
    RuleRecord child = makeRule(QStringLiteral("主角"), QStringLiteral("hero"), parent.id);
    child.id = 11;
    QVERIFY2(rules.replaceRules({ parent, child }), qPrintable(rules.lastError()));

    QCOMPARE(rules.fetchRules(false).size(), 2);
    QCOMPARE(rules.fetchRule(child.id).parentId, parent.id);

    RuleRecord duplicate = makeRule(QStringLiteral("重复"), QStringLiteral("dup"));
    duplicate.id = parent.id;
    QVERIFY(!rules.replaceRules({ parent, duplicate }));

    const QVector<RuleRecord> afterFailure = rules.fetchRules(false);
    QCOMPARE(afterFailure.size(), 2);
    QCOMPARE(rules.fetchRule(parent.id).name, parent.name);
    QCOMPARE(rules.fetchRule(child.id).parentId, parent.id);
}

QTEST_MAIN(DatabaseRepositoryTests)

#include "DatabaseRepositoryTests.moc"
