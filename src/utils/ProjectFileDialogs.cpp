#include "utils/ProjectFileDialogs.h"

#include <QDialog>
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

QString selectFile(QWidget* parent,
    const QString& title,
    const QString& filter,
    QFileDialog::AcceptMode acceptMode,
    QFileDialog::FileMode fileMode,
    const QString& defaultSuffix = {})
{
    QFileDialog dialog(parent, title, QDir::currentPath(), filter);
    dialog.setAcceptMode(acceptMode);
    dialog.setFileMode(fileMode);
    if (!defaultSuffix.isEmpty())
        dialog.setDefaultSuffix(defaultSuffix);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.selectedFiles().value(0);
}
}

QString selectNewProjectDatabase(QWidget* parent)
{
    return selectFile(parent,
        QStringLiteral("新建项目数据库"),
        sqliteFilter(),
        QFileDialog::AcceptSave,
        QFileDialog::AnyFile,
        QStringLiteral("db"));
}

QString selectExistingProjectDatabase(QWidget* parent)
{
    return selectFile(parent,
        QStringLiteral("打开项目数据库"),
        sqliteFilter(),
        QFileDialog::AcceptOpen,
        QFileDialog::ExistingFile);
}

QString selectRuleExportPath(QWidget* parent)
{
    return selectFile(parent,
        QStringLiteral("导出规则为 JSON"),
        jsonFilter(),
        QFileDialog::AcceptSave,
        QFileDialog::AnyFile,
        QStringLiteral("json"));
}

QString selectRuleImportPath(QWidget* parent)
{
    return selectFile(parent,
        QStringLiteral("从 JSON 导入规则并覆盖"),
        jsonFilter(),
        QFileDialog::AcceptOpen,
        QFileDialog::ExistingFile);
}

QString selectResourceDirectory(QWidget* parent)
{
    QFileDialog dialog(parent, QStringLiteral("选择资源目录"), QDir::currentPath());
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    return dialog.selectedFiles().value(0);
}

}
