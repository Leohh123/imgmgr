#pragma once

#include "types.h"

#include <QRegularExpression>
#include <QSet>
#include <QString>

namespace RuleUtils {

QString globRuleType();
QString regexRuleType();

QString fileNameStemTarget();
QString fileNameTarget();
QString relativePathTarget();
QString absolutePathTarget();
QString parentDirTarget();

QSet<QString> validMatchTargets();
bool isValidRuleType(const QString& ruleType);
bool isValidMatchTarget(const QString& matchTarget);

QString targetForImage(const ImageRecord& image, const QString& matchTarget);
QRegularExpression buildRegularExpression(const QString& pattern, const QString& ruleType, bool caseSensitive, bool wholeMatch);
bool targetMatches(const QString& target, const QString& pattern, const QString& ruleType, bool caseSensitive, bool wholeMatch);
bool imageMatchesRule(const ImageRecord& image, const RuleRecord& rule);
bool imageMatchesFilter(const ImageRecord& image, const ImageFilter& filter);

}
