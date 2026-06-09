#include "utils/RecentProjectsStore.h"

#include <QSettings>

namespace {
constexpr int MaxRecentProjects = 10;
}

namespace RecentProjectsStore {

QStringList projects()
{
    return QSettings().value(QStringLiteral("recentProjects")).toStringList();
}

void setProjects(const QStringList& projects)
{
    QSettings().setValue(QStringLiteral("recentProjects"), projects);
}

QStringList addProject(const QString& dbPath)
{
    QStringList items = projects();
    items.removeAll(dbPath);
    items.prepend(dbPath);
    while (items.size() > MaxRecentProjects)
        items.removeLast();
    setProjects(items);
    return items;
}

}
