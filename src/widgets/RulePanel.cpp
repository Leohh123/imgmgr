#include "widgets/RulePanel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

RulePanel::RulePanel(RuleTreeModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    m_tree = new QTreeView(this);
    m_tree->setModel(m_model);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tree->viewport()->installEventFilter(this);

    auto* edit = new QPushButton(QStringLiteral("编辑"), this);
    auto* remove = new QPushButton(QStringLiteral("删除"), this);
    auto* toggle = new QPushButton(QStringLiteral("启用/禁用"), this);
    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(edit);
    toolbar->addWidget(remove);
    toolbar->addWidget(toggle);
    toolbar->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(m_tree);

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        if (m_model && current.isValid())
            emit ruleSelected(m_model->ruleForIndex(current));
    });

    connect(edit, &QPushButton::clicked, this, [this] {
        const RuleRecord rule = currentRule();
        if (rule.id > 0)
            emit editRuleRequested(rule);
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        const RuleRecord rule = currentRule();
        if (rule.id > 0)
            emit deleteRuleRequested(rule);
    });
    connect(toggle, &QPushButton::clicked, this, [this] {
        const RuleRecord rule = currentRule();
        if (rule.id > 0)
            emit toggleRuleRequested(rule);
    });
}

bool RulePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_tree->viewport() && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton && !m_tree->indexAt(mouseEvent->pos()).isValid()) {
            m_tree->clearSelection();
            m_tree->setCurrentIndex(QModelIndex());
            emit ruleSelectionCleared();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void RulePanel::reload()
{
    if (m_model)
        m_model->reload();
    m_tree->expandAll();
}

RuleRecord RulePanel::currentRule() const
{
    if (!m_model)
        return {};
    return m_model->ruleForIndex(m_tree->currentIndex());
}
