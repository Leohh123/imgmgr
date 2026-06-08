#pragma once

#include <QImage>
#include <QStringList>

namespace ImageUtils {
QStringList supportedExtensions();
bool isSupportedImageFile(const QString& path);
QImage buildChannelView(const QImage& source, bool showR, bool showG, bool showB, bool showA);
}
