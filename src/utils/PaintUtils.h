#pragma once

#include <QColor>
#include <QRect>

class QPainter;

namespace PaintUtils {
void paintBackgroundPreset(QPainter* painter, const QRect& rect, const QColor& color, bool checkerboard);
}
