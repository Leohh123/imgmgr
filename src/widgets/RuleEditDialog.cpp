#include "widgets/RuleEditDialog.h"

#include "utils/RuleUtils.h"
#include "utils/UiUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

RuleEditDialog::RuleEditDialog(const RuleRecord& rule,
    const QVector<RuleRecord>& allRules,
    const QSet<int>& invalidParentIds,
    const QString& title,
    QWidget* parent)
    : QDialog(parent)
    , m_rule(rule)
{
    setWindowTitle(title);
    for (const RuleRecord& item : allRules)
        m_rulesById.insert(item.id, item);
    createControls(allRules, invalidParentIds);
    buildLayout();
}

void RuleEditDialog::createControls(const QVector<RuleRecord>& allRules, const QSet<int>& invalidParentIds)
{
    m_name = new QLineEdit(m_rule.name, this);

    m_parent = new QComboBox(this);
    m_parent->addItem(QStringLiteral("无（顶层规则）"), 0);
    for (const RuleRecord& item : allRules) {
        if (invalidParentIds.contains(item.id))
            continue;
        m_parent->addItem(rulePath(item.id), item.id);
    }
    m_parent->setCurrentIndex(m_parent->findData(m_rule.parentId));
    if (m_parent->currentIndex() < 0)
        m_parent->setCurrentIndex(0);

    m_pattern = new QLineEdit(m_rule.pattern, this);

    m_type = new QComboBox(this);
    m_type->addItem(QStringLiteral("通配符"), RuleUtils::globRuleType());
    m_type->addItem(QStringLiteral("正则表达式"), RuleUtils::regexRuleType());
    m_type->setCurrentIndex(m_type->findData(m_rule.ruleType));
    if (m_type->currentIndex() < 0)
        m_type->setCurrentIndex(0);

    m_target = new QComboBox(this);
    UiUtils::populateMatchTargetCombo(m_target);
    m_target->setCurrentIndex(m_target->findData(m_rule.matchTarget));
    if (m_target->currentIndex() < 0)
        m_target->setCurrentIndex(0);

    m_priority = new QSpinBox(this);
    m_priority->setRange(-100000, 100000);
    m_priority->setValue(m_rule.priority);

    m_enabled = new QCheckBox(QStringLiteral("启用"), this);
    m_enabled->setChecked(m_rule.enabled);
    m_allowConflict = new QCheckBox(QStringLiteral("允许冲突"), this);
    m_allowConflict->setChecked(m_rule.allowConflict);
    m_caseSensitive = new QCheckBox(QStringLiteral("区分大小写"), this);
    m_caseSensitive->setChecked(m_rule.caseSensitive);
    m_wholeMatch = new QCheckBox(QStringLiteral("全字匹配"), this);
    m_wholeMatch->setChecked(m_rule.wholeMatch);
    m_note = new QLineEdit(m_rule.note, this);
}

void RuleEditDialog::buildLayout()
{
    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("规则名称"), m_name);
    form->addRow(QStringLiteral("父规则"), m_parent);
    form->addRow(QStringLiteral("规则内容"), m_pattern);
    form->addRow(QStringLiteral("规则类型"), m_type);
    form->addRow(QStringLiteral("匹配目标"), m_target);
    form->addRow(QStringLiteral("优先级"), m_priority);
    form->addRow(QString(), m_enabled);
    form->addRow(QString(), m_allowConflict);
    form->addRow(QString(), m_caseSensitive);
    form->addRow(QString(), m_wholeMatch);
    form->addRow(QStringLiteral("备注"), m_note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &RuleEditDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RuleEditDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QString RuleEditDialog::rulePath(int ruleId) const
{
    QStringList parts;
    QSet<int> seen;
    int current = ruleId;
    while (current != 0 && m_rulesById.contains(current) && !seen.contains(current)) {
        seen.insert(current);
        const RuleRecord item = m_rulesById.value(current);
        parts.prepend(item.name);
        current = item.parentId;
    }
    return parts.join(QStringLiteral(" / "));
}

void RuleEditDialog::accept()
{
    const QString ruleName = m_name->text().trimmed();
    const QString rulePattern = m_pattern->text().trimmed();
    if (ruleName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("规则名称为空"), QStringLiteral("请输入规则名称。"));
        return;
    }
    if (rulePattern.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("规则为空"), QStringLiteral("请输入规则内容。"));
        return;
    }
    if (m_type->currentData().toString() == RuleUtils::regexRuleType()) {
        const QRegularExpression re(rulePattern);
        if (!re.isValid()) {
            QMessageBox::warning(this, QStringLiteral("正则无效"), re.errorString());
            return;
        }
    }

    m_rule.name = ruleName;
    m_rule.parentId = m_parent->currentData().toInt();
    m_rule.pattern = rulePattern;
    m_rule.ruleType = m_type->currentData().toString();
    m_rule.matchTarget = m_target->currentData().toString();
    m_rule.priority = m_priority->value();
    m_rule.enabled = m_enabled->isChecked();
    m_rule.allowConflict = m_allowConflict->isChecked();
    m_rule.caseSensitive = m_caseSensitive->isChecked();
    m_rule.wholeMatch = m_wholeMatch->isChecked();
    m_rule.note = m_note->text();
    QDialog::accept();
}
