#include "database/RuleRepository.h"

#include "database/SqlUtils.h"

#include <QDateTime>
#include <QQueue>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

namespace {
RuleRecord ruleFromQuery(const QSqlQuery& q)
{
    RuleRecord r;
    r.id = q.value("id").toInt();
    r.parentId = q.value("parent_id").toInt();
    r.name = q.value("name").toString();
    r.ruleType = q.value("rule_type").toString();
    r.pattern = q.value("pattern").toString();
    r.matchTarget = q.value("match_target").toString();
    r.enabled = q.value("enabled").toInt() != 0;
    r.priority = q.value("priority").toInt();
    r.allowConflict = q.value("allow_conflict").toInt() != 0;
    r.caseSensitive = q.value("case_sensitive").toInt() != 0;
    r.wholeMatch = q.value("whole_match").toInt() != 0;
    r.note = q.value("note").toString();
    r.matchCount = q.value("match_count").toInt();
    r.conflictCount = q.value("conflict_count").toInt();
    return r;
}

void bindNullableParentId(QSqlQuery* query, int parentId)
{
    query->addBindValue(parentId == 0 ? QVariant() : parentId);
}

void bindRuleFields(QSqlQuery* query, const RuleRecord& rule)
{
    bindNullableParentId(query, rule.parentId);
    query->addBindValue(rule.name);
    query->addBindValue(rule.ruleType);
    query->addBindValue(rule.pattern);
    query->addBindValue(rule.matchTarget);
    query->addBindValue(rule.enabled ? 1 : 0);
    query->addBindValue(rule.priority);
    query->addBindValue(rule.allowConflict ? 1 : 0);
    query->addBindValue(rule.caseSensitive ? 1 : 0);
    query->addBindValue(rule.wholeMatch ? 1 : 0);
    query->addBindValue(rule.note);
}

void bindRuleTimestamps(QSqlQuery* query, qint64 createdAt, qint64 updatedAt)
{
    query->addBindValue(createdAt);
    query->addBindValue(updatedAt);
}

QString fetchRulesSql(bool enabledOnly)
{
    QString sql = QStringLiteral(
        "SELECT r.*, COUNT(m.image_id) AS match_count, SUM(COALESCE(m.is_conflict,0)) AS conflict_count "
        "FROM rules r LEFT JOIN image_rule_matches m ON m.rule_id=r.id");
    if (enabledOnly)
        sql += QStringLiteral(" WHERE r.enabled=1");
    sql += QStringLiteral(" GROUP BY r.id ORDER BY r.parent_id, r.priority DESC, r.name");
    return sql;
}
}

RuleRepository::RuleRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

int RuleRepository::addRule(const RuleRecord& rule)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO rules (parent_id,name,rule_type,pattern,match_target,enabled,priority,allow_conflict,case_sensitive,whole_match,note,created_at,updated_at) "
              "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    bindRuleFields(&q, rule);
    bindRuleTimestamps(&q, now, now);
    if (!SqlUtils::exec(&q, &m_lastError))
        return 0;
    return q.lastInsertId().toInt();
}

bool RuleRepository::updateRule(const RuleRecord& rule)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE rules SET parent_id=?,name=?,rule_type=?,pattern=?,match_target=?,enabled=?,priority=?,allow_conflict=?,case_sensitive=?,whole_match=?,note=?,updated_at=? WHERE id=?");
    bindRuleFields(&q, rule);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.addBindValue(rule.id);
    if (!SqlUtils::exec(&q, &m_lastError))
        return false;
    return true;
}

bool RuleRepository::replaceRules(const QVector<RuleRecord>& rules)
{
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }

    QSqlQuery clear(m_db);
    const QStringList clearStatements = {
        QStringLiteral("DELETE FROM image_rule_matches"),
        QStringLiteral("DELETE FROM rule_excludes"),
        QStringLiteral("DELETE FROM rules")
    };
    for (const QString& statement : clearStatements) {
        if (!SqlUtils::exec(&clear, statement, &m_lastError)) {
            m_db.rollback();
            return false;
        }
    }

    QSqlQuery insert(m_db);
    insert.prepare("INSERT INTO rules (id,parent_id,name,rule_type,pattern,match_target,enabled,priority,allow_conflict,case_sensitive,whole_match,note,created_at,updated_at) "
                   "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const RuleRecord& rule : rules) {
        insert.addBindValue(rule.id);
        bindRuleFields(&insert, rule);
        bindRuleTimestamps(&insert, now, now);
        if (!SqlUtils::exec(&insert, &m_lastError)) {
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    return true;
}

bool RuleRepository::removeRule(int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM rules WHERE id=?");
    q.addBindValue(id);
    if (!SqlUtils::exec(&q, &m_lastError))
        return false;
    return true;
}

bool RuleRepository::removeRuleRecursive(int id)
{
    QVector<int> ids;
    if (!collectChildRuleIdsRecursive(id, &ids))
        return false;
    std::reverse(ids.begin(), ids.end());
    ids.append(id);
    if (!m_db.transaction()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    QSqlQuery deleteMatches(m_db);
    QSqlQuery deleteRules(m_db);
    deleteMatches.prepare("DELETE FROM image_rule_matches WHERE rule_id=?");
    deleteRules.prepare("DELETE FROM rules WHERE id=?");
    for (int ruleId : ids) {
        deleteMatches.addBindValue(ruleId);
        if (!SqlUtils::exec(&deleteMatches, &m_lastError)) {
            m_db.rollback();
            return false;
        }
        deleteRules.addBindValue(ruleId);
        if (!SqlUtils::exec(&deleteRules, &m_lastError)) {
            m_db.rollback();
            return false;
        }
    }
    if (!m_db.commit()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    return true;
}

RuleRecord RuleRepository::fetchRule(int id) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT r.*, COUNT(m.image_id) AS match_count, SUM(COALESCE(m.is_conflict,0)) AS conflict_count "
              "FROM rules r LEFT JOIN image_rule_matches m ON m.rule_id=r.id WHERE r.id=? GROUP BY r.id");
    q.addBindValue(id);
    if (!SqlUtils::exec(&q, &m_lastError))
        return {};
    if (!q.next())
        return {};

    return ruleFromQuery(q);
}

QVector<RuleRecord> RuleRepository::fetchRules(bool enabledOnly) const
{
    QVector<RuleRecord> rules;
    QSqlQuery q(m_db);
    if (!SqlUtils::exec(&q, fetchRulesSql(enabledOnly), &m_lastError))
        return rules;

    while (q.next()) {
        rules << ruleFromQuery(q);
    }
    return rules;
}

QVector<int> RuleRepository::childRuleIdsRecursive(int ruleId) const
{
    QVector<int> result;
    collectChildRuleIdsRecursive(ruleId, &result);
    return result;
}

bool RuleRepository::collectChildRuleIdsRecursive(int ruleId, QVector<int>* result) const
{
    QQueue<int> queue;
    queue.enqueue(ruleId);
    while (!queue.isEmpty()) {
        const int parent = queue.dequeue();
        QSqlQuery q(m_db);
        q.prepare("SELECT id FROM rules WHERE parent_id=?");
        q.addBindValue(parent);
        if (!SqlUtils::exec(&q, &m_lastError))
            return false;
        while (q.next()) {
            const int id = q.value(0).toInt();
            *result << id;
            queue.enqueue(id);
        }
    }
    return true;
}

QHash<int, int> RuleRepository::matchCounts() const
{
    QHash<int, int> counts;
    QSqlQuery q(m_db);
    if (!SqlUtils::exec(&q, QStringLiteral("SELECT rule_id, COUNT(*) FROM image_rule_matches GROUP BY rule_id"), &m_lastError))
        return counts;

    while (q.next())
        counts.insert(q.value(0).toInt(), q.value(1).toInt());
    return counts;
}
