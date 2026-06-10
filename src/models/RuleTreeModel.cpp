#include "models/RuleTreeModel.h"

#include <QBrush>
#include <QHash>

namespace {
QVariant displayValueForColumn(const RuleRecord& rule, int column)
{
    switch (column) {
    case RuleTreeModel::NameColumn: return rule.name;
    case RuleTreeModel::TypeColumn: return rule.ruleType;
    case RuleTreeModel::PatternColumn: return rule.pattern;
    case RuleTreeModel::TargetColumn: return rule.matchTarget;
    case RuleTreeModel::CaseSensitiveColumn: return rule.caseSensitive ? QStringLiteral("是") : QStringLiteral("否");
    case RuleTreeModel::WholeMatchColumn: return rule.wholeMatch ? QStringLiteral("是") : QStringLiteral("否");
    case RuleTreeModel::EnabledColumn: return rule.enabled ? QStringLiteral("启用") : QStringLiteral("禁用");
    case RuleTreeModel::MatchCountColumn: return rule.matchCount;
    case RuleTreeModel::ConflictCountColumn: return rule.conflictCount;
    case RuleTreeModel::PriorityColumn: return rule.priority;
    default: return {};
    }
}

QString headerTitleForColumn(int section)
{
    switch (section) {
    case RuleTreeModel::NameColumn: return QStringLiteral("规则名称");
    case RuleTreeModel::TypeColumn: return QStringLiteral("类型");
    case RuleTreeModel::PatternColumn: return QStringLiteral("规则内容");
    case RuleTreeModel::TargetColumn: return QStringLiteral("匹配目标");
    case RuleTreeModel::CaseSensitiveColumn: return QStringLiteral("区分大小写");
    case RuleTreeModel::WholeMatchColumn: return QStringLiteral("全字匹配");
    case RuleTreeModel::EnabledColumn: return QStringLiteral("状态");
    case RuleTreeModel::MatchCountColumn: return QStringLiteral("命中");
    case RuleTreeModel::ConflictCountColumn: return QStringLiteral("冲突");
    case RuleTreeModel::PriorityColumn: return QStringLiteral("优先级");
    default: return {};
    }
}

std::unique_ptr<RuleNode> makeNode(const RuleRecord& rule, RuleNode* parent)
{
    auto node = std::make_unique<RuleNode>();
    node->rule = rule;
    node->parent = parent;
    return node;
}

RuleNode* appendChildNode(RuleNode* parent, std::unique_ptr<RuleNode> node)
{
    RuleNode* raw = node.get();
    parent->children.push_back(std::move(node));
    return raw;
}

bool attachRulesWithAvailableParents(QVector<RuleRecord>* rules, QHash<int, RuleNode*>* nodesById)
{
    bool attachedAny = false;
    for (int i = 0; i < rules->size(); ++i) {
        const RuleRecord rule = rules->at(i);
        RuleNode* parentNode = nodesById->value(rule.parentId, nullptr);
        if (!parentNode)
            continue;

        RuleNode* childNode = appendChildNode(parentNode, makeNode(rule, parentNode));
        nodesById->insert(rule.id, childNode);
        rules->removeAt(i);
        --i;
        attachedAny = true;
    }
    return attachedAny;
}

void attachOrphanRulesToRoot(const QVector<RuleRecord>& rules, RuleNode* root)
{
    for (const RuleRecord& rule : rules)
        appendChildNode(root, makeNode(rule, root));
}
}

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
    if (role == Qt::DisplayRole)
        return displayValueForColumn(r, index.column());
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
    return headerTitleForColumn(section);
}

void RuleTreeModel::reload()
{
    beginResetModel();
    m_root = std::make_unique<RuleNode>();
    QHash<int, RuleNode*> byId;
    byId.insert(0, m_root.get());
    QVector<RuleRecord> rules = m_repository ? m_repository->fetchRules(false) : QVector<RuleRecord>();

    while (!rules.isEmpty()) {
        if (!attachRulesWithAvailableParents(&rules, &byId))
            break;
    }
    attachOrphanRulesToRoot(rules, m_root.get());
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
