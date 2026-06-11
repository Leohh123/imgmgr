#include "utils/HashUtils.h"

#include <QtTest/QtTest>

class HashUtilsTests : public QObject {
    Q_OBJECT

private slots:
    void sha1HexMatchesKnownDigest();
    void sha1HexIsDeterministic();
};

void HashUtilsTests::sha1HexMatchesKnownDigest()
{
    QCOMPARE(HashUtils::sha1Hex(QStringLiteral("abc")),
        QStringLiteral("a9993e364706816aba3e25717850c26c9cd0d89d"));
}

void HashUtilsTests::sha1HexIsDeterministic()
{
    const QString text = QStringLiteral("图片分类规则");
    QCOMPARE(HashUtils::sha1Hex(text), HashUtils::sha1Hex(text));
    QCOMPARE(HashUtils::sha1Hex(text).size(), 40);
}

QTEST_APPLESS_MAIN(HashUtilsTests)

#include "HashUtilsTests.moc"
