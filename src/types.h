#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

enum class ImageStatus {
    Unclassified = 0,
    Classified = 1,
    Conflict = 2,
    MultiMatch = 3
};

inline QString imageStatusText(ImageStatus status)
{
    switch (status) {
    case ImageStatus::Unclassified: return QStringLiteral("未分类");
    case ImageStatus::Classified: return QStringLiteral("已分类");
    case ImageStatus::Conflict: return QStringLiteral("冲突");
    case ImageStatus::MultiMatch: return QStringLiteral("多重命中");
    }
    return {};
}

struct ImageRecord {
    int id = 0;
    QString absolutePath;
    QString relativePath;
    QString fileName;
    QString fileStem;
    QString parentDir;
    QString extension;
    qint64 fileSize = 0;
    qint64 modifiedTime = 0;
    int width = 0;
    int height = 0;
    bool hasAlpha = false;
    QString imageFormat;
    QString thumbnailPath;
    bool thumbnailReady = false;
    int matchCount = 0;
    ImageStatus status = ImageStatus::Unclassified;
};

struct ImageFilter {
    QString pattern;
    QString ruleType = QStringLiteral("glob");
    QString matchTarget = QStringLiteral("filename_stem");
    bool onlyClassified = true;
    bool onlyUnclassified = true;
    bool onlyConflict = true;
    bool onlyMultiMatch = true;
    int currentRuleId = 0;
    bool includeChildren = false;
    bool onlyCurrentRule = false;
    bool caseSensitive = false;
    bool wholeMatch = true;
};

struct RuleRecord {
    int id = 0;
    int parentId = 0;
    QString name;
    QString ruleType = QStringLiteral("glob");
    QString pattern;
    QString matchTarget = QStringLiteral("filename_stem");
    bool enabled = true;
    int priority = 0;
    bool allowConflict = false;
    bool caseSensitive = false;
    bool wholeMatch = true;
    QString note;
    int matchCount = 0;
    int conflictCount = 0;
};
