#pragma once

#include <QSqlDatabase>
#include <QString>

class QSqlQuery;

class DatabaseManager {
public:
    explicit DatabaseManager(QString connectionName = QStringLiteral("imgmgr_main"));
    ~DatabaseManager();

    bool openProject(const QString& dbPath);
    bool initialize();
    void close();
    QSqlDatabase db() const;
    QString databasePath() const { return m_dbPath; }
    QString lastError() const { return m_lastError; }

private:
    bool initializeSchema(QSqlQuery* query);
    bool runMigrations(QSqlQuery* query);
    bool backfillDerivedFields(QSqlQuery* query);

    QString m_connectionName;
    QString m_dbPath;
    QString m_lastError;
};
