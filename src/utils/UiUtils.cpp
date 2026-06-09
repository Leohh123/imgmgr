#include "utils/UiUtils.h"

#include "utils/RuleUtils.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QPalette>
#include <QRadioButton>

namespace {
void addBackgroundRadio(QHBoxLayout* layout, QButtonGroup* group, QWidget* parent, const QString& text, int id, bool checked = false)
{
    auto* button = new QRadioButton(text, parent);
    button->setChecked(checked);
    group->addButton(button, id);
    layout->addWidget(button);
}
}

namespace UiUtils {

void addBackgroundPresetRadios(QHBoxLayout* layout, QButtonGroup* group, QWidget* parent)
{
    addBackgroundRadio(layout, group, parent, QStringLiteral("棋盘格"), 0, true);
    addBackgroundRadio(layout, group, parent, QStringLiteral("系统"), 1);
    addBackgroundRadio(layout, group, parent, QStringLiteral("黑色"), 2);
    addBackgroundRadio(layout, group, parent, QStringLiteral("白色"), 3);
    addBackgroundRadio(layout, group, parent, QStringLiteral("灰色"), 4);
}

void addVerticalSeparator(QHBoxLayout* layout, QWidget* parent, int spacing)
{
    auto* separator = new QFrame(parent);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addSpacing(spacing);
    layout->addWidget(separator);
    layout->addSpacing(spacing);
}

BackgroundPreset backgroundPresetForId(int id)
{
    switch (id) {
    case 0: return { QApplication::palette().color(QPalette::Highlight), true };
    case 1: return { QApplication::palette().color(QPalette::Highlight), false };
    case 2: return { QColor(Qt::black), false };
    case 3: return { QColor(Qt::white), false };
    case 4: return { QColor(Qt::gray), false };
    default: return { QApplication::palette().color(QPalette::Highlight), true };
    }
}

void populateMatchTargetCombo(QComboBox* combo)
{
    combo->addItem(QStringLiteral("文件名（无后缀）"), RuleUtils::fileNameStemTarget());
    combo->addItem(QStringLiteral("文件名（有后缀）"), RuleUtils::fileNameTarget());
    combo->addItem(QStringLiteral("相对路径"), RuleUtils::relativePathTarget());
    combo->addItem(QStringLiteral("完整路径"), RuleUtils::absolutePathTarget());
    combo->addItem(QStringLiteral("父目录"), RuleUtils::parentDirTarget());
}

}
