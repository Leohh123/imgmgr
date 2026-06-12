#pragma once

#include "types.h"

#include <QTemporaryDir>

namespace TestDbUtils {

QString uniqueConnectionName(const QString& prefix);
QString temporaryDatabasePath(QTemporaryDir* dir);

ImageRecord makeImage(const QString& stem, bool hasAlpha = false);
ImageRecord makeImage(const QString& absolutePath, const QString& relativePath, const QString& fileName);

RuleRecord makeRule(const QString& name, const QString& pattern, int parentId = 0);

}
