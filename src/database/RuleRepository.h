#pragma once

#include "types.h"

#include <QHash>
#include <QSqlDatabase>

class RuleRepository {
public:
    explicit RuleRepository(QSqlDatabase db = {});
    void setDatabase(QSqlDatabase db) { m_db = db; }

    int addRule(const RuleRecord& rule);
    bool updateRule(const RuleRecord& rule);
    bool replaceRules(const QVector<RuleRecord>& rules);
    bool removeRule(int id);
    bool removeRuleRecursive(int id);
    RuleRecord fetchRule(int id) const;
    QVector<RuleRecord> fetchRules(bool enabledOnly = false) const;
    QVector<int> childRuleIdsRecursive(int ruleId) const;
    QHash<int, int> matchCounts() const;
    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase m_db;
    mutable QString m_lastError;
};
