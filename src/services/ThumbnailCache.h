#pragma once

#include "database/ImageRepository.h"
#include "types.h"

#include <QCache>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QSet>
#include <QThreadPool>

class ThumbnailCache : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailCache(ImageRepository* repository, QObject* parent = nullptr);
    void setCacheDir(const QString& cacheDir);
    QImage thumbnail(const ImageRecord& image, const QSize& size = QSize(128, 128));
    void clearMemory();

signals:
    void thumbnailReady(int imageId);

private:
    QString diskPathFor(const ImageRecord& image) const;
    void scheduleGeneration(const ImageRecord& image, const QSize& size);

    ImageRepository* m_repository = nullptr;
    QString m_cacheDir;
    QCache<int, QImage> m_memoryCache;
    QSet<int> m_pending;
    quint64 m_generation = 0;
};
