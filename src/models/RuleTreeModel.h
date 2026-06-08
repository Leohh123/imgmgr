#pragma once

#include "database/RuleRepository.h"

#include <QAbstractItemModel>
#include <memory>
#include <vector>

struct RuleNode {
    RuleRecord rule;
    RuleNode* parent = nullptr;
    std::vector<std::unique_ptr<RuleNode>> children;
};

class RuleTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { NameColumn, TypeColumn, PatternColumn, TargetColumn, CaseSensitiveColumn, WholeMatchColumn, EnabledColumn, MatchCountColumn, ConflictCountColumn, PriorityColumn, ColumnCount };

    explicit RuleTreeModel(RuleRepository* repository, QObject* parent = nullptr);
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void reload();
    RuleRecord ruleForIndex(const QModelIndex& index) const;

private:
    RuleNode* nodeFromIndex(const QModelIndex& index) const;

    RuleRepository* m_repository = nullptr;
    std::unique_ptr<RuleNode> m_root;
};
