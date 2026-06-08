#include "models/RuleTreeModel.h"

#include <QBrush>
#include <QHash>

RuleTreeModel::RuleTreeModel(RuleRepository* repository, QObject* parent)
    : QAbstractItemModel(parent)
    , m_repository(repository)
    , m_root(std::make_unique<RuleNode>())
{
}

QModelIndex RuleTreeModel::index(int row, int column, const QModelIndex& parentIndex) const
{
    if (!hasIndex(row, column, parentIndex))
        return {};
    RuleNode* parentNode = nodeFromIndex(parentIndex);
    if (!parentNode || row >= parentNode->children.size())
        return {};
    return createIndex(row, column, parentNode->children[row].get());
}

QModelIndex RuleTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return {};
    RuleNode* node = nodeFromIndex(child);
    RuleNode* parentNode = node ? node->parent : nullptr;
    if (!parentNode || parentNode == m_root.get())
        return {};
    RuleNode* grand = parentNode->parent ? parentNode->parent : m_root.get();
    for (int row = 0; row < grand->children.size(); ++row) {
        if (grand->children[row].get() == parentNode)
            return createIndex(row, 0, parentNode);
    }
    return {};
}

int RuleTreeModel::rowCount(const QModelIndex& parentIndex) const
{
    RuleNode* node = nodeFromIndex(parentIndex);
    return node ? node->children.size() : 0;
}

int RuleTreeModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)
    return ColumnCount;
}

QVariant RuleTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};
    const RuleRecord& r = nodeFromIndex(index)->rule;
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case NameColumn: return r.name;
        case TypeColumn: return r.ruleType;
        case PatternColumn: return r.pattern;
        case TargetColumn: return r.matchTarget;
        case CaseSensitiveColumn: return r.caseSensitive ? QStringLiteral("是") : QStringLiteral("否");
        case WholeMatchColumn: return r.wholeMatch ? QStringLiteral("是") : QStringLiteral("否");
        case EnabledColumn: return r.enabled ? QStringLiteral("启用") : QStringLiteral("禁用");
        case MatchCountColumn: return r.matchCount;
        case ConflictCountColumn: return r.conflictCount;
        case PriorityColumn: return r.priority;
        default: return {};
        }
    }
    if (role == Qt::ForegroundRole && !r.enabled)
        return QBrush(Qt::gray);
    if (role == Qt::UserRole)
        return r.id;
    return {};
}

QVariant RuleTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case NameColumn: return QStringLiteral("规则名称");
    case TypeColumn: return QStringLiteral("类型");
    case PatternColumn: return QStringLiteral("规则内容");
    case TargetColumn: return QStringLiteral("匹配目标");
    case CaseSensitiveColumn: return QStringLiteral("区分大小写");
    case WholeMatchColumn: return QStringLiteral("全字匹配");
    case EnabledColumn: return QStringLiteral("状态");
    case MatchCountColumn: return QStringLiteral("命中");
    case ConflictCountColumn: return QStringLiteral("冲突");
    case PriorityColumn: return QStringLiteral("优先级");
    default: return {};
    }
}

void RuleTreeModel::reload()
{
    beginResetModel();
    m_root = std::make_unique<RuleNode>();
    QHash<int, RuleNode*> byId;
    byId.insert(0, m_root.get());
    QVector<RuleRecord> rules = m_repository ? m_repository->fetchRules(false) : QVector<RuleRecord>();

    bool attached = true;
    while (attached && !rules.isEmpty()) {
        attached = false;
        for (int i = rules.size() - 1; i >= 0; --i) {
            RuleRecord rule = rules.at(i);
            RuleNode* parentNode = byId.value(rule.parentId, nullptr);
            if (!parentNode)
                continue;
            auto node = std::make_unique<RuleNode>();
            node->rule = rule;
            node->parent = parentNode;
            RuleNode* raw = node.get();
            parentNode->children.push_back(std::move(node));
            byId.insert(rule.id, raw);
            rules.removeAt(i);
            attached = true;
        }
    }
    for (const auto& rule : rules) {
        auto node = std::make_unique<RuleNode>();
        node->rule = rule;
        node->parent = m_root.get();
        m_root->children.push_back(std::move(node));
    }
    endResetModel();
}

RuleRecord RuleTreeModel::ruleForIndex(const QModelIndex& index) const
{
    RuleNode* node = nodeFromIndex(index);
    return node ? node->rule : RuleRecord();
}

RuleNode* RuleTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    return index.isValid() ? static_cast<RuleNode*>(index.internalPointer()) : m_root.get();
}
