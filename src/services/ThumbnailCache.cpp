#include "services/ThumbnailCache.h"

#include "utils/HashUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QThread>
#include <QtConcurrent>

ThumbnailCache::ThumbnailCache(ImageRepository* repository, QObject* parent)
    : QObject(parent)
    , m_repository(repository)
    , m_memoryCache(512)
{
    QThreadPool::globalInstance()->setMaxThreadCount(qMax(2, QThread::idealThreadCount() - 1));
}

void ThumbnailCache::setCacheDir(const QString& cacheDir)
{
    m_cacheDir = cacheDir;
    QDir().mkpath(m_cacheDir);
}

QImage ThumbnailCache::thumbnail(const ImageRecord& image, const QSize& size)
{
    if (auto* cached = m_memoryCache.object(image.id))
        return *cached;

    if (image.width > 0 && image.height > 0 && image.width < size.width() && image.height < size.height()) {
        QImage original(image.absolutePath);
        if (!original.isNull()) {
            m_memoryCache.insert(image.id, new QImage(original), 1);
            return original;
        }
    }

    if (image.thumbnailReady && QFile::exists(image.thumbnailPath)) {
        QImage disk(image.thumbnailPath);
        if (!disk.isNull()) {
            m_memoryCache.insert(image.id, new QImage(disk), 1);
            return disk;
        }
    }

    const QString path = diskPathFor(image);
    if (QFile::exists(path)) {
        QImage disk(path);
        if (!disk.isNull()) {
            m_memoryCache.insert(image.id, new QImage(disk), 1);
            if (m_repository)
                m_repository->updateThumbnail(image.id, path, disk.width(), disk.height());
            return disk;
        }
    }

    scheduleGeneration(image, size);
    QImage placeholder(size, QImage::Format_ARGB32_Premultiplied);
    placeholder.fill(QColor(48, 48, 48, 40));
    return placeholder;
}

void ThumbnailCache::clearMemory()
{
    m_memoryCache.clear();
}

QString ThumbnailCache::diskPathFor(const ImageRecord& image) const
{
    const QString key = image.relativePath + QString::number(image.modifiedTime);
    return QDir(m_cacheDir).filePath(HashUtils::sha1Hex(key) + ".png");
}

void ThumbnailCache::scheduleGeneration(const ImageRecord& image, const QSize& size)
{
    if (m_pending.contains(image.id) || m_cacheDir.isEmpty())
        return;
    m_pending.insert(image.id);
    const QString outputPath = diskPathFor(image);
    const QString sourcePath = image.absolutePath;
    const int imageId = image.id;
    auto* repo = m_repository;

    [[maybe_unused]] auto future = QtConcurrent::run([this, repo, imageId, sourcePath, outputPath, size]() {
        QImageReader reader(sourcePath);
        reader.setAutoTransform(true);
        QSize scaled = reader.size();
        if (scaled.isValid())
            scaled.scale(size, Qt::KeepAspectRatio);
        reader.setScaledSize(scaled.isValid() ? scaled : size);
        QImage thumb = reader.read();
        if (!thumb.isNull()) {
            QDir().mkpath(QFileInfo(outputPath).absolutePath());
            thumb.save(outputPath, "PNG");
        }
        QMetaObject::invokeMethod(this, [this, repo, imageId, outputPath, thumb]() {
            m_pending.remove(imageId);
            if (!thumb.isNull()) {
                m_memoryCache.insert(imageId, new QImage(thumb), 1);
                if (repo)
                    repo->updateThumbnail(imageId, outputPath, thumb.width(), thumb.height());
            }
            emit thumbnailReady(imageId);
        }, Qt::QueuedConnection);
    });
}
