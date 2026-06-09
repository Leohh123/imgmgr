#pragma once

#include "types.h"

#include <QString>

namespace RuleValidationService {

struct ValidationError {
    QString title;
    QString message;
};

bool validateRuleForSave(const RuleRecord& rule, ValidationError* error);
bool validateFilterPattern(const ImageFilter& filter, ValidationError* error);

}
