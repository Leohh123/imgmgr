#include "utils/RuleUtils.h"

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

}
