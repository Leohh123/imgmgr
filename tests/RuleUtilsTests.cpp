#include "services/RuleJsonService.h"
#include "services/RuleValidationService.h"
#include "utils/RuleExplanationBuilder.h"
#include "utils/RuleUtils.h"

#include <QtTest/QtTest>

class RuleUtilsTests : public QObject {
    Q_OBJECT

private slots:
    void globMatchingNormalizesPathSeparators();
    void regexWholeMatchHonorsCaseSensitivity();
    void ancestorRulesDoNotConflictWithChildren();
    void exportImportPreservesRuleTreeParents();
    void importRejectsDuplicateRuleIds();
    void explanationShowsNoConflictForSingleMatch();
    void explanationShowsSiblingConflictAdvice();
    void ruleValidationRejectsEmptyNameAndPattern();
    void ruleValidationRejectsInvalidRegex();
    void filterValidationOnlyChecksNonEmptyRegex();
};

void RuleUtilsTests::globMatchingNormalizesPathSeparators()
{
    QVERIFY(RuleUtils::targetMatches(
        QStringLiteral("textures\\wood\\oak.png"),
        QStringLiteral("textures/wood/*.png"),
        RuleUtils::globRuleType(),
        false,
        true));
}

void RuleUtilsTests::regexWholeMatchHonorsCaseSensitivity()
{
    QVERIFY(RuleUtils::targetMatches(
        QStringLiteral("Hero_01"),
        QStringLiteral("hero_\\d+"),
        RuleUtils::regexRuleType(),
        false,
        true));

    QVERIFY(!RuleUtils::targetMatches(
        QStringLiteral("Hero_01"),
        QStringLiteral("hero_\\d+"),
        RuleUtils::regexRuleType(),
        true,
        true));
}

void RuleUtilsTests::ancestorRulesDoNotConflictWithChildren()
{
    RuleRecord parent;
    parent.id = 1;
    parent.name = QStringLiteral("角色");

    RuleRecord child;
    child.id = 2;
    child.parentId = 1;
    child.name = QStringLiteral("主角");

    QHash<int, RuleRecord> rulesById;
    rulesById.insert(parent.id, parent);
    rulesById.insert(child.id, child);

    QHash<int, int> parentById;
    parentById.insert(parent.id, parent.parentId);
    parentById.insert(child.id, child.parentId);

    QVERIFY(RuleUtils::isAncestorRule(parentById, parent.id, child.id));
    QVERIFY(!RuleUtils::isConflictBetweenRules(rulesById, parentById, parent.id, child.id));
}

void RuleUtilsTests::exportImportPreservesRuleTreeParents()
{
    RuleRecord parent;
    parent.id = 1;
    parent.name = QStringLiteral("角色");
    parent.pattern = QStringLiteral("characters/*");
    parent.matchTarget = RuleUtils::relativePathTarget();

    RuleRecord child;
    child.id = 2;
    child.parentId = 1;
    child.name = QStringLiteral("主角");
    child.pattern = QStringLiteral("hero_*");
    child.matchTarget = RuleUtils::fileNameStemTarget();
    child.priority = 10;

    const QJsonDocument document = RuleJsonService::buildExportDocument({ parent, child });

    QVector<RuleRecord> imported;
    QString error;
    QVERIFY2(RuleJsonService::parseImportDocument(document.toJson(), &imported, &error), qPrintable(error));
    QCOMPARE(imported.size(), 2);
    QCOMPARE(imported.at(0).id, parent.id);
    QCOMPARE(imported.at(0).parentId, 0);
    QCOMPARE(imported.at(1).id, child.id);
    QCOMPARE(imported.at(1).parentId, parent.id);
    QCOMPARE(imported.at(1).priority, child.priority);
}

void RuleUtilsTests::importRejectsDuplicateRuleIds()
{
    const QByteArray json = R"([
        {
            "id": 1,
            "name": "A",
            "rule_type": "glob",
            "pattern": "a*",
            "match_target": "filename_stem"
        },
        {
            "id": 1,
            "name": "B",
            "rule_type": "glob",
            "pattern": "b*",
            "match_target": "filename_stem"
        }
    ])";

    QVector<RuleRecord> imported;
    QString error;
    QVERIFY(!RuleJsonService::parseImportDocument(json, &imported, &error));
    QVERIFY(error.contains(QStringLiteral("重复")));
}

void RuleUtilsTests::explanationShowsNoConflictForSingleMatch()
{
    ImageRecord image;
    image.relativePath = QStringLiteral("characters/hero.png");
    image.status = ImageStatus::Classified;

    RuleRecord rule;
    rule.id = 1;
    rule.name = QStringLiteral("角色");
    rule.pattern = QStringLiteral("characters/*");
    rule.matchTarget = RuleUtils::relativePathTarget();

    const QString explanation = RuleExplanationBuilder::build(image, { rule.id }, { rule });

    QVERIFY(explanation.contains(QStringLiteral("当前图片：")));
    QVERIFY(explanation.contains(QStringLiteral("- 角色  [glob: characters/* | 目标: relative_path]")));
    QVERIFY(explanation.contains(QStringLiteral("命中规则数量不超过 1")));
    QVERIFY(!explanation.contains(QStringLiteral("建议处理方式：")));
}

void RuleUtilsTests::explanationShowsSiblingConflictAdvice()
{
    ImageRecord image;
    image.relativePath = QStringLiteral("characters/hero.png");
    image.status = ImageStatus::Conflict;

    RuleRecord hero;
    hero.id = 1;
    hero.name = QStringLiteral("主角");
    hero.pattern = QStringLiteral("hero*");

    RuleRecord enemy;
    enemy.id = 2;
    enemy.name = QStringLiteral("敌人");
    enemy.pattern = QStringLiteral("enemy*");

    const QString explanation = RuleExplanationBuilder::build(image, { hero.id, enemy.id }, { hero, enemy });

    QVERIFY(explanation.contains(QStringLiteral("存在冲突的规则对：")));
    QVERIFY(explanation.contains(QStringLiteral("主角  <->  敌人")));
    QVERIFY(explanation.contains(QStringLiteral("两个规则不在同一祖先链上")));
    QVERIFY(explanation.contains(QStringLiteral("建议处理方式：")));
}

void RuleUtilsTests::ruleValidationRejectsEmptyNameAndPattern()
{
    RuleValidationService::ValidationError error;

    RuleRecord missingName;
    missingName.pattern = QStringLiteral("hero*");
    QVERIFY(!RuleValidationService::validateRuleForSave(missingName, &error));
    QCOMPARE(error.title, QStringLiteral("规则名称为空"));

    RuleRecord missingPattern;
    missingPattern.name = QStringLiteral("主角");
    QVERIFY(!RuleValidationService::validateRuleForSave(missingPattern, &error));
    QCOMPARE(error.title, QStringLiteral("规则为空"));
}

void RuleUtilsTests::ruleValidationRejectsInvalidRegex()
{
    RuleRecord rule;
    rule.name = QStringLiteral("无效正则");
    rule.ruleType = RuleUtils::regexRuleType();
    rule.pattern = QStringLiteral("(");

    RuleValidationService::ValidationError error;
    QVERIFY(!RuleValidationService::validateRuleForSave(rule, &error));
    QCOMPARE(error.title, QStringLiteral("正则无效"));
    QVERIFY(!error.message.isEmpty());
}

void RuleUtilsTests::filterValidationOnlyChecksNonEmptyRegex()
{
    RuleValidationService::ValidationError error;

    ImageFilter globFilter;
    globFilter.ruleType = RuleUtils::globRuleType();
    globFilter.pattern = QStringLiteral("(");
    QVERIFY(RuleValidationService::validateFilterPattern(globFilter, &error));

    ImageFilter emptyRegexFilter;
    emptyRegexFilter.ruleType = RuleUtils::regexRuleType();
    emptyRegexFilter.pattern = QStringLiteral("   ");
    QVERIFY(RuleValidationService::validateFilterPattern(emptyRegexFilter, &error));

    ImageFilter invalidRegexFilter;
    invalidRegexFilter.ruleType = RuleUtils::regexRuleType();
    invalidRegexFilter.pattern = QStringLiteral("(");
    QVERIFY(!RuleValidationService::validateFilterPattern(invalidRegexFilter, &error));
    QCOMPARE(error.title, QStringLiteral("正则无效"));
}

QTEST_APPLESS_MAIN(RuleUtilsTests)

#include "RuleUtilsTests.moc"
