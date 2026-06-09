#pragma once

#include "types.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

class FilterPanel : public QWidget {
    Q_OBJECT
public:
    explicit FilterPanel(QWidget* parent = nullptr);
    ImageFilter filter() const;
    void setRule(const RuleRecord& rule, bool bindRuleMatch = true);
    void clearRuleBinding();
    void setChildRuleEnabled(bool enabled);

signals:
    void filterRequested(const ImageFilter& filter);
    void clearRequested();
    void addTopRuleRequested(const RuleRecord& rule);
    void addChildRuleRequested(const RuleRecord& rule);

private:
    QString ruleType() const;
    RuleRecord ruleFromInputs() const;
    void setStatusFiltersChecked(bool checked);
    void updateStatusToggleText();

    QRadioButton* m_globType = nullptr;
    QRadioButton* m_regexType = nullptr;
    QComboBox* m_target = nullptr;
    QLineEdit* m_ruleName = nullptr;
    QLineEdit* m_pattern = nullptr;
    QPushButton* m_addChildButton = nullptr;
    QPushButton* m_statusToggleButton = nullptr;
    QCheckBox* m_classified = nullptr;
    QCheckBox* m_unclassified = nullptr;
    QCheckBox* m_conflict = nullptr;
    QCheckBox* m_multi = nullptr;
    QCheckBox* m_onlyCurrentRule = nullptr;
    QCheckBox* m_caseSensitive = nullptr;
    QCheckBox* m_wholeMatch = nullptr;
    int m_currentRuleId = 0;
};
