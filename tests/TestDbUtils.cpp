#include "TestDbUtils.h"

#include <QFileInfo>
#include <QUuid>

namespace TestDbUtils {

QString uniqueConnectionName(const QString& prefix)
{
    return QStringLiteral("%1_%2").arg(prefix, QUuid::createUuid().toString(QUuid::Id128));
}

QString temporaryDatabasePath(QTemporaryDir* dir)
{
    return dir->filePath(QStringLiteral("project.imgmgr"));
}

ImageRecord makeImage(const QString& stem, bool hasAlpha)
{
    ImageRecord image;
    image.absolutePath = QStringLiteral("D:/assets/%1.png").arg(stem);
    image.relativePath = QStringLiteral("%1.png").arg(stem);
    image.fileName = QStringLiteral("%1.png").arg(stem);
    image.fileStem = stem;
    image.parentDir = QStringLiteral(".");
    image.extension = QStringLiteral("png");
    image.fileSize = 100;
    image.modifiedTime = 100;
    image.width = 16;
    image.height = 16;
    image.hasAlpha = hasAlpha;
    image.imageFormat = QStringLiteral("png");
    return image;
}

ImageRecord makeImage(const QString& absolutePath, const QString& relativePath, const QString& fileName)
{
    ImageRecord image;
    image.absolutePath = absolutePath;
    image.relativePath = relativePath;
    image.fileName = fileName;
    image.fileStem = QFileInfo(fileName).completeBaseName();
    image.parentDir = QFileInfo(relativePath).path();
    image.extension = QFileInfo(fileName).suffix();
    image.fileSize = 1024;
    image.modifiedTime = 100;
    image.width = 64;
    image.height = 32;
    image.hasAlpha = true;
    image.imageFormat = QStringLiteral("png");
    return image;
}

RuleRecord makeRule(const QString& name, const QString& pattern, int parentId)
{
    RuleRecord rule;
    rule.parentId = parentId;
    rule.name = name;
    rule.pattern = pattern;
    return rule;
}

}
