#pragma once

#include "types.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace RuleExplanationBuilder {
QString rulePath(int ruleId, const QHash<int, RuleRecord>& rulesById);
QString conflictReason(int ruleA, int ruleB, const QHash<int, RuleRecord>& rulesById, const QHash<int, int>& parentById);
QString build(const ImageRecord& image, const QVector<int>& matches, const QVector<RuleRecord>& rules);
}
