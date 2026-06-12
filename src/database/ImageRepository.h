#pragma once

#include "types.h"

#include <QHash>
#include <QSqlDatabase>
#include <QStringList>

class ImageRepository {
public:
    static constexpr int DefaultImageFetchLimit = 50000;
    static constexpr int DefaultFetchLimit = DefaultImageFetchLimit;

    explicit ImageRepository(QSqlDatabase db = {});
    void setDatabase(QSqlDatabase db) { m_db = db; }

    bool upsertImages(const QVector<ImageRecord>& records);
    QVector<ImageRecord> fetchImages(const ImageFilter& filter, int limit = DefaultImageFetchLimit) const;
    QVector<ImageRecord> fetchAllImages(const ImageFilter& filter = {}) const;
    QVector<ImageRecord> fetchImagesForRuleEvaluation() const;
    ImageRecord fetchImage(int imageId) const;
    bool updateThumbnail(int imageId, const QString& thumbnailPath, int width, int height);
    int imageCount() const;
    QSqlDatabase database() const { return m_db; }
    QString lastError() const { return m_lastError; }

private:
    QSqlDatabase m_db;
    mutable QString m_lastError;
};
