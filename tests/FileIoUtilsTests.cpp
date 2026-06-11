#include "utils/FileIoUtils.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class FileIoUtilsTests : public QObject {
    Q_OBJECT

private slots:
    void writeAllAndReadAllRoundTrip();
    void readAllFailureClearsOutputAndReportsError();
    void writeAllFailureReportsError();
};

void FileIoUtilsTests::writeAllAndReadAllRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString path = QDir(temp.path()).filePath(QStringLiteral("rules.json"));
    const QByteArray content = R"({"format":"imgmgr.rules","rules":[]})";

    QString error;
    QVERIFY2(FileIoUtils::writeAll(path, content, &error), qPrintable(error));

    QByteArray readBack;
    QVERIFY2(FileIoUtils::readAll(path, &readBack, &error), qPrintable(error));
    QCOMPARE(readBack, content);
}

void FileIoUtilsTests::readAllFailureClearsOutputAndReportsError()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString missingPath = QDir(temp.path()).filePath(QStringLiteral("missing.json"));
    QByteArray data = QByteArrayLiteral("stale");
    QString error;

    QVERIFY(!FileIoUtils::readAll(missingPath, &data, &error));
    QVERIFY(data.isEmpty());
    QVERIFY(!error.isEmpty());
}

void FileIoUtilsTests::writeAllFailureReportsError()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString directoryPath = QDir(temp.path()).filePath(QStringLiteral("existing_dir"));
    QVERIFY(QDir().mkpath(directoryPath));

    QString error;
    QVERIFY(!FileIoUtils::writeAll(directoryPath, QByteArrayLiteral("content"), &error));
    QVERIFY(!error.isEmpty());
}

QTEST_APPLESS_MAIN(FileIoUtilsTests)

#include "FileIoUtilsTests.moc"
