#include "database/SqlUtils.h"

#include <QSqlError>
#include <QSqlQuery>

namespace SqlUtils {

namespace {
QString formatError(const QSqlQuery& query, const QString& sqlContext)
{
    QStringList parts;
    parts << query.lastError().text();

    const QString sql = !sqlContext.isEmpty() ? sqlContext : query.lastQuery();
    if (!sql.isEmpty())
        parts << QStringLiteral("SQL: %1").arg(sql);

    const QString executedSql = query.executedQuery();
    if (!executedSql.isEmpty() && executedSql != sql)
        parts << QStringLiteral("Executed SQL: %1").arg(executedSql);

    return parts.join(QStringLiteral("\n"));
}

void writeLastError(const QSqlQuery& query, const QString& sqlContext, QString* lastError)
{
    if (lastError)
        *lastError = formatError(query, sqlContext);
}
}

bool exec(QSqlQuery* query, QString* lastError)
{
    if (query->exec())
        return true;
    writeLastError(*query, {}, lastError);
    return false;
}

bool exec(QSqlQuery* query, const QString& sql, QString* lastError)
{
    if (query->exec(sql))
        return true;
    writeLastError(*query, sql, lastError);
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
