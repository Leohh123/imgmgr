#pragma once

#include "database/ImageRepository.h"

#include <QObject>
#include <QFutureWatcher>

class ProjectScanner : public QObject {
    Q_OBJECT
public:
    explicit ProjectScanner(ImageRepository* repository, QObject* parent = nullptr);

public slots:
    void scan(const QString& resourceDir);

signals:
    void progress(int current, int total, const QString& path);
    void finished(int imageCount);
    void failed(const QString& error);

private:
    void finishScan(const QVector<ImageRecord>& records);

    ImageRepository* m_repository = nullptr;
    bool m_running = false;
};
