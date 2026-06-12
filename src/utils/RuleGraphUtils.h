#pragma once

#include "types.h"

#include <QHash>

namespace RuleGraphUtils {

bool isAncestorRule(const QHash<int, int>& parentById, int possibleAncestorId, int ruleId);
bool isConflictBetweenRules(const QHash<int, RuleRecord>& rulesById, const QHash<int, int>& parentById, int ruleA, int ruleB);

}
