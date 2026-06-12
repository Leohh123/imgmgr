#include "database/SqlUtils.h"
#include "TestDbUtils.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtTest/QtTest>

class SqlUtilsTests : public QObject {
    Q_OBJECT

private slots:
    void execRunsSqlTextAndPreparedStatements();
    void execFailureReportsError();
    void runStatementsStopsOnFirstFailure();
};

void SqlUtilsTests::execRunsSqlTextAndPreparedStatements()
{
    const QString connectionName = TestDbUtils::uniqueConnectionName(QStringLiteral("sql_utils_test"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(db.open());

        QString error;
        {
            QSqlQuery query(db);
            QVERIFY2(SqlUtils::exec(&query, QStringLiteral("CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT)"), &error),
                qPrintable(error));

            query.prepare(QStringLiteral("INSERT INTO items (name) VALUES (?)"));
            query.addBindValue(QStringLiteral("hero"));
            QVERIFY2(SqlUtils::exec(&query, &error), qPrintable(error));

            QVERIFY2(SqlUtils::exec(&query, QStringLiteral("SELECT COUNT(*) FROM items"), &error), qPrintable(error));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 1);
        }
        db.close();
    }
}

void SqlUtilsTests::execFailureReportsError()
{
    const QString connectionName = TestDbUtils::uniqueConnectionName(QStringLiteral("sql_utils_test"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(db.open());

        QString error;
        {
            QSqlQuery query(db);
            QVERIFY(!SqlUtils::exec(&query, QStringLiteral("SELECT * FROM missing_table"), &error));
            QVERIFY(!error.isEmpty());
        }
        db.close();
    }
}

void SqlUtilsTests::runStatementsStopsOnFirstFailure()
{
    const QString connectionName = TestDbUtils::uniqueConnectionName(QStringLiteral("sql_utils_test"));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(db.open());

        QString error;
        {
            QSqlQuery query(db);
            const QStringList statements = {
                QStringLiteral("CREATE TABLE first_table (id INTEGER PRIMARY KEY)"),
                QStringLiteral("INSERT INTO missing_table VALUES (1)"),
                QStringLiteral("CREATE TABLE second_table (id INTEGER PRIMARY KEY)")
            };

            QVERIFY(!SqlUtils::runStatements(&query, statements, &error));
            QVERIFY(!error.isEmpty());

            QVERIFY2(SqlUtils::exec(&query, QStringLiteral(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='first_table'"), &error), qPrintable(error));
            QVERIFY(query.next());

            QVERIFY2(SqlUtils::exec(&query, QStringLiteral(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='second_table'"), &error), qPrintable(error));
            QVERIFY(!query.next());
        }
        db.close();
    }
}

QTEST_MAIN(SqlUtilsTests)

#include "SqlUtilsTests.moc"
