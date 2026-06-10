#pragma once

#include <QString>

class QWidget;

namespace ProjectFileDialogs {

QString selectNewProjectDatabase(QWidget* parent);
QString selectExistingProjectDatabase(QWidget* parent);
QString selectRuleExportPath(QWidget* parent);
QString selectRuleImportPath(QWidget* parent);
QString selectResourceDirectory(QWidget* parent);

}
