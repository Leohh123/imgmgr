#include "utils/RuleGraphUtils.h"

namespace RuleGraphUtils {

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

}
