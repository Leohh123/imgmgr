#include "utils/PaintUtils.h"

#include <QPainter>

namespace PaintUtils {

void paintBackgroundPreset(QPainter* painter, const QRect& rect, const QColor& color, bool checkerboard)
{
    if (!checkerboard) {
        painter->fillRect(rect, color);
        return;
    }

    constexpr int cell = 12;
    const QColor light(238, 238, 238);
    const QColor dark(185, 185, 185);
    for (int y = rect.top(); y <= rect.bottom(); y += cell) {
        for (int x = rect.left(); x <= rect.right(); x += cell) {
            const bool alternate = ((x / cell) + (y / cell)) % 2;
            painter->fillRect(QRect(x, y, cell, cell).intersected(rect), alternate ? dark : light);
        }
    }
}

}
