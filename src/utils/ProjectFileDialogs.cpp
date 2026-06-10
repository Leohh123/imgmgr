#include "utils/ProjectFileDialogs.h"

#include <QDir>
#include <QFileDialog>

namespace ProjectFileDialogs {

namespace {
QString sqliteFilter()
{
    return QStringLiteral("SQLite DB (*.db)");
}

QString jsonFilter()
{
    return QStringLiteral("JSON 文件 (*.json)");
}
}

QString selectNewProjectDatabase(QWidget* parent)
{
    QFileDialog dialog(parent, QStringLiteral("新建项目数据库"), QDir::currentPath(), sqliteFilter());
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(QStringLiteral("db"));
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.selectedFiles().value(0);
}

QString selectExistingProjectDatabase(QWidget* parent)
{
    QFileDialog dialog(parent, QStringLiteral("打开项目数据库"), QDir::currentPath(), sqliteFilter());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.selectedFiles().value(0);
}

QString selectRuleExportPath(QWidget* parent)
{
    return QFileDialog::getSaveFileName(parent,
        QStringLiteral("导出规则为 JSON"),
        QDir::currentPath(),
        jsonFilter());
}

QString selectRuleImportPath(QWidget* parent)
{
    return QFileDialog::getOpenFileName(parent,
        QStringLiteral("从 JSON 导入规则并覆盖"),
        QDir::currentPath(),
        jsonFilter());
}

QString selectResourceDirectory(QWidget* parent)
{
    return QFileDialog::getExistingDirectory(parent, QStringLiteral("选择资源目录"));
}

}
