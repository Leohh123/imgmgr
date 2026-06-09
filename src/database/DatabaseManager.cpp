#include "database/DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace {
bool isDuplicateColumnError(const QSqlError& error)
{
    return error.text().contains(QStringLiteral("duplicate column"), Qt::CaseInsensitive);
}
}

DatabaseManager::DatabaseManager(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::openProject(const QString& dbPath)
{
    close();
    QFileInfo info(dbPath);
    QDir().mkpath(info.absolutePath());

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(dbPath);
    if (!database.open()) {
        m_lastError = database.lastError().text();
        return false;
    }
    m_dbPath = dbPath;
    return initialize();
}

bool DatabaseManager::initialize()
{
    auto database = db();
    QSqlQuery q(database);
    const QStringList statements = {
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("PRAGMA synchronous=NORMAL"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS images ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "absolute_path TEXT NOT NULL UNIQUE,"
                       "relative_path TEXT NOT NULL,"
                       "file_name TEXT NOT NULL,"
                       "file_stem TEXT,"
                       "parent_dir TEXT,"
                       "extension TEXT,"
                       "file_size INTEGER,"
                       "modified_time INTEGER,"
                       "width INTEGER,"
                       "height INTEGER,"
                       "has_alpha INTEGER,"
                       "image_format TEXT,"
                       "thumbnail_path TEXT,"
                       "thumbnail_ready INTEGER DEFAULT 0,"
                       "file_hash TEXT,"
                       "perceptual_hash TEXT,"
                       "ignored INTEGER DEFAULT 0,"
                       "confirmed INTEGER DEFAULT 0,"
                       "created_at INTEGER,"
                       "updated_at INTEGER)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS rules ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "parent_id INTEGER,"
                       "name TEXT NOT NULL,"
                       "rule_type TEXT NOT NULL,"
                       "pattern TEXT NOT NULL,"
                       "match_target TEXT NOT NULL,"
                       "enabled INTEGER DEFAULT 1,"
                       "priority INTEGER DEFAULT 0,"
                       "allow_conflict INTEGER DEFAULT 0,"
                       "case_sensitive INTEGER DEFAULT 0,"
                       "whole_match INTEGER DEFAULT 1,"
                       "note TEXT,"
                       "created_at INTEGER,"
                       "updated_at INTEGER)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS rule_excludes ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "rule_id INTEGER NOT NULL,"
                       "rule_type TEXT NOT NULL,"
                       "pattern TEXT NOT NULL,"
                       "match_target TEXT NOT NULL,"
                       "enabled INTEGER DEFAULT 1)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS image_rule_matches ("
                       "image_id INTEGER NOT NULL,"
                       "rule_id INTEGER NOT NULL,"
                       "is_conflict INTEGER DEFAULT 0,"
                       "created_at INTEGER,"
                       "PRIMARY KEY (image_id, rule_id))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS tags ("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       "name TEXT NOT NULL UNIQUE,"
                       "color TEXT,"
                       "created_at INTEGER)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS image_tags ("
                       "image_id INTEGER NOT NULL,"
                       "tag_id INTEGER NOT NULL,"
                       "PRIMARY KEY (image_id, tag_id))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_images_relative_path ON images(relative_path)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_images_file_name ON images(file_name)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_images_file_stem ON images(file_stem)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_matches_image ON image_rule_matches(image_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_matches_rule ON image_rule_matches(rule_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_rules_parent ON rules(parent_id)")
    };

    for (const QString& sql : statements) {
        if (!q.exec(sql)) {
            m_lastError = q.lastError().text();
            return false;
        }
    }
    const QStringList migrations = {
        QStringLiteral("ALTER TABLE images ADD COLUMN file_stem TEXT"),
        QStringLiteral("ALTER TABLE rules ADD COLUMN case_sensitive INTEGER DEFAULT 0"),
        QStringLiteral("ALTER TABLE rules ADD COLUMN whole_match INTEGER DEFAULT 1")
    };
    for (const QString& sql : migrations) {
        if (!q.exec(sql) && !isDuplicateColumnError(q.lastError())) {
            m_lastError = q.lastError().text();
            return false;
        }
    }
    if (!q.exec(QStringLiteral("UPDATE images SET file_stem=substr(file_name, 1, length(file_name) - length(extension) - 1) "
                               "WHERE (file_stem IS NULL OR file_stem='') AND extension IS NOT NULL AND extension != ''"))) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

void DatabaseManager::close()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        auto database = QSqlDatabase::database(m_connectionName);
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QSqlDatabase DatabaseManager::db() const
{
    return QSqlDatabase::database(m_connectionName);
}
