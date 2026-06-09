#include "widgets/FilterPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

FilterPanel::FilterPanel(QWidget* parent)
    : QWidget(parent)
{
    m_globType = new QRadioButton(QStringLiteral("通配符"), this);
    m_regexType = new QRadioButton(QStringLiteral("正则表达式"), this);
    m_globType->setChecked(true);
    m_caseSensitive = new QCheckBox(QStringLiteral("区分大小写"), this);
    m_wholeMatch = new QCheckBox(QStringLiteral("全字匹配"), this);
    m_wholeMatch->setChecked(true);
    auto* typeSeparator = new QFrame(this);
    typeSeparator->setFrameShape(QFrame::VLine);
    typeSeparator->setFrameShadow(QFrame::Sunken);
    auto* statusSeparator = new QFrame(this);
    statusSeparator->setFrameShape(QFrame::VLine);
    statusSeparator->setFrameShadow(QFrame::Sunken);
    m_statusToggleButton = new QPushButton(QStringLiteral("全选"), this);
    m_statusToggleButton->setFixedWidth(56);
    m_classified = new QCheckBox(QStringLiteral("已分类"), this);
    m_unclassified = new QCheckBox(QStringLiteral("未分类"), this);
    m_conflict = new QCheckBox(QStringLiteral("冲突"), this);
    m_multi = new QCheckBox(QStringLiteral("多重命中"), this);
    m_onlyCurrentRule = new QCheckBox(QStringLiteral("仅当前规则"), this);
    m_onlyCurrentRule->setEnabled(false);
    setStatusFiltersChecked(true);
    auto* typeLayout = new QHBoxLayout;
    typeLayout->addWidget(m_globType);
    typeLayout->addWidget(m_regexType);
    typeLayout->addSpacing(8);
    typeLayout->addWidget(typeSeparator);
    typeLayout->addSpacing(8);
    typeLayout->addWidget(m_caseSensitive);
    typeLayout->addWidget(m_wholeMatch);
    typeLayout->addStretch();

    auto* statusLayout = new QHBoxLayout;
    statusLayout->addWidget(m_statusToggleButton);
    statusLayout->addWidget(m_classified);
    statusLayout->addWidget(m_unclassified);
    statusLayout->addWidget(m_conflict);
    statusLayout->addWidget(m_multi);
    statusLayout->addSpacing(8);
    statusLayout->addWidget(statusSeparator);
    statusLayout->addSpacing(8);
    statusLayout->addWidget(m_onlyCurrentRule);
    statusLayout->addStretch();

    m_target = new QComboBox(this);
    m_target->addItem(QStringLiteral("文件名（无后缀）"), "filename_stem");
    m_target->addItem(QStringLiteral("文件名（有后缀）"), "filename");
    m_target->addItem(QStringLiteral("相对路径"), "relative_path");
    m_target->addItem(QStringLiteral("完整路径"), "absolute_path");
    m_target->addItem(QStringLiteral("父目录"), "parent_dir");

    m_ruleName = new QLineEdit(this);
    m_ruleName->setPlaceholderText(QStringLiteral("例如：按钮"));
    m_pattern = new QLineEdit(this);
    m_pattern->setPlaceholderText(QStringLiteral("btn_*.png 或 btn_\\w+\\.png"));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("规则类型"), typeLayout);
    form->addRow(QStringLiteral("状态筛选"), statusLayout);
    form->addRow(QStringLiteral("匹配目标"), m_target);
    form->addRow(QStringLiteral("规则名称"), m_ruleName);
    form->addRow(QStringLiteral("规则内容"), m_pattern);

    auto* filterButton = new QPushButton(QStringLiteral("筛选"), this);
    auto* clearButton = new QPushButton(QStringLiteral("清空筛选"), this);
    auto* addTopButton = new QPushButton(QStringLiteral("新增顶层规则"), this);
    m_addChildButton = new QPushButton(QStringLiteral("新增子规则"), this);
    m_addChildButton->setEnabled(false);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(filterButton);
    buttons->addWidget(clearButton);
    buttons->addWidget(addTopButton);
    buttons->addWidget(m_addChildButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);

    auto clearBoundRule = [this] { m_currentRuleId = 0; };
    connect(m_ruleName, &QLineEdit::textEdited, this, clearBoundRule);
    connect(m_pattern, &QLineEdit::textEdited, this, clearBoundRule);
    connect(m_globType, &QRadioButton::clicked, this, clearBoundRule);
    connect(m_regexType, &QRadioButton::clicked, this, clearBoundRule);
    connect(m_target, &QComboBox::activated, this, clearBoundRule);
    auto clearBoundRuleAndFilter = [this, clearBoundRule] {
        clearBoundRule();
        emit filterRequested(filter());
    };
    connect(m_caseSensitive, &QCheckBox::clicked, this, clearBoundRuleAndFilter);
    connect(m_wholeMatch, &QCheckBox::clicked, this, clearBoundRuleAndFilter);
    auto updateStatusControls = [this] { updateStatusToggleText(); };
    connect(m_classified, &QCheckBox::toggled, this, updateStatusControls);
    connect(m_unclassified, &QCheckBox::toggled, this, updateStatusControls);
    connect(m_conflict, &QCheckBox::toggled, this, updateStatusControls);
    connect(m_multi, &QCheckBox::toggled, this, updateStatusControls);
    auto filterOnStatusClick = [this] { emit filterRequested(filter()); };
    connect(m_classified, &QCheckBox::clicked, this, filterOnStatusClick);
    connect(m_unclassified, &QCheckBox::clicked, this, filterOnStatusClick);
    connect(m_conflict, &QCheckBox::clicked, this, filterOnStatusClick);
    connect(m_multi, &QCheckBox::clicked, this, filterOnStatusClick);
    connect(m_onlyCurrentRule, &QCheckBox::clicked, this, filterOnStatusClick);
    connect(m_statusToggleButton, &QPushButton::clicked, this, [this] {
        const bool allChecked = m_classified->isChecked()
            && m_unclassified->isChecked()
            && m_conflict->isChecked()
            && m_multi->isChecked();
        setStatusFiltersChecked(!allChecked);
        emit filterRequested(filter());
    });

    connect(filterButton, &QPushButton::clicked, this, [this] { emit filterRequested(filter()); });
    connect(m_pattern, &QLineEdit::returnPressed, this, [this] { emit filterRequested(filter()); });
    connect(clearButton, &QPushButton::clicked, this, [this] {
        m_ruleName->clear();
        m_pattern->clear();
        m_currentRuleId = 0;
        setStatusFiltersChecked(true);
        m_caseSensitive->setChecked(false);
        m_wholeMatch->setChecked(true);
        emit clearRequested();
    });
    connect(addTopButton, &QPushButton::clicked, this, [this] {
        emit addTopRuleRequested(ruleFromInputs());
    });
    connect(m_addChildButton, &QPushButton::clicked, this, [this] {
        emit addChildRuleRequested(ruleFromInputs());
    });
}

ImageFilter FilterPanel::filter() const
{
    ImageFilter f;
    f.pattern = m_pattern->text().trimmed();
    f.ruleType = ruleType();
    f.matchTarget = m_target->currentData().toString();
    f.onlyClassified = m_classified->isChecked();
    f.onlyUnclassified = m_unclassified->isChecked();
    f.onlyConflict = m_conflict->isChecked();
    f.onlyMultiMatch = m_multi->isChecked();
    f.currentRuleId = m_currentRuleId;
    f.onlyCurrentRule = m_onlyCurrentRule->isChecked();
    f.caseSensitive = m_caseSensitive->isChecked();
    f.wholeMatch = m_wholeMatch->isChecked();
    return f;
}

void FilterPanel::setRule(const RuleRecord& rule, bool bindRuleMatch)
{
    m_currentRuleId = bindRuleMatch ? rule.id : 0;
    m_ruleName->setText(rule.name);
    m_pattern->setText(rule.pattern);
    m_regexType->setChecked(rule.ruleType == "regex");
    m_globType->setChecked(rule.ruleType != "regex");
    m_target->setCurrentIndex(m_target->findData(rule.matchTarget));
    m_caseSensitive->setChecked(rule.caseSensitive);
    m_wholeMatch->setChecked(rule.wholeMatch);
}

void FilterPanel::clearRuleBinding()
{
    m_currentRuleId = 0;
}

void FilterPanel::setChildRuleEnabled(bool enabled)
{
    if (m_addChildButton)
        m_addChildButton->setEnabled(enabled);
    if (m_onlyCurrentRule)
        m_onlyCurrentRule->setEnabled(enabled);
}

QString FilterPanel::ruleType() const
{
    return m_regexType && m_regexType->isChecked() ? QStringLiteral("regex") : QStringLiteral("glob");
}

void FilterPanel::setStatusFiltersChecked(bool checked)
{
    const QSignalBlocker blockClassified(m_classified);
    const QSignalBlocker blockUnclassified(m_unclassified);
    const QSignalBlocker blockConflict(m_conflict);
    const QSignalBlocker blockMulti(m_multi);
    m_classified->setChecked(checked);
    m_unclassified->setChecked(checked);
    m_conflict->setChecked(checked);
    m_multi->setChecked(checked);
    updateStatusToggleText();
}

void FilterPanel::updateStatusToggleText()
{
    const bool allChecked = m_classified->isChecked()
        && m_unclassified->isChecked()
        && m_conflict->isChecked()
        && m_multi->isChecked();
    m_statusToggleButton->setText(allChecked ? QStringLiteral("重置") : QStringLiteral("全选"));
}

RuleRecord FilterPanel::ruleFromInputs() const
{
    RuleRecord rule;
    rule.name = m_ruleName->text().trimmed();
    rule.pattern = m_pattern->text().trimmed();
    rule.ruleType = ruleType();
    rule.matchTarget = m_target->currentData().toString();
    rule.caseSensitive = m_caseSensitive->isChecked();
    rule.wholeMatch = m_wholeMatch->isChecked();
    return rule;
}
