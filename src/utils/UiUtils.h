#pragma once

#include <QColor>

class QButtonGroup;
class QComboBox;
class QHBoxLayout;
class QWidget;

namespace UiUtils {

struct BackgroundPreset {
    QColor color;
    bool checkerboard = false;
};

void addBackgroundPresetRadios(QHBoxLayout* layout, QButtonGroup* group, QWidget* parent);
void addVerticalSeparator(QHBoxLayout* layout, QWidget* parent, int spacing = 6);
BackgroundPreset backgroundPresetForId(int id);
void populateMatchTargetCombo(QComboBox* combo);

}
