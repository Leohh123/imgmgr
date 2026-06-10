#include "services/ProjectScanner.h"

#include "utils/ImageUtils.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent>

#include <functional>

namespace {
QVector<QString> enumerateImageFiles(const QString& resourceDir, const std::function<void(int, const QString&)>& onProgress)
{
    QVector<QString> files;
    QDirIterator it(resourceDir, QDir::Files, QDirIterator::Subdirectories);
    int discovered = 0;
    while (it.hasNext()) {
        const QString path = it.next();
        if (ImageUtils::isSupportedImageFile(path)) {
            files << path;
            ++discovered;
            if (discovered == 1 || discovered % 200 == 0)
                onProgress(discovered, QFileInfo(path).fileName());
        }
    }
    return files;
}

ImageRecord readImageMetadata(const QString& path, const QDir& root)
{
    QFileInfo fi(path);
    QImageReader reader(path);
    const QSize size = reader.size();
    const QImage image = reader.read();

    ImageRecord record;
    record.absolutePath = fi.absoluteFilePath();
    record.relativePath = QDir::toNativeSeparators(root.relativeFilePath(fi.absoluteFilePath()));
    record.fileName = fi.fileName();
    record.fileStem = fi.completeBaseName();
    record.parentDir = QDir::toNativeSeparators(root.relativeFilePath(fi.absolutePath()));
    record.extension = fi.suffix().toLower();
    record.fileSize = fi.size();
    record.modifiedTime = fi.lastModified().toSecsSinceEpoch();
    record.width = size.width();
    record.height = size.height();
    record.hasAlpha = !image.isNull() && image.hasAlphaChannel();
    record.imageFormat = QString::fromLatin1(reader.format()).toLower();
    return record;
}

void publishProgress(
    const QPointer<ProjectScanner>& scanner,
    int current,
    int total,
    const QString& path)
{
    if (!scanner)
        return;
    QMetaObject::invokeMethod(scanner, [scanner, current, total, path] {
        if (scanner)
            emit scanner->progress(current, total, path);
    }, Qt::QueuedConnection);
}

QVector<ImageRecord> scanImages(const QString& resourceDir, const QPointer<ProjectScanner>& scanner)
{
    publishProgress(scanner, 0, 0, QStringLiteral("开始枚举"));
    const QVector<QString> files = enumerateImageFiles(resourceDir, [scanner](int discovered, const QString& fileName) {
        publishProgress(scanner, discovered, 0, fileName);
    });
    publishProgress(scanner, 0, files.size(), QStringLiteral("读取图片元数据"));

    QVector<ImageRecord> records;
    records.reserve(files.size());
    QDir root(resourceDir);
    for (int i = 0; i < files.size(); ++i) {
        const ImageRecord record = readImageMetadata(files.at(i), root);
        records << record;
        if (i == 0 || i % 25 == 0 || i + 1 == files.size())
            publishProgress(scanner, i + 1, files.size(), record.relativePath);
    }
    publishProgress(scanner, files.size(), files.size(), QString());
    return records;
}
}

ProjectScanner::ProjectScanner(ImageRepository* repository, QObject* parent)
    : QObject(parent)
    , m_repository(repository)
{
}

void ProjectScanner::scan(const QString& resourceDir)
{
    if (m_running) {
        emit failed(QStringLiteral("已有扫描任务正在运行。"));
        return;
    }
    if (!m_repository) {
        emit failed(QStringLiteral("ImageRepository 未初始化"));
        return;
    }
    m_running = true;
    emit progress(0, 0, QStringLiteral("准备扫描"));

    auto* watcher = new QFutureWatcher<QVector<ImageRecord>>(this);
    connect(watcher, &QFutureWatcher<QVector<ImageRecord>>::finished, this, [this, watcher]() {
        const QVector<ImageRecord> records = watcher->result();
        watcher->deleteLater();
        finishScan(records);
    });

    QPointer<ProjectScanner> scanner(this);
    auto future = QtConcurrent::run([scanner, resourceDir]() {
        return scanImages(resourceDir, scanner);
    });
    watcher->setFuture(future);
}

void ProjectScanner::finishScan(const QVector<ImageRecord>& records)
{
    if (!m_repository->upsertImages(records)) {
        m_running = false;
        emit failed(m_repository->lastError());
        return;
    }
    m_running = false;
    emit finished(records.size());
}
