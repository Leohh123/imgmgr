#include "services/RuleJsonService.h"

#include "utils/RuleUtils.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>

namespace RuleJsonService {

QJsonDocument buildExportDocument(const QVector<RuleRecord>& rules)
{
    QJsonArray ruleArray;
    QHash<int, QVector<RuleRecord>> childrenByParent;
    for (const RuleRecord& rule : rules)
        childrenByParent[rule.parentId].append(rule);
    for (const RuleRecord& rule : childrenByParent.value(0))
        ruleArray.append(RuleUtils::ruleTreeToJson(rule, childrenByParent));

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("imgmgr.rules"));
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("rules"), ruleArray);
    return QJsonDocument(root);
}

bool parseImportDocument(const QByteArray& json, QVector<RuleRecord>* rules, QString* error)
{
    if (!rules) {
        if (error)
            *error = QStringLiteral("内部错误：规则输出参数为空。");
        return false;
    }
    rules->clear();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error)
            *error = parseError.errorString();
        return false;
    }

    QJsonArray ruleArray;
    if (document.isObject()) {
        const QJsonObject root = document.object();
        if (!root.contains(QStringLiteral("rules")) || !root.value(QStringLiteral("rules")).isArray()) {
            if (error)
                *error = QStringLiteral("JSON 中缺少 rules 数组。");
            return false;
        }
        ruleArray = root.value(QStringLiteral("rules")).toArray();
    } else if (document.isArray()) {
        ruleArray = document.array();
    } else {
        if (error)
            *error = QStringLiteral("JSON 根节点必须是对象或数组。");
        return false;
    }

    if (!RuleUtils::appendRulesFromJsonTree(ruleArray, 0, rules, error))
        return false;
    return RuleUtils::validateImportedRules(*rules, error);
}

}
