#include "utils/ImageUtils.h"

#include <QFileInfo>
#include <QImageReader>
#include <QSet>

QStringList ImageUtils::supportedExtensions()
{
    QStringList exts = { "png", "jpg", "jpeg", "webp", "bmp" };
    const auto formats = QImageReader::supportedImageFormats();
    if (formats.contains("tga"))
        exts << "tga";
    return exts;
}

bool ImageUtils::isSupportedImageFile(const QString& path)
{
    static const QSet<QString> exts = [] {
        QSet<QString> set;
        for (const QString& ext : ImageUtils::supportedExtensions())
            set.insert(ext);
        return set;
    }();
    return exts.contains(QFileInfo(path).suffix().toLower());
}

QImage ImageUtils::buildChannelView(const QImage& source, bool showR, bool showG, bool showB, bool showA)
{
    if (source.isNull())
        return {};

    QImage src = source.convertToFormat(QImage::Format_ARGB32);
    QImage out(src.size(), QImage::Format_ARGB32);

    for (int y = 0; y < src.height(); ++y) {
        const auto* inLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        auto* outLine = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const QRgb pixel = inLine[x];
            if (!showR && !showG && !showB && showA) {
                const int a = qAlpha(pixel);
                outLine[x] = qRgba(a, a, a, 255);
                continue;
            }
            const int r = showR ? qRed(pixel) : 0;
            const int g = showG ? qGreen(pixel) : 0;
            const int b = showB ? qBlue(pixel) : 0;
            const int a = showA ? qAlpha(pixel) : 255;
            outLine[x] = qRgba(r, g, b, a);
        }
    }
    return out;
}
