#include "services/RuleValidationService.h"

#include "utils/RuleUtils.h"

#include <QRegularExpression>

namespace RuleValidationService {

namespace {
bool validateRegex(const QString& pattern, ValidationError* error)
{
    const QRegularExpression re(pattern);
    if (re.isValid())
        return true;

    if (error) {
        error->title = QStringLiteral("正则无效");
        error->message = re.errorString();
    }
    return false;
}
}

bool validateRuleForSave(const RuleRecord& rule, ValidationError* error)
{
    if (rule.name.isEmpty()) {
        if (error) {
            error->title = QStringLiteral("规则名称为空");
            error->message = QStringLiteral("请输入规则名称，例如“按钮”。");
        }
        return false;
    }
    if (rule.pattern.isEmpty()) {
        if (error) {
            error->title = QStringLiteral("规则为空");
            error->message = QStringLiteral("请输入规则内容。");
        }
        return false;
    }
    if (rule.ruleType == RuleUtils::regexRuleType())
        return validateRegex(rule.pattern, error);
    return true;
}

bool validateFilterPattern(const ImageFilter& filter, ValidationError* error)
{
    if (filter.ruleType != RuleUtils::regexRuleType() || filter.pattern.trimmed().isEmpty())
        return true;
    return validateRegex(filter.pattern, error);
}

}
