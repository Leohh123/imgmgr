#include "services/ProjectPathService.h"

#include <QDir>

namespace ProjectPathService {

bool projectWritesWouldTouchResourceDir(const QString& projectDir, const QString& resourceDir)
{
    const QString resourcePath = QDir(resourceDir).canonicalPath();
    const QString projectPath = QDir(projectDir).canonicalPath();
    if (resourcePath.isEmpty() || projectPath.isEmpty())
        return false;

    const QString normalizedResource = QDir::cleanPath(resourcePath).toLower();
    const QString normalizedProject = QDir::cleanPath(projectPath).toLower();
    return normalizedProject == normalizedResource
        || normalizedProject.startsWith(normalizedResource + QLatin1Char('/'));
}

}
