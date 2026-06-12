#include "services/ProjectSession.h"

#include "models/ImageListModel.h"
#include "models/RuleTreeModel.h"
#include "services/ProjectScanner.h"
#include "services/RuleEngine.h"
#include "services/ThumbnailCache.h"

#include <QDir>
#include <QFileInfo>

ProjectSession::ProjectSession(QObject* parent)
    : QObject(parent)
{
}

ProjectSession::~ProjectSession()
{
    resetComponents();
}

bool ProjectSession::openProject(const QString& dbPath)
{
    if (!m_database.openProject(dbPath)) {
        resetComponents();
        return false;
    }

    m_projectDir = QFileInfo(dbPath).absolutePath();
    ++m_generation;
    m_images.setDatabase(m_database.db());
    m_rules.setDatabase(m_database.db());

    resetComponents();
    createComponents();
    return true;
}

void ProjectSession::resetComponents()
{
    delete m_thumbnails;
    m_thumbnails = nullptr;
    delete m_imageModel;
    m_imageModel = nullptr;
    delete m_ruleModel;
    m_ruleModel = nullptr;
    delete m_scanner;
    m_scanner = nullptr;
    delete m_ruleEngine;
    m_ruleEngine = nullptr;
}

void ProjectSession::createComponents()
{
    m_thumbnails = new ThumbnailCache(&m_images, this);
    m_thumbnails->setCacheDir(QDir(m_projectDir).filePath(".project_cache/thumbnails"));
    m_imageModel = new ImageListModel(&m_images, m_thumbnails, this);
    m_ruleModel = new RuleTreeModel(&m_rules, this);
    m_scanner = new ProjectScanner(&m_images, this);
    m_ruleEngine = new RuleEngine(&m_images, &m_rules, this);
}
