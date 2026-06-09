#include "services/RuleEngine.h"

#include "utils/RuleUtils.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QtConcurrent>

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
    const auto rules = m_rules->fetchRules(false);
    QHash<int, int> parentById;
    for (const auto& rule : rules)
        parentById.insert(rule.id, rule.parentId);
    return RuleUtils::isAncestorRule(parentById, possibleAncestorId, ruleId);
}

bool RuleEngine::isConflictBetweenRules(int ruleA, int ruleB) const
{
    if (!m_rules || ruleA == ruleB)
        return false;
    const auto rules = m_rules->fetchRules(false);
    QHash<int, RuleRecord> byId;
    QHash<int, int> parentById;
    for (const auto& rule : rules) {
        byId.insert(rule.id, rule);
        parentById.insert(rule.id, rule.parentId);
    }
    return RuleUtils::isConflictBetweenRules(byId, parentById, ruleA, ruleB);
}

QVector<int> RuleEngine::matchedRulesForImage(int imageId) const
{
    QVector<int> ids;
    if (!m_images)
        return ids;
    QSqlQuery q(m_images->database());
    q.prepare("SELECT rule_id FROM image_rule_matches WHERE image_id=? ORDER BY rule_id");
    q.addBindValue(imageId);
    q.exec();
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
    const auto images = m_images->fetchAllImages();
    const auto rules = m_rules->fetchRules(true);
    const QVector<CompiledRule> compiledRules = compileRules(rules);
    QHash<int, RuleRecord> ruleById;
    QHash<int, int> parentById;
    for (const auto& r : rules) {
        ruleById.insert(r.id, r);
        parentById.insert(r.id, r.parentId);
    }

    if (!db.transaction()) {
        emit failed(db.lastError().text());
        return;
    }
    QSqlQuery clear(db);
    if (!clear.exec("DELETE FROM image_rule_matches")) {
        db.rollback();
        emit failed(clear.lastError().text());
        return;
    }

    QSqlQuery insert(db);
    insert.prepare("INSERT INTO image_rule_matches (image_id, rule_id, is_conflict, created_at) VALUES (?,?,?,?)");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (int i = 0; i < images.size(); ++i) {
        QVector<int> matched;
        for (const CompiledRule& compiledRule : compiledRules) {
            if (compiledRule.matches(images.at(i)))
                matched << compiledRule.rule.id;
        }
        QSet<int> matchedSet;
        for (int id : matched)
            matchedSet.insert(id);

        QSet<int> conflictRules;
        for (int id : matched) {
            int parent = parentById.value(id, 0);
            while (parent != 0) {
                if (ruleById.contains(parent) && !matchedSet.contains(parent)) {
                    conflictRules.insert(id);
                    break;
                }
                parent = parentById.value(parent, 0);
            }
        }
        for (int a = 0; a < matched.size(); ++a) {
            for (int b = a + 1; b < matched.size(); ++b) {
                const auto ra = ruleById.value(matched[a]);
                const auto rb = ruleById.value(matched[b]);
                const bool allowed = ra.allowConflict || rb.allowConflict
                    || RuleUtils::isAncestorRule(parentById, ra.id, rb.id)
                    || RuleUtils::isAncestorRule(parentById, rb.id, ra.id);
                if (!allowed) {
                    conflictRules.insert(ra.id);
                    conflictRules.insert(rb.id);
                }
            }
        }
        for (int id : matched) {
            insert.addBindValue(images.at(i).id);
            insert.addBindValue(id);
            insert.addBindValue(conflictRules.contains(id) ? 1 : 0);
            insert.addBindValue(now);
            if (!insert.exec()) {
                db.rollback();
                emit failed(insert.lastError().text());
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
