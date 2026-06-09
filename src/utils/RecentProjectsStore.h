#pragma once

#include <QString>
#include <QStringList>

namespace RecentProjectsStore {
QStringList projects();
void setProjects(const QStringList& projects);
QStringList addProject(const QString& dbPath);
}
