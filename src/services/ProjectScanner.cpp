#include "services/ProjectScanner.h"

#include "utils/ImageUtils.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QtConcurrent>

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
        if (!m_repository->upsertImages(records)) {
            m_running = false;
            emit failed(m_repository->lastError());
            return;
        }
        m_running = false;
        emit finished(records.size());
    });

    auto future = QtConcurrent::run([this, resourceDir]() {
        QVector<QString> files;
        QDirIterator it(resourceDir, QDir::Files, QDirIterator::Subdirectories);
        int discovered = 0;
        emit progress(0, 0, QStringLiteral("开始枚举"));
        while (it.hasNext()) {
            const QString path = it.next();
            if (ImageUtils::isSupportedImageFile(path)) {
                files << path;
                ++discovered;
                if (discovered == 1 || discovered % 200 == 0)
                    emit progress(discovered, 0, QFileInfo(path).fileName());
            }
        }
        emit progress(0, files.size(), QStringLiteral("读取图片元数据"));

        QVector<ImageRecord> records;
        records.reserve(files.size());
        QDir root(resourceDir);
        for (int i = 0; i < files.size(); ++i) {
            const QString path = files.at(i);
            QFileInfo fi(path);
            QImageReader reader(path);
            const QSize size = reader.size();
            ImageRecord r;
            r.absolutePath = fi.absoluteFilePath();
            r.relativePath = QDir::toNativeSeparators(root.relativeFilePath(fi.absoluteFilePath()));
            r.fileName = fi.fileName();
            r.fileStem = fi.completeBaseName();
            r.parentDir = QDir::toNativeSeparators(root.relativeFilePath(fi.absolutePath()));
            r.extension = fi.suffix().toLower();
            r.fileSize = fi.size();
            r.modifiedTime = fi.lastModified().toSecsSinceEpoch();
            r.width = size.width();
            r.height = size.height();
            r.hasAlpha = reader.supportsOption(QImageIOHandler::ImageFormat)
                ? QString::fromLatin1(reader.format()).compare("png", Qt::CaseInsensitive) == 0
                : false;
            r.imageFormat = QString::fromLatin1(reader.format()).toLower();
            records << r;
            if (i == 0 || i % 25 == 0 || i + 1 == files.size())
                emit progress(i + 1, files.size(), r.relativePath);
        }
        emit progress(files.size(), files.size(), QString());
        return records;
    });
    watcher->setFuture(future);
}
