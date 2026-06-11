#include "utils/ImageUtils.h"

#include <QtTest/QtTest>

class ImageUtilsTests : public QObject {
    Q_OBJECT

private slots:
    void supportedImageFileIsCaseInsensitive();
    void unsupportedImageFileReturnsFalse();
    void channelViewKeepsSelectedChannels();
    void alphaOnlyChannelViewRendersAlphaAsGray();
    void nullImageChannelViewReturnsNull();
};

void ImageUtilsTests::supportedImageFileIsCaseInsensitive()
{
    QVERIFY(ImageUtils::isSupportedImageFile(QStringLiteral("assets/Preview.PNG")));
    QVERIFY(ImageUtils::isSupportedImageFile(QStringLiteral("assets/photo.JpEg")));
}

void ImageUtilsTests::unsupportedImageFileReturnsFalse()
{
    QVERIFY(!ImageUtils::isSupportedImageFile(QStringLiteral("assets/readme.txt")));
}

void ImageUtilsTests::channelViewKeepsSelectedChannels()
{
    QImage source(1, 1, QImage::Format_ARGB32);
    source.setPixel(0, 0, qRgba(10, 20, 30, 40));

    const QImage redBlue = ImageUtils::buildChannelView(source, true, false, true, false);
    const QRgb pixel = redBlue.pixel(0, 0);
    QCOMPARE(qRed(pixel), 10);
    QCOMPARE(qGreen(pixel), 0);
    QCOMPARE(qBlue(pixel), 30);
    QCOMPARE(qAlpha(pixel), 255);
}

void ImageUtilsTests::alphaOnlyChannelViewRendersAlphaAsGray()
{
    QImage source(1, 1, QImage::Format_ARGB32);
    source.setPixel(0, 0, qRgba(10, 20, 30, 40));

    const QImage alpha = ImageUtils::buildChannelView(source, false, false, false, true);
    const QRgb pixel = alpha.pixel(0, 0);
    QCOMPARE(qRed(pixel), 40);
    QCOMPARE(qGreen(pixel), 40);
    QCOMPARE(qBlue(pixel), 40);
    QCOMPARE(qAlpha(pixel), 255);
}

void ImageUtilsTests::nullImageChannelViewReturnsNull()
{
    QVERIFY(ImageUtils::buildChannelView({}, true, true, true, true).isNull());
}

QTEST_APPLESS_MAIN(ImageUtilsTests)

#include "ImageUtilsTests.moc"
