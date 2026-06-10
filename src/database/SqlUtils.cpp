#include "database/SqlUtils.h"

#include <QSqlError>
#include <QSqlQuery>

namespace SqlUtils {

bool exec(QSqlQuery* query, QString* lastError)
{
    if (query->exec())
        return true;
    if (lastError)
        *lastError = query->lastError().text();
    return false;
}

bool exec(QSqlQuery* query, const QString& sql, QString* lastError)
{
    if (query->exec(sql))
        return true;
    if (lastError)
        *lastError = query->lastError().text();
    return false;
}

bool runStatements(QSqlQuery* query, const QStringList& statements, QString* lastError)
{
    for (const QString& sql : statements) {
        if (!exec(query, sql, lastError))
            return false;
    }
    return true;
}

}
