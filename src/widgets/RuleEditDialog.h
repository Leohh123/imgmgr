#pragma once

#include "types.h"

#include <QDialog>
#include <QHash>
#include <QSet>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

class RuleEditDialog : public QDialog {
    Q_OBJECT
public:
    RuleEditDialog(const RuleRecord& rule,
        const QVector<RuleRecord>& allRules,
        const QSet<int>& invalidParentIds,
        const QString& title,
        QWidget* parent = nullptr);

    RuleRecord rule() const { return m_rule; }

public slots:
    void accept() override;

private:
    void createControls(const QVector<RuleRecord>& allRules, const QSet<int>& invalidParentIds);
    void buildLayout();
    QString rulePath(int ruleId) const;

    RuleRecord m_rule;
    QHash<int, RuleRecord> m_rulesById;
    QLineEdit* m_name = nullptr;
    QComboBox* m_parent = nullptr;
    QLineEdit* m_pattern = nullptr;
    QComboBox* m_type = nullptr;
    QComboBox* m_target = nullptr;
    QSpinBox* m_priority = nullptr;
    QCheckBox* m_enabled = nullptr;
    QCheckBox* m_allowConflict = nullptr;
    QCheckBox* m_caseSensitive = nullptr;
    QCheckBox* m_wholeMatch = nullptr;
    QLineEdit* m_note = nullptr;
};
