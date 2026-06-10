#pragma once

#include <QString>
#include <QStringList>

class QSqlQuery;

namespace SqlUtils {

bool exec(QSqlQuery* query, QString* lastError);
bool exec(QSqlQuery* query, const QString& sql, QString* lastError);
bool runStatements(QSqlQuery* query, const QStringList& statements, QString* lastError);

}
