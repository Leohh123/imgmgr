#include "services/RuleEngine.h"

#include "database/SqlUtils.h"
#include "utils/RuleGraphUtils.h"
#include "utils/RuleUtils.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>

namespace {
struct CompiledRule {
    RuleRecord rule;
    QRegularExpression expression;

    bool matches(const ImageRecord& image) const
    {
        if (!expression.isValid())
            return false;
        const QString target = RuleUtils::targetForImage(image, rule.matchTarget);
        if (expression.match(target).hasMatch())
            return true;
        const QString normalized = QString(target).replace('\\', '/');
        return normalized != target && expression.match(normalized).hasMatch();
    }
};

struct RuleLookup {
    QHash<int, RuleRecord> byId;
    QHash<int, int> parentById;
};

QVector<CompiledRule> compileRules(const QVector<RuleRecord>& rules)
{
    QVector<CompiledRule> compiled;
    compiled.reserve(rules.size());
    for (const RuleRecord& rule : rules) {
        compiled << CompiledRule {
            rule,
            RuleUtils::buildRegularExpression(rule.pattern.trimmed(), rule.ruleType, rule.caseSensitive, rule.wholeMatch)
        };
    }
    return compiled;
}

RuleLookup buildRuleLookup(const QVector<RuleRecord>& rules)
{
    RuleLookup lookup;
    for (const RuleRecord& rule : rules) {
        lookup.byId.insert(rule.id, rule);
        lookup.parentById.insert(rule.id, rule.parentId);
    }
    return lookup;
}

QVector<int> matchedRuleIds(const ImageRecord& image, const QVector<CompiledRule>& rules)
{
    QVector<int> matched;
    for (const CompiledRule& rule : rules) {
        if (rule.matches(image))
            matched << rule.rule.id;
    }
    return matched;
}

QSet<int> matchedRuleSet(const QVector<int>& matched)
{
    QSet<int> matchedSet;
    for (int id : matched)
        matchedSet.insert(id);
    return matchedSet;
}

void addMissingAncestorConflicts(const QVector<int>& matched, const RuleLookup& lookup, QSet<int>* conflicts)
{
    const QSet<int> matchedSet = matchedRuleSet(matched);
    for (int id : matched) {
        int parent = lookup.parentById.value(id, 0);
        while (parent != 0) {
            if (lookup.byId.contains(parent) && !matchedSet.contains(parent)) {
                conflicts->insert(id);
                break;
            }
            parent = lookup.parentById.value(parent, 0);
        }
    }
}

void addSiblingConflicts(const QVector<int>& matched, const RuleLookup& lookup, QSet<int>* conflicts)
{
    for (int a = 0; a < matched.size(); ++a) {
        for (int b = a + 1; b < matched.size(); ++b) {
            const auto ruleA = lookup.byId.value(matched[a]);
            const auto ruleB = lookup.byId.value(matched[b]);
            const bool allowed = ruleA.allowConflict || ruleB.allowConflict
                || RuleGraphUtils::isAncestorRule(lookup.parentById, ruleA.id, ruleB.id)
                || RuleGraphUtils::isAncestorRule(lookup.parentById, ruleB.id, ruleA.id);
            if (!allowed) {
                conflicts->insert(ruleA.id);
                conflicts->insert(ruleB.id);
            }
        }
    }
}

QSet<int> conflictRuleIds(const QVector<int>& matched, const RuleLookup& lookup)
{
    QSet<int> conflicts;
    addMissingAncestorConflicts(matched, lookup, &conflicts);
    addSiblingConflicts(matched, lookup, &conflicts);
    return conflicts;
}
}

RuleEngine::RuleEngine(ImageRepository* images, RuleRepository* rules, QObject* parent)
    : QObject(parent)
    , m_images(images)
    , m_rules(rules)
{
}

bool RuleEngine::isAncestorRule(int possibleAncestorId, int ruleId) const
{
    if (!m_rules || possibleAncestorId == ruleId)
        return false;
    const RuleLookup lookup = buildRuleLookup(m_rules->fetchRules(false));
    return RuleGraphUtils::isAncestorRule(lookup.parentById, possibleAncestorId, ruleId);
}

bool RuleEngine::isConflictBetweenRules(int ruleA, int ruleB) const
{
    if (!m_rules || ruleA == ruleB)
        return false;
    const RuleLookup lookup = buildRuleLookup(m_rules->fetchRules(false));
    return RuleGraphUtils::isConflictBetweenRules(lookup.byId, lookup.parentById, ruleA, ruleB);
}

QVector<int> RuleEngine::matchedRulesForImage(int imageId) const
{
    QVector<int> ids;
    if (!m_images)
        return ids;
    QSqlQuery q(m_images->database());
    q.prepare("SELECT rule_id FROM image_rule_matches WHERE image_id=? ORDER BY rule_id");
    q.addBindValue(imageId);
    if (!SqlUtils::exec(&q, nullptr))
        return ids;
    while (q.next())
        ids << q.value(0).toInt();
    return ids;
}

void RuleEngine::recalculate()
{
    if (!m_images || !m_rules) {
        emit failed(QStringLiteral("RuleEngine 未初始化"));
        return;
    }

    QSqlDatabase db = m_images->database();
    const auto images = m_images->fetchImagesForRuleEvaluation();
    const auto rules = m_rules->fetchRules(true);
    const QVector<CompiledRule> compiledRules = compileRules(rules);
    const RuleLookup ruleLookup = buildRuleLookup(rules);

    if (!db.transaction()) {
        emit failed(db.lastError().text());
        return;
    }
    QSqlQuery clear(db);
    QString queryError;
    if (!SqlUtils::exec(&clear, QStringLiteral("DELETE FROM image_rule_matches"), &queryError)) {
        db.rollback();
        emit failed(queryError);
        return;
    }

    QSqlQuery insert(db);
    insert.prepare("INSERT INTO image_rule_matches (image_id, rule_id, is_conflict, created_at) VALUES (?,?,?,?)");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int i = 0; i < images.size(); ++i) {
        const QVector<int> matched = matchedRuleIds(images.at(i), compiledRules);
        const QSet<int> conflictRules = conflictRuleIds(matched, ruleLookup);
        for (int id : matched) {
            insert.addBindValue(images.at(i).id);
            insert.addBindValue(id);
            insert.addBindValue(conflictRules.contains(id) ? 1 : 0);
            insert.addBindValue(now);
            if (!SqlUtils::exec(&insert, &queryError)) {
                db.rollback();
                emit failed(queryError);
                return;
            }
        }
        if (i % 100 == 0)
            emit progress(i + 1, images.size());
    }
    if (!db.commit()) {
        emit failed(db.lastError().text());
        return;
    }
    emit progress(images.size(), images.size());
    emit finished();
}
