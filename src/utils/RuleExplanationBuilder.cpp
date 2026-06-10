#include "utils/RuleExplanationBuilder.h"

#include "utils/RuleUtils.h"

#include <QSet>
#include <QStringList>

namespace RuleExplanationBuilder {

namespace {
QString cachedRulePath(int ruleId, const QHash<int, RuleRecord>& rulesById, QHash<int, QString>* pathByRuleId)
{
    if (!pathByRuleId)
        return rulePath(ruleId, rulesById);

    const auto it = pathByRuleId->constFind(ruleId);
    if (it != pathByRuleId->constEnd())
        return it.value();

    const QString path = rulePath(ruleId, rulesById);
    pathByRuleId->insert(ruleId, path);
    return path;
}

QString cachedConflictReason(
    int ruleA,
    int ruleB,
    const QHash<int, RuleRecord>& rulesById,
    const QHash<int, int>& parentById,
    QHash<int, QString>* pathByRuleId)
{
    const RuleRecord a = rulesById.value(ruleA);
    const RuleRecord b = rulesById.value(ruleB);
    if (a.allowConflict || b.allowConflict)
        return QStringLiteral("无冲突：至少一个规则允许冲突。");
    if (RuleUtils::isAncestorRule(parentById, ruleA, ruleB))
        return QStringLiteral("无冲突：%1 是 %2 的祖先规则。")
            .arg(cachedRulePath(ruleA, rulesById, pathByRuleId),
                 cachedRulePath(ruleB, rulesById, pathByRuleId));
    if (RuleUtils::isAncestorRule(parentById, ruleB, ruleA))
        return QStringLiteral("无冲突：%1 是 %2 的祖先规则。")
            .arg(cachedRulePath(ruleB, rulesById, pathByRuleId),
                 cachedRulePath(ruleA, rulesById, pathByRuleId));
    return QStringLiteral("存在冲突：两个规则不在同一祖先链上，且未设置允许冲突。");
}
}

QString rulePath(int ruleId, const QHash<int, RuleRecord>& rulesById)
{
    QStringList parts;
    int current = ruleId;
    QSet<int> seen;
    while (current != 0 && rulesById.contains(current) && !seen.contains(current)) {
        seen.insert(current);
        const RuleRecord rule = rulesById.value(current);
        parts.prepend(rule.name);
        current = rule.parentId;
    }
    return parts.join(QStringLiteral(" / "));
}

QString conflictReason(int ruleA, int ruleB, const QHash<int, RuleRecord>& rulesById, const QHash<int, int>& parentById)
{
    const RuleRecord a = rulesById.value(ruleA);
    const RuleRecord b = rulesById.value(ruleB);
    if (a.allowConflict || b.allowConflict)
        return QStringLiteral("无冲突：至少一个规则允许冲突。");
    if (RuleUtils::isAncestorRule(parentById, ruleA, ruleB))
        return QStringLiteral("无冲突：%1 是 %2 的祖先规则。").arg(rulePath(ruleA, rulesById), rulePath(ruleB, rulesById));
    if (RuleUtils::isAncestorRule(parentById, ruleB, ruleA))
        return QStringLiteral("无冲突：%1 是 %2 的祖先规则。").arg(rulePath(ruleB, rulesById), rulePath(ruleA, rulesById));
    return QStringLiteral("存在冲突：两个规则不在同一祖先链上，且未设置允许冲突。");
}

QString build(const ImageRecord& image, const QVector<int>& matches, const QVector<RuleRecord>& rules)
{
    QHash<int, RuleRecord> rulesById;
    QHash<int, int> parentById;
    for (const RuleRecord& rule : rules) {
        rulesById.insert(rule.id, rule);
        parentById.insert(rule.id, rule.parentId);
    }
    QHash<int, QString> pathByRuleId;

    QStringList lines;
    lines << QStringLiteral("当前图片：") << image.relativePath << QString();
    lines << QStringLiteral("命中规则：");
    if (matches.isEmpty()) {
        lines << QStringLiteral("- 无");
    } else {
        for (int id : matches) {
            const RuleRecord rule = rulesById.value(id);
            lines << QStringLiteral("- %1  [%2: %3 | 目标: %4%5]")
                .arg(cachedRulePath(id, rulesById, &pathByRuleId),
                     rule.ruleType,
                     rule.pattern,
                     rule.matchTarget,
                     rule.enabled ? QString() : QStringLiteral(" | 已禁用"));
        }
    }

    QSet<int> matchedSet;
    for (int id : matches)
        matchedSet.insert(id);

    QStringList conflictLines;
    QStringList ancestorConflictLines;
    QStringList nonConflictLines;
    for (int id : matches) {
        int parent = rulesById.value(id).parentId;
        while (parent != 0 && rulesById.contains(parent)) {
            const RuleRecord ancestor = rulesById.value(parent);
            if (ancestor.enabled && !matchedSet.contains(parent)) {
                ancestorConflictLines << QStringLiteral("- %1 命中了子规则“%2”，但没有命中启用的祖先规则“%3”。")
                    .arg(image.relativePath,
                         cachedRulePath(id, rulesById, &pathByRuleId),
                         cachedRulePath(parent, rulesById, &pathByRuleId));
                break;
            }
            parent = ancestor.parentId;
        }
    }

    for (int i = 0; i < matches.size(); ++i) {
        for (int j = i + 1; j < matches.size(); ++j) {
            const int a = matches.at(i);
            const int b = matches.at(j);
            const QString pairText = QStringLiteral("- %1  <->  %2\n  %3")
                .arg(cachedRulePath(a, rulesById, &pathByRuleId),
                     cachedRulePath(b, rulesById, &pathByRuleId),
                     cachedConflictReason(a, b, rulesById, parentById, &pathByRuleId));
            if (RuleUtils::isConflictBetweenRules(rulesById, parentById, a, b))
                conflictLines << pairText;
            else
                nonConflictLines << pairText;
        }
    }

    lines << QString() << QStringLiteral("最终状态：") << imageStatusText(image.status);
    lines << QString() << QStringLiteral("冲突判断：");
    if (matches.size() <= 1 && ancestorConflictLines.isEmpty()) {
        lines << QStringLiteral("- 命中规则数量不超过 1，不存在规则冲突。");
    } else {
        if (!ancestorConflictLines.isEmpty()) {
            lines << QStringLiteral("存在祖先链冲突：");
            lines << ancestorConflictLines;
        }
        if (!conflictLines.isEmpty()) {
            lines << QStringLiteral("存在冲突的规则对：");
            lines << conflictLines;
        } else if (ancestorConflictLines.isEmpty()) {
            lines << QStringLiteral("- 无冲突。所有多重命中规则都在同一祖先链上，或规则允许冲突。");
        }
        if (!nonConflictLines.isEmpty()) {
            lines << QString() << QStringLiteral("非冲突规则对：");
            lines << nonConflictLines;
        }
    }

    if (!conflictLines.isEmpty() || !ancestorConflictLines.isEmpty()) {
        lines << QString() << QStringLiteral("建议处理方式：");
        lines << QStringLiteral("- 将更具体的规则移动为泛化规则的子规则。");
        lines << QStringLiteral("- 为确实可共存的规则启用“允许冲突”。");
        lines << QStringLiteral("- 调整规则内容，避免无关资源被同时命中。");
        lines << QStringLiteral("- 后续可添加排除规则进一步细化分类。");
    }
    return lines.join(QStringLiteral("\n"));
}

}
