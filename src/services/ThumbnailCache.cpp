#include "services/ThumbnailCache.h"

#include "utils/HashUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QPointer>
#include <QThread>
#include <QtConcurrent>

namespace {
constexpr int ThumbnailMemoryCacheCostLimit = 512;

QImage generateThumbnailImage(const QString& sourcePath, const QString& outputPath, const QSize& size)
{
    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    QSize scaled = reader.size();
    if (scaled.isValid())
        scaled.scale(size, Qt::KeepAspectRatio);
    reader.setScaledSize(scaled.isValid() ? scaled : size);

    QImage thumbnail = reader.read();
    if (!thumbnail.isNull()) {
        QDir().mkpath(QFileInfo(outputPath).absolutePath());
        thumbnail.save(outputPath, "PNG");
    }
    return thumbnail;
}
}

ThumbnailCache::ThumbnailCache(ImageRepository* repository, QObject* parent)
    : QObject(parent)
    , m_repository(repository)
    , m_memoryCache(ThumbnailMemoryCacheCostLimit)
{
    QThreadPool::globalInstance()->setMaxThreadCount(qMax(2, QThread::idealThreadCount() - 1));
}

void ThumbnailCache::setCacheDir(const QString& cacheDir)
{
    ++m_generation;
    m_pending.clear();
    m_memoryCache.clear();
    m_cacheDir = cacheDir;
    QDir().mkpath(m_cacheDir);
}

QImage ThumbnailCache::thumbnail(const ImageRecord& image, const QSize& size)
{
    if (auto* cached = m_memoryCache.object(image.id))
        return *cached;

    QImage thumbnail = loadOriginalIfSmaller(image, size);
    if (!thumbnail.isNull())
        return thumbnail;

    thumbnail = loadDiskThumbnail(image);
    if (!thumbnail.isNull())
        return thumbnail;

    thumbnail = loadGeneratedThumbnail(image);
    if (!thumbnail.isNull())
        return thumbnail;

    scheduleGeneration(image, size);
    return placeholderThumbnail(size);
}

void ThumbnailCache::clearMemory()
{
    ++m_generation;
    m_pending.clear();
    m_memoryCache.clear();
}

void ThumbnailCache::cacheImage(int imageId, const QImage& image)
{
    m_memoryCache.insert(imageId, new QImage(image), 1);
}

void ThumbnailCache::applyGeneratedThumbnail(int imageId, const QString& outputPath, const QImage& thumbnail, quint64 generation)
{
    if (generation != m_generation)
        return;

    m_pending.remove(imageId);
    if (!thumbnail.isNull()) {
        cacheImage(imageId, thumbnail);
        if (m_repository)
            m_repository->updateThumbnail(imageId, outputPath, thumbnail.width(), thumbnail.height());
    }
    emit thumbnailReady(imageId);
}

QImage ThumbnailCache::loadOriginalIfSmaller(const ImageRecord& image, const QSize& size)
{
    if (image.width <= 0 || image.height <= 0 || image.width >= size.width() || image.height >= size.height())
        return {};

    QImage original(image.absolutePath);
    if (!original.isNull())
        cacheImage(image.id, original);
    return original;
}

QImage ThumbnailCache::loadDiskThumbnail(const ImageRecord& image)
{
    if (!image.thumbnailReady || !QFile::exists(image.thumbnailPath))
        return {};

    QImage disk(image.thumbnailPath);
    if (!disk.isNull())
        cacheImage(image.id, disk);
    return disk;
}

QImage ThumbnailCache::loadGeneratedThumbnail(const ImageRecord& image)
{
    const QString path = diskPathFor(image);
    if (!QFile::exists(path))
        return {};

    QImage disk(path);
    if (disk.isNull())
        return {};

    cacheImage(image.id, disk);
    if (m_repository)
        m_repository->updateThumbnail(image.id, path, disk.width(), disk.height());
    return disk;
}

QImage ThumbnailCache::placeholderThumbnail(const QSize& size) const
{
    QImage placeholder(size, QImage::Format_ARGB32_Premultiplied);
    placeholder.fill(QColor(48, 48, 48, 40));
    return placeholder;
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
    const quint64 generation = m_generation;
    QPointer<ThumbnailCache> cache(this);

    [[maybe_unused]] auto future = QtConcurrent::run([cache, imageId, sourcePath, outputPath, size, generation]() {
        const QImage thumb = generateThumbnailImage(sourcePath, outputPath, size);
        if (!cache)
            return;

        QMetaObject::invokeMethod(cache, [cache, imageId, outputPath, thumb, generation]() {
            if (!cache)
                return;
            cache->applyGeneratedThumbnail(imageId, outputPath, thumb, generation);
        }, Qt::QueuedConnection);
    });
}
