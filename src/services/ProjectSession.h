#pragma once

#include "database/DatabaseManager.h"
#include "database/ImageRepository.h"
#include "database/RuleRepository.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>

class ImageListModel;
class ProjectScanner;
class RuleEngine;
class RuleTreeModel;
class ThumbnailCache;

class ProjectSession : public QObject {
    Q_OBJECT
public:
    explicit ProjectSession(QObject* parent = nullptr);
    ~ProjectSession() override;

    bool openProject(const QString& dbPath);
    void resetComponents();

    bool isOpen() const { return m_database.db().isOpen(); }
    QSqlDatabase db() const { return m_database.db(); }
    QString databasePath() const { return m_database.databasePath(); }
    QString projectDir() const { return m_projectDir; }
    QString lastError() const { return m_database.lastError(); }
    quint64 generation() const { return m_generation; }

    ImageRepository& images() { return m_images; }
    const ImageRepository& images() const { return m_images; }
    RuleRepository& rules() { return m_rules; }
    const RuleRepository& rules() const { return m_rules; }
    ThumbnailCache* thumbnails() const { return m_thumbnails; }
    ImageListModel* imageModel() const { return m_imageModel; }
    RuleTreeModel* ruleModel() const { return m_ruleModel; }
    ProjectScanner* scanner() const { return m_scanner; }
    RuleEngine* ruleEngine() const { return m_ruleEngine; }

private:
    void createComponents();

    DatabaseManager m_database;
    ImageRepository m_images;
    RuleRepository m_rules;
    ThumbnailCache* m_thumbnails = nullptr;
    ImageListModel* m_imageModel = nullptr;
    RuleTreeModel* m_ruleModel = nullptr;
    ProjectScanner* m_scanner = nullptr;
    RuleEngine* m_ruleEngine = nullptr;
    QString m_projectDir;
    quint64 m_generation = 0;
};
