#include "widgets/FilterPanel.h"

#include <QCheckBox>
#include <QtTest/QtTest>

class FilterPanelTests : public QObject {
    Q_OBJECT

private slots:
    void clearRuleContextClearsBoundRuleAndOnlyCurrentToggle();
};

namespace {
QCheckBox* findCheckBox(FilterPanel* panel, const QString& text)
{
    const QList<QCheckBox*> boxes = panel->findChildren<QCheckBox*>();
    for (QCheckBox* box : boxes) {
        if (box->text() == text)
            return box;
    }
    return nullptr;
}
}

void FilterPanelTests::clearRuleContextClearsBoundRuleAndOnlyCurrentToggle()
{
    FilterPanel panel;
    QCheckBox* onlyCurrentRule = findCheckBox(&panel, QStringLiteral("仅当前规则"));
    QVERIFY(onlyCurrentRule);

    RuleRecord rule;
    rule.id = 42;
    rule.name = QStringLiteral("按钮");
    rule.pattern = QStringLiteral("btn_*");
    panel.setRule(rule, true);
    panel.setChildRuleEnabled(true);
    onlyCurrentRule->setChecked(true);

    ImageFilter boundFilter = panel.filter();
    QCOMPARE(boundFilter.currentRuleId, rule.id);
    QVERIFY(boundFilter.onlyCurrentRule);
    QVERIFY(onlyCurrentRule->isEnabled());

    panel.clearRuleContext();

    ImageFilter clearedFilter = panel.filter();
    QCOMPARE(clearedFilter.currentRuleId, 0);
    QVERIFY(!clearedFilter.onlyCurrentRule);
    QVERIFY(!onlyCurrentRule->isEnabled());
}

QTEST_MAIN(FilterPanelTests)

#include "FilterPanelTests.moc"
