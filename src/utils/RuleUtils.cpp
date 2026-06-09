#include "utils/RuleUtils.h"

#include <QHash>
#include <QJsonValue>
#include <QStringList>

namespace {
QString normalizedTarget(const QString& target)
{
    return QString(target).replace('\\', '/');
}
}

namespace RuleUtils {

QString globRuleType()
{
    return QStringLiteral("glob");
}

QString regexRuleType()
{
    return QStringLiteral("regex");
}

QString fileNameStemTarget()
{
    return QStringLiteral("filename_stem");
}

QString fileNameTarget()
{
    return QStringLiteral("filename");
}

QString relativePathTarget()
{
    return QStringLiteral("relative_path");
}

QString absolutePathTarget()
{
    return QStringLiteral("absolute_path");
}

QString parentDirTarget()
{
    return QStringLiteral("parent_dir");
}

QSet<QString> validMatchTargets()
{
    return {
        fileNameStemTarget(),
        fileNameTarget(),
        relativePathTarget(),
        absolutePathTarget(),
        parentDirTarget()
    };
}

bool isValidRuleType(const QString& ruleType)
{
    return ruleType == globRuleType() || ruleType == regexRuleType();
}

bool isValidMatchTarget(const QString& matchTarget)
{
    return validMatchTargets().contains(matchTarget);
}

QString targetForImage(const ImageRecord& image, const QString& matchTarget)
{
    if (matchTarget == relativePathTarget()) return image.relativePath;
    if (matchTarget == absolutePathTarget()) return image.absolutePath;
    if (matchTarget == parentDirTarget()) return image.parentDir;
    if (matchTarget == fileNameTarget()) return image.fileName;
    if (matchTarget == fileNameStemTarget()) return image.fileStem;
    return image.fileName;
}

QRegularExpression buildRegularExpression(const QString& pattern, const QString& ruleType, bool caseSensitive, bool wholeMatch)
{
    QString expression;
    if (ruleType == globRuleType()) {
        const auto options = wholeMatch
            ? QRegularExpression::DefaultWildcardConversion
            : QRegularExpression::UnanchoredWildcardConversion;
        expression = QRegularExpression::wildcardToRegularExpression(pattern, options);
    } else {
        expression = wholeMatch ? QStringLiteral("\\A(?:%1)\\z").arg(pattern) : pattern;
    }

    QRegularExpression::PatternOptions patternOptions = QRegularExpression::NoPatternOption;
    if (!caseSensitive)
        patternOptions |= QRegularExpression::CaseInsensitiveOption;
    return QRegularExpression(expression, patternOptions);
}

bool targetMatches(const QString& target, const QString& pattern, const QString& ruleType, bool caseSensitive, bool wholeMatch)
{
    const QRegularExpression re = buildRegularExpression(pattern.trimmed(), ruleType, caseSensitive, wholeMatch);
    if (!re.isValid())
        return false;
    if (re.match(target).hasMatch())
        return true;
    const QString normalized = normalizedTarget(target);
    return normalized != target && re.match(normalized).hasMatch();
}

bool imageMatchesRule(const ImageRecord& image, const RuleRecord& rule)
{
    return targetMatches(targetForImage(image, rule.matchTarget), rule.pattern, rule.ruleType, rule.caseSensitive, rule.wholeMatch);
}

bool imageMatchesFilter(const ImageRecord& image, const ImageFilter& filter)
{
    return targetMatches(targetForImage(image, filter.matchTarget), filter.pattern, filter.ruleType, filter.caseSensitive, filter.wholeMatch);
}

bool isAncestorRule(const QHash<int, int>& parentById, int possibleAncestorId, int ruleId)
{
    if (possibleAncestorId == ruleId)
        return false;
    int current = parentById.value(ruleId, 0);
    while (current != 0) {
        if (current == possibleAncestorId)
            return true;
        current = parentById.value(current, 0);
    }
    return false;
}

bool isConflictBetweenRules(const QHash<int, RuleRecord>& rulesById, const QHash<int, int>& parentById, int ruleA, int ruleB)
{
    if (ruleA == ruleB)
        return false;
    const RuleRecord a = rulesById.value(ruleA);
    const RuleRecord b = rulesById.value(ruleB);
    if (a.allowConflict || b.allowConflict)
        return false;
    return !isAncestorRule(parentById, ruleA, ruleB) && !isAncestorRule(parentById, ruleB, ruleA);
}

QJsonObject ruleToJson(const RuleRecord& rule)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), rule.id);
    object.insert(QStringLiteral("name"), rule.name);
    object.insert(QStringLiteral("rule_type"), rule.ruleType);
    object.insert(QStringLiteral("pattern"), rule.pattern);
    object.insert(QStringLiteral("match_target"), rule.matchTarget);
    object.insert(QStringLiteral("enabled"), rule.enabled);
    object.insert(QStringLiteral("priority"), rule.priority);
    object.insert(QStringLiteral("allow_conflict"), rule.allowConflict);
    object.insert(QStringLiteral("case_sensitive"), rule.caseSensitive);
    object.insert(QStringLiteral("whole_match"), rule.wholeMatch);
    object.insert(QStringLiteral("note"), rule.note);
    return object;
}

QJsonObject ruleTreeToJson(const RuleRecord& rule, const QHash<int, QVector<RuleRecord>>& childrenByParent)
{
    QJsonObject object = ruleToJson(rule);
    QJsonArray children;
    const QVector<RuleRecord> childRules = childrenByParent.value(rule.id);
    for (const RuleRecord& child : childRules)
        children.append(ruleTreeToJson(child, childrenByParent));
    object.insert(QStringLiteral("children"), children);
    return object;
}

bool jsonToRule(const QJsonObject& object, int parentId, RuleRecord* rule, QString* error)
{
    const QStringList required = {
        QStringLiteral("id"),
        QStringLiteral("name"),
        QStringLiteral("rule_type"),
        QStringLiteral("pattern"),
        QStringLiteral("match_target")
    };
    for (const QString& key : required) {
        if (!object.contains(key)) {
            if (error)
                *error = QStringLiteral("规则缺少字段：%1").arg(key);
            return false;
        }
    }

    RuleRecord result;
    result.id = object.value(QStringLiteral("id")).toInt();
    result.parentId = parentId;
    result.name = object.value(QStringLiteral("name")).toString().trimmed();
    result.ruleType = object.value(QStringLiteral("rule_type")).toString();
    result.pattern = object.value(QStringLiteral("pattern")).toString().trimmed();
    result.matchTarget = object.value(QStringLiteral("match_target")).toString();
    result.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    result.priority = object.value(QStringLiteral("priority")).toInt();
    result.allowConflict = object.value(QStringLiteral("allow_conflict")).toBool(false);
    result.caseSensitive = object.value(QStringLiteral("case_sensitive")).toBool(false);
    result.wholeMatch = object.value(QStringLiteral("whole_match")).toBool(true);
    result.note = object.value(QStringLiteral("note")).toString();

    if (result.id <= 0) {
        if (error)
            *error = QStringLiteral("规则 ID 必须大于 0。");
        return false;
    }
    if (result.name.isEmpty() || result.pattern.isEmpty()) {
        if (error)
            *error = QStringLiteral("规则名称和规则内容不能为空。");
        return false;
    }
    if (!isValidRuleType(result.ruleType)) {
        if (error)
            *error = QStringLiteral("规则类型无效：%1").arg(result.ruleType);
        return false;
    }
    if (!isValidMatchTarget(result.matchTarget)) {
        if (error)
            *error = QStringLiteral("匹配目标无效：%1").arg(result.matchTarget);
        return false;
    }
    if (result.ruleType == regexRuleType()) {
        const QRegularExpression re(result.pattern);
        if (!re.isValid()) {
            if (error)
                *error = QStringLiteral("正则表达式无效：%1").arg(re.errorString());
            return false;
        }
    }

    *rule = result;
    return true;
}

bool appendRulesFromJsonTree(const QJsonArray& array, int parentId, QVector<RuleRecord>* rules, QString* error)
{
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            if (error)
                *error = QStringLiteral("rules/children 数组中存在非对象元素。");
            return false;
        }

        const QJsonObject object = value.toObject();
        RuleRecord rule;
        if (!jsonToRule(object, parentId, &rule, error))
            return false;
        rules->append(rule);

        const QJsonValue childrenValue = object.value(QStringLiteral("children"));
        if (!childrenValue.isUndefined()) {
            if (!childrenValue.isArray()) {
                if (error)
                    *error = QStringLiteral("规则“%1”的 children 必须是数组。").arg(rule.name);
                return false;
            }
            if (!appendRulesFromJsonTree(childrenValue.toArray(), rule.id, rules, error))
                return false;
        }
    }
    return true;
}

bool validateImportedRules(const QVector<RuleRecord>& rules, QString* error)
{
    QSet<int> ids;
    QHash<int, int> parentById;
    for (const RuleRecord& rule : rules) {
        if (ids.contains(rule.id)) {
            if (error)
                *error = QStringLiteral("规则 ID 重复：%1").arg(rule.id);
            return false;
        }
        ids.insert(rule.id);
        parentById.insert(rule.id, rule.parentId);
    }

    for (const RuleRecord& rule : rules) {
        if (rule.parentId == rule.id) {
            if (error)
                *error = QStringLiteral("规则不能作为自己的父规则：%1").arg(rule.name);
            return false;
        }
        if (rule.parentId != 0 && !ids.contains(rule.parentId)) {
            if (error)
                *error = QStringLiteral("规则“%1”的父规则不存在。").arg(rule.name);
            return false;
        }

        QSet<int> seen;
        int current = rule.parentId;
        while (current != 0) {
            if (seen.contains(current)) {
                if (error)
                    *error = QStringLiteral("规则树存在循环。");
                return false;
            }
            seen.insert(current);
            current = parentById.value(current, 0);
        }
    }
    return true;
}

}
