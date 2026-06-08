#pragma once

#include "models/RuleTreeModel.h"

#include <QWidget>

class QTreeView;

class RulePanel : public QWidget {
    Q_OBJECT
public:
    explicit RulePanel(RuleTreeModel* model, QWidget* parent = nullptr);
    void reload();
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void ruleSelected(const RuleRecord& rule);
    void ruleSelectionCleared();
    void editRuleRequested(const RuleRecord& rule);
    void deleteRuleRequested(const RuleRecord& rule);
    void toggleRuleRequested(const RuleRecord& rule);

private:
    RuleRecord currentRule() const;

    RuleTreeModel* m_model = nullptr;
    QTreeView* m_tree = nullptr;
};
