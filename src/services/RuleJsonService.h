#pragma once

#include "types.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QVector>

namespace RuleJsonService {

QJsonDocument buildExportDocument(const QVector<RuleRecord>& rules);
bool parseImportDocument(const QByteArray& json, QVector<RuleRecord>* rules, QString* error);

}
