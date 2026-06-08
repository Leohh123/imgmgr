#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QProgressBar>
#include <QRadioButton>
#include <QRegularExpression>
#include <QPainter>
#include <QSettings>
#include <QSplitter>
#include <QSpinBox>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTabWidget>
#include <QTextEdit>
#include <QSet>
#include <QVBoxLayout>

static void paintBackgroundPreset(QPainter* painter, const QRect& rect, const QColor& color, bool checkerboard)
{
    if (!checkerboard) {
        painter->fillRect(rect, color);
        return;
    }
    const int cell = 12;
    const QColor light(238, 238, 238);
    const QColor dark(185, 185, 185);
    for (int y = rect.top(); y <= rect.bottom(); y += cell) {
        for (int x = rect.left(); x <= rect.right(); x += cell) {
            const bool alternate = ((x / cell) + (y / cell)) % 2;
            painter->fillRect(QRect(x, y, cell, cell).intersected(rect), alternate ? dark : light);
        }
    }
}

class ThumbnailDelegate : public QStyledItemDelegate {
public:
    explicit ThumbnailDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (index.column() != ImageListModel::ThumbnailColumn) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text.clear();
        opt.icon = QIcon();
        QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        const QPixmap pixmap = index.data(Qt::DecorationRole).value<QPixmap>();
        QColor background = QApplication::palette().color(QPalette::Highlight);
        if (opt.widget) {
            const QVariant value = opt.widget->property("thumbnailBackgroundColor");
            if (value.canConvert<QColor>() && value.value<QColor>().isValid())
                background = value.value<QColor>();
        }
        const bool checkerboard = opt.widget && opt.widget->property("thumbnailCheckerboardBackground").toBool();
        paintBackgroundPreset(painter, option.rect.adjusted(1, 1, -1, -1), background, checkerboard);

        if (pixmap.isNull())
            return;

        const QRect content = option.rect.adjusted(0, 4, 0, -4);
        const QPoint topLeft(
            content.left() + (content.width() - pixmap.width()) / 2,
            content.top() + (content.height() - pixmap.height()) / 2);
        painter->drawPixmap(topLeft, pixmap);
    }
};

static QString imageColumnTitle(int column)
{
    switch (column) {
    case ImageListModel::ThumbnailColumn: return QStringLiteral("缩略图");
    case ImageListModel::FileNameColumn: return QStringLiteral("文件名");
    case ImageListModel::RelativePathColumn: return QStringLiteral("相对路径");
    case ImageListModel::SizeColumn: return QStringLiteral("图片尺寸");
    case ImageListModel::FileSizeColumn: return QStringLiteral("文件大小");
    case ImageListModel::MatchCountColumn: return QStringLiteral("命中规则数量");
    case ImageListModel::StatusColumn: return QStringLiteral("状态");
    default: return {};
    }
}

static bool isDefaultImageColumnVisible(int column)
{
    return column == ImageListModel::ThumbnailColumn
        || column == ImageListModel::FileNameColumn
        || column == ImageListModel::SizeColumn
        || column == ImageListModel::StatusColumn;
}

static void addBackgroundRadio(QHBoxLayout* layout, QButtonGroup* group, QWidget* parent, const QString& text, int id, bool checked = false)
{
    auto* button = new QRadioButton(text, parent);
    button->setChecked(checked);
    group->addButton(button, id);
    layout->addWidget(button);
}

static QJsonObject ruleToJson(const RuleRecord& rule)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), rule.id);
    object.insert(QStringLiteral("parent_id"), rule.parentId);
    object.insert(QStringLiteral("name"), rule.name);
    object.insert(QStringLiteral("rule_type"), rule.ruleType);
    object.insert(QStringLiteral("pattern"), rule.pattern);
    object.insert(QStringLiteral("match_target"), rule.matchTarget);
    object.insert(QStringLiteral("enabled"), rule.enabled);
    object.insert(QStringLiteral("priority"), rule.priority);
    object.insert(QStringLiteral("allow_conflict"), rule.allowConflict);
    object.insert(QStringLiteral("case_sensitive"), rule.caseSensitive);
    object.insert(QStringLiteral("whole_match"), rule.wholeMatch);
    object.insert(QStringLiteral("note"), rule.note);
    return object;
}

static bool jsonToRule(const QJsonObject& object, RuleRecord* rule, QString* error)
{
    const QStringList required = {
        QStringLiteral("id"),
        QStringLiteral("name"),
        QStringLiteral("rule_type"),
        QStringLiteral("pattern"),
        QStringLiteral("match_target")
    };
    for (const QString& key : required) {
        if (!object.contains(key)) {
            if (error)
                *error = QStringLiteral("规则缺少字段：%1").arg(key);
            return false;
        }
    }

    RuleRecord result;
    result.id = object.value(QStringLiteral("id")).toInt();
    result.parentId = object.value(QStringLiteral("parent_id")).toInt();
    result.name = object.value(QStringLiteral("name")).toString().trimmed();
    result.ruleType = object.value(QStringLiteral("rule_type")).toString();
    result.pattern = object.value(QStringLiteral("pattern")).toString().trimmed();
    result.matchTarget = object.value(QStringLiteral("match_target")).toString();
    result.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    result.priority = object.value(QStringLiteral("priority")).toInt();
    result.allowConflict = object.value(QStringLiteral("allow_conflict")).toBool(false);
    result.caseSensitive = object.value(QStringLiteral("case_sensitive")).toBool(false);
    result.wholeMatch = object.value(QStringLiteral("whole_match")).toBool(true);
    result.note = object.value(QStringLiteral("note")).toString();

    if (result.id <= 0) {
        if (error)
            *error = QStringLiteral("规则 ID 必须大于 0。");
        return false;
    }
    if (result.name.isEmpty() || result.pattern.isEmpty()) {
        if (error)
            *error = QStringLiteral("规则名称和规则内容不能为空。");
        return false;
    }
    if (result.ruleType != QStringLiteral("glob") && result.ruleType != QStringLiteral("regex")) {
        if (error)
            *error = QStringLiteral("规则类型无效：%1").arg(result.ruleType);
        return false;
    }
    const QSet<QString> targets = {
        QStringLiteral("filename_stem"),
        QStringLiteral("filename"),
        QStringLiteral("relative_path"),
        QStringLiteral("absolute_path"),
        QStringLiteral("parent_dir")
    };
    if (!targets.contains(result.matchTarget)) {
        if (error)
            *error = QStringLiteral("匹配目标无效：%1").arg(result.matchTarget);
        return false;
    }
    if (result.ruleType == QStringLiteral("regex")) {
        const QRegularExpression re(result.pattern);
        if (!re.isValid()) {
            if (error)
                *error = QStringLiteral("正则表达式无效：%1").arg(re.errorString());
            return false;
        }
    }

    *rule = result;
    return true;
}

static bool validateImportedRules(const QVector<RuleRecord>& rules, QString* error)
{
    QSet<int> ids;
    QHash<int, int> parentById;
    for (const RuleRecord& rule : rules) {
        if (ids.contains(rule.id)) {
            if (error)
                *error = QStringLiteral("规则 ID 重复：%1").arg(rule.id);
            return false;
        }
        ids.insert(rule.id);
        parentById.insert(rule.id, rule.parentId);
    }

    for (const RuleRecord& rule : rules) {
        if (rule.parentId == rule.id) {
            if (error)
                *error = QStringLiteral("规则不能作为自己的父规则：%1").arg(rule.name);
            return false;
        }
        if (rule.parentId != 0 && !ids.contains(rule.parentId)) {
            if (error)
                *error = QStringLiteral("规则“%1”的父规则不存在。").arg(rule.name);
            return false;
        }

        QSet<int> seen;
        int current = rule.parentId;
        while (current != 0) {
            if (seen.contains(current)) {
                if (error)
                    *error = QStringLiteral("规则树存在循环。");
                return false;
            }
            seen.insert(current);
            current = parentById.value(current, 0);
        }
    }
    return true;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    resize(1400, 850);
}

void MainWindow::buildUi()
{
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("项目"));
    fileMenu->addAction(QStringLiteral("新建项目数据库"), this, &MainWindow::createProject);
    fileMenu->addAction(QStringLiteral("打开项目数据库"), this, &MainWindow::openProject);
    m_recentProjectsMenu = fileMenu->addMenu(QStringLiteral("最近项目"));
    updateRecentProjectsMenu();
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("选择资源目录并扫描"), this, &MainWindow::scanResourceDirectory);
    fileMenu->addAction(QStringLiteral("重算规则命中"), this, &MainWindow::recalculateRules);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("导出规则为 JSON"), this, &MainWindow::exportRulesToJson);
    fileMenu->addAction(QStringLiteral("从 JSON 导入规则并覆盖"), this, &MainWindow::importRulesFromJson);

    setupImageColumnMenu();

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);

    auto* listBackgroundLayout = new QHBoxLayout;
    listBackgroundLayout->addWidget(new QLabel(QStringLiteral("图片列表背景"), left));
    auto* listBackgroundGroup = new QButtonGroup(left);
    addBackgroundRadio(listBackgroundLayout, listBackgroundGroup, left, QStringLiteral("棋盘格"), 0, true);
    addBackgroundRadio(listBackgroundLayout, listBackgroundGroup, left, QStringLiteral("系统"), 1);
    addBackgroundRadio(listBackgroundLayout, listBackgroundGroup, left, QStringLiteral("黑色"), 2);
    addBackgroundRadio(listBackgroundLayout, listBackgroundGroup, left, QStringLiteral("白色"), 3);
    addBackgroundRadio(listBackgroundLayout, listBackgroundGroup, left, QStringLiteral("灰色"), 4);
    listBackgroundLayout->addStretch();
    leftLayout->addLayout(listBackgroundLayout);

    m_table = new QTableView(left);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(false);
    applyThumbnailBackgroundColor();
    leftLayout->addWidget(m_table, 1);

    m_filter = new FilterPanel(left);
    leftLayout->addWidget(m_filter);

    auto* tabs = new QTabWidget(splitter);
    m_preview = new ImagePreviewWidget(tabs);
    m_stats = new QTextEdit(tabs);
    m_stats->setReadOnly(true);
    m_explain = new QTextEdit(tabs);
    m_explain->setReadOnly(true);
    tabs->addTab(m_preview, QStringLiteral("图片预览"));
    tabs->addTab(new QWidget(tabs), QStringLiteral("规则树"));
    tabs->addTab(m_explain, QStringLiteral("命中解释"));
    tabs->addTab(m_stats, QStringLiteral("统计"));

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    setCentralWidget(splitter);

    m_status = new QLabel(QStringLiteral("未打开项目"), this);
    m_progress = new QProgressBar(this);
    m_progress->setMaximumWidth(240);
    m_progress->setVisible(false);
    statusBar()->addWidget(m_status, 1);
    statusBar()->addPermanentWidget(m_progress);

    connect(m_filter, &FilterPanel::filterRequested, this, [this](const ImageFilter& f) {
        if (f.ruleType == "regex" && !f.pattern.trimmed().isEmpty()) {
            QRegularExpression re(f.pattern);
            if (!re.isValid()) {
                QMessageBox::warning(this, QStringLiteral("正则无效"), re.errorString());
                return;
            }
        }
        reloadImages(f);
    });
    connect(m_filter, &FilterPanel::clearRequested, this, [this] {
        reloadImages({});
    });
    connect(m_filter, &FilterPanel::addTopRuleRequested, this, &MainWindow::saveRule);
    connect(m_filter, &FilterPanel::addChildRuleRequested, this, &MainWindow::saveChildRule);
    connect(listBackgroundGroup, &QButtonGroup::idClicked, this, [this](int id) {
        switch (id) {
        case 0: setThumbnailBackgroundPreset(QApplication::palette().color(QPalette::Highlight), true); break;
        case 1: setThumbnailBackgroundPreset(QApplication::palette().color(QPalette::Highlight), false); break;
        case 2: setThumbnailBackgroundPreset(QColor(Qt::black), false); break;
        case 3: setThumbnailBackgroundPreset(QColor(Qt::white), false); break;
        case 4: setThumbnailBackgroundPreset(QColor(Qt::gray), false); break;
        default: break;
        }
    });
}

void MainWindow::setupImageColumnMenu()
{
    m_viewMenu = menuBar()->addMenu(QStringLiteral("查看"));
    auto* imageColumnsMenu = m_viewMenu->addMenu(QStringLiteral("文件列表显示信息"));

    for (int column = 0; column < ImageListModel::ColumnCount; ++column) {
        QAction* action = imageColumnsMenu->addAction(imageColumnTitle(column));
        action->setCheckable(true);
        action->setChecked(isDefaultImageColumnVisible(column));
        m_imageColumnActions.insert(column, action);
        connect(action, &QAction::toggled, this, [this](bool) {
            updateImageColumnVisibility();
        });
    }

}

void MainWindow::configureImageTableColumns()
{
    if (!m_table)
        return;

    QHeaderView* header = m_table->horizontalHeader();
    header->setStretchLastSection(false);
    header->setMinimumSectionSize(24);

    header->setSectionResizeMode(ImageListModel::ThumbnailColumn, QHeaderView::Fixed);
    m_table->setColumnWidth(ImageListModel::ThumbnailColumn, 128);

    header->setSectionResizeMode(ImageListModel::FileNameColumn, QHeaderView::Stretch);

    header->setSectionResizeMode(ImageListModel::RelativePathColumn, QHeaderView::Interactive);
    m_table->setColumnWidth(ImageListModel::RelativePathColumn, 220);

    header->setSectionResizeMode(ImageListModel::SizeColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ImageListModel::FileSizeColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ImageListModel::MatchCountColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ImageListModel::StatusColumn, QHeaderView::ResizeToContents);
}

void MainWindow::updateImageColumnVisibility()
{
    if (!m_table)
        return;
    configureImageTableColumns();
    for (int column = 0; column < ImageListModel::ColumnCount; ++column) {
        QAction* action = m_imageColumnActions.value(column, nullptr);
        const bool visible = action ? action->isChecked() : isDefaultImageColumnVisible(column);
        m_table->setColumnHidden(column, !visible);
    }
}

void MainWindow::setThumbnailBackgroundPreset(const QColor& color, bool checkerboard)
{
    m_thumbnailBackgroundColor = color.isValid() ? color : QApplication::palette().color(QPalette::Highlight);
    m_thumbnailCheckerboardBackground = checkerboard;
    applyThumbnailBackgroundColor();
}

void MainWindow::applyThumbnailBackgroundColor()
{
    if (!m_table)
        return;
    m_table->setProperty("thumbnailBackgroundColor", m_thumbnailBackgroundColor);
    m_table->setProperty("thumbnailCheckerboardBackground", m_thumbnailCheckerboardBackground);
    m_table->viewport()->update();
}

void MainWindow::createProject()
{
    QFileDialog dialog(this, QStringLiteral("新建项目数据库"), QDir::currentPath(), QStringLiteral("SQLite DB (*.db)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(QStringLiteral("db"));
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString dbPath = dialog.selectedFiles().value(0);
    if (!dbPath.isEmpty())
        setProject(dbPath);
}

void MainWindow::openProject()
{
    QFileDialog dialog(this, QStringLiteral("打开项目数据库"), QDir::currentPath(), QStringLiteral("SQLite DB (*.db)"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString dbPath = dialog.selectedFiles().value(0);
    if (!dbPath.isEmpty())
        setProject(dbPath);
}

void MainWindow::setProject(const QString& dbPath)
{
    if (!m_database.openProject(dbPath)) {
        QMessageBox::critical(this, QStringLiteral("打开项目失败"), m_database.lastError());
        return;
    }
    m_projectDir = QFileInfo(dbPath).absolutePath();
    m_images.setDatabase(m_database.db());
    m_rules.setDatabase(m_database.db());

    delete m_rulePanel;
    m_rulePanel = nullptr;
    delete m_thumbnails;
    delete m_imageModel;
    delete m_ruleModel;
    delete m_scanner;
    delete m_ruleEngine;

    m_thumbnails = new ThumbnailCache(&m_images, this);
    m_thumbnails->setCacheDir(QDir(m_projectDir).filePath(".project_cache/thumbnails"));
    m_imageModel = new ImageListModel(&m_images, m_thumbnails, this);
    m_ruleModel = new RuleTreeModel(&m_rules, this);
    m_scanner = new ProjectScanner(&m_images, this);
    m_ruleEngine = new RuleEngine(&m_images, &m_rules, this);

    m_table->setModel(m_imageModel);
    m_table->setItemDelegateForColumn(ImageListModel::ThumbnailColumn, new ThumbnailDelegate(m_table));
    configureImageTableColumns();
    updateImageColumnVisibility();
    reloadImages({});
    m_selectedRule = {};
    m_filter->setChildRuleEnabled(false);

    auto* tabs = findChild<QTabWidget*>();
    if (tabs && !m_rulePanel) {
        m_rulePanel = new RulePanel(m_ruleModel, tabs);
        tabs->removeTab(1);
        tabs->insertTab(1, m_rulePanel, QStringLiteral("规则树"));
    }
    m_rulePanel->reload();

    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::showImage);
    connect(m_rulePanel, &RulePanel::ruleSelected, this, [this](const RuleRecord& rule) {
        m_selectedRule = rule;
        m_filter->setChildRuleEnabled(rule.id > 0);
        m_filter->setRule(rule, rule.enabled);
        ImageFilter f = m_filter->filter();
        if (!rule.enabled)
            f.currentRuleId = 0;
        reloadImages(f);
    });
    connect(m_rulePanel, &RulePanel::ruleSelectionCleared, this, [this] {
        m_selectedRule = {};
        m_filter->clearRuleBinding();
        m_filter->setChildRuleEnabled(false);
    });
    connect(m_rulePanel, &RulePanel::editRuleRequested, this, [this](const RuleRecord& selectedRule) {
        RuleRecord rule = m_rules.fetchRule(selectedRule.id);
        if (rule.id == 0)
            return;
        if (!editRuleWithDialog(rule, QStringLiteral("编辑规则")))
            return;
        if (!m_rules.updateRule(rule))
            QMessageBox::critical(this, QStringLiteral("保存规则失败"), m_rules.lastError());
        recalculateRules();
    });
    connect(m_rulePanel, &RulePanel::deleteRuleRequested, this, [this](const RuleRecord& rule) {
        const auto answer = QMessageBox::question(this,
            QStringLiteral("删除规则"),
            QStringLiteral("确定删除规则“%1”及其所有子规则吗？").arg(rule.name));
        if (answer != QMessageBox::Yes)
            return;
        if (!m_rules.removeRuleRecursive(rule.id)) {
            QMessageBox::critical(this, QStringLiteral("删除规则失败"), m_rules.lastError());
            return;
        }
        recalculateRules();
    });
    connect(m_rulePanel, &RulePanel::toggleRuleRequested, this, [this](const RuleRecord& selectedRule) {
        RuleRecord rule = m_rules.fetchRule(selectedRule.id);
        if (rule.id == 0)
            return;
        rule.enabled = !rule.enabled;
        if (!m_rules.updateRule(rule)) {
            QMessageBox::critical(this, QStringLiteral("保存规则失败"), m_rules.lastError());
            return;
        }
        recalculateRules();
    });
    connect(m_scanner, &ProjectScanner::progress, this, [this](int current, int total, const QString& path) {
        m_progress->setVisible(true);
        if (total <= 0) {
            m_progress->setRange(0, 0);
            m_status->setText(QStringLiteral("正在枚举文件，已发现 %1 张图片：%2").arg(current).arg(path));
        } else {
            m_progress->setRange(0, total);
            m_progress->setValue(current);
            const int percent = total > 0 ? int((100.0 * current) / total) : 0;
            m_status->setText(QStringLiteral("扫描中：%1 / %2（%3%） %4").arg(current).arg(total).arg(percent).arg(path));
        }
    });
    connect(m_scanner, &ProjectScanner::finished, this, [this](int count) {
        m_progress->setVisible(false);
        m_status->setText(QStringLiteral("扫描完成：%1 张图片").arg(count));
        reloadImages({});
        refreshStats();
    });
    connect(m_scanner, &ProjectScanner::failed, this, [this](const QString& error) {
        m_progress->setVisible(false);
        QMessageBox::critical(this, QStringLiteral("扫描失败"), error);
    });
    connect(m_ruleEngine, &RuleEngine::progress, this, [this](int current, int total) {
        m_progress->setVisible(true);
        m_progress->setRange(0, total);
        m_progress->setValue(current);
        m_status->setText(QStringLiteral("规则重算中"));
    });
    connect(m_ruleEngine, &RuleEngine::finished, this, [this] {
        m_progress->setVisible(false);
        m_status->setText(QStringLiteral("规则重算完成"));
        reloadImages(m_filter->filter());
        m_rulePanel->reload();
        refreshStats();
    });
    connect(m_ruleEngine, &RuleEngine::failed, this, [this](const QString& error) {
        m_progress->setVisible(false);
        QMessageBox::critical(this, QStringLiteral("规则重算失败"), error);
    });

    setWindowTitle(QStringLiteral("imgmgr - %1").arg(QDir::toNativeSeparators(dbPath)));
    addRecentProject(dbPath);
    refreshStats();
}

void MainWindow::updateRecentProjectsMenu()
{
    if (!m_recentProjectsMenu)
        return;
    m_recentProjectsMenu->clear();
    const QStringList projects = recentProjects();
    if (projects.isEmpty()) {
        auto* emptyAction = m_recentProjectsMenu->addAction(QStringLiteral("无最近项目"));
        emptyAction->setEnabled(false);
        return;
    }
    for (const QString& project : projects) {
        QAction* action = m_recentProjectsMenu->addAction(project);
        connect(action, &QAction::triggered, this, [this, project] {
            setProject(project);
        });
    }
    m_recentProjectsMenu->addSeparator();
    m_recentProjectsMenu->addAction(QStringLiteral("清空最近项目"), this, [this] {
        setRecentProjects({});
        updateRecentProjectsMenu();
    });
}

void MainWindow::addRecentProject(const QString& dbPath)
{
    QStringList projects = recentProjects();
    projects.removeAll(dbPath);
    projects.prepend(dbPath);
    while (projects.size() > 10)
        projects.removeLast();
    setRecentProjects(projects);
    updateRecentProjectsMenu();
}

QStringList MainWindow::recentProjects() const
{
    return QSettings().value(QStringLiteral("recentProjects")).toStringList();
}

void MainWindow::setRecentProjects(const QStringList& projects)
{
    QSettings().setValue(QStringLiteral("recentProjects"), projects);
}

void MainWindow::exportRulesToJson()
{
    if (!m_database.db().isOpen()) {
        QMessageBox::warning(this, QStringLiteral("未打开项目"), QStringLiteral("请先打开项目数据库。"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("导出规则为 JSON"),
        QDir::currentPath(),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty())
        return;

    QJsonArray ruleArray;
    const QVector<RuleRecord> rules = m_rules.fetchRules(false);
    for (const RuleRecord& rule : rules)
        ruleArray.append(ruleToJson(rule));

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("imgmgr.rules"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("rules"), ruleArray);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("已导出 %1 条规则。").arg(rules.size()));
}

void MainWindow::importRulesFromJson()
{
    if (!m_database.db().isOpen()) {
        QMessageBox::warning(this, QStringLiteral("未打开项目"), QStringLiteral("请先打开项目数据库。"));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("从 JSON 导入规则并覆盖"),
        QDir::currentPath(),
        QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), file.errorString());
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), parseError.errorString());
        return;
    }

    QJsonArray ruleArray;
    if (document.isObject()) {
        const QJsonObject root = document.object();
        if (!root.contains(QStringLiteral("rules")) || !root.value(QStringLiteral("rules")).isArray()) {
            QMessageBox::critical(this, QStringLiteral("导入失败"), QStringLiteral("JSON 中缺少 rules 数组。"));
            return;
        }
        ruleArray = root.value(QStringLiteral("rules")).toArray();
    } else if (document.isArray()) {
        ruleArray = document.array();
    } else {
        QMessageBox::critical(this, QStringLiteral("导入失败"), QStringLiteral("JSON 根节点必须是对象或数组。"));
        return;
    }

    QVector<RuleRecord> rules;
    rules.reserve(ruleArray.size());
    for (const QJsonValue& value : ruleArray) {
        if (!value.isObject()) {
            QMessageBox::critical(this, QStringLiteral("导入失败"), QStringLiteral("rules 数组中存在非对象元素。"));
            return;
        }
        RuleRecord rule;
        QString error;
        if (!jsonToRule(value.toObject(), &rule, &error)) {
            QMessageBox::critical(this, QStringLiteral("导入失败"), error);
            return;
        }
        rules << rule;
    }

    QString error;
    if (!validateImportedRules(rules, &error)) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), error);
        return;
    }

    const auto answer = QMessageBox::warning(this,
        QStringLiteral("覆盖当前规则"),
        QStringLiteral("导入会删除当前所有规则、排除规则和规则命中记录，并导入 JSON 中的 %1 条规则。是否继续？").arg(rules.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    if (!m_rules.replaceRules(rules)) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), m_rules.lastError());
        return;
    }

    m_selectedRule = {};
    if (m_filter) {
        m_filter->clearRuleBinding();
        m_filter->setChildRuleEnabled(false);
    }
    if (m_rulePanel)
        m_rulePanel->reload();
    recalculateRules();
    QMessageBox::information(this, QStringLiteral("导入完成"), QStringLiteral("已导入 %1 条规则。").arg(rules.size()));
}

bool MainWindow::editRuleWithDialog(RuleRecord& rule, const QString& title)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);

    const QVector<RuleRecord> allRules = m_rules.fetchRules(false);
    QHash<int, RuleRecord> rulesById;
    for (const RuleRecord& item : allRules)
        rulesById.insert(item.id, item);
    QSet<int> invalidParentIds;
    if (rule.id > 0) {
        invalidParentIds.insert(rule.id);
        for (int childId : m_rules.childRuleIdsRecursive(rule.id))
            invalidParentIds.insert(childId);
    }
    auto localRulePath = [&rulesById](int ruleId) {
        QStringList parts;
        QSet<int> seen;
        int current = ruleId;
        while (current != 0 && rulesById.contains(current) && !seen.contains(current)) {
            seen.insert(current);
            const RuleRecord item = rulesById.value(current);
            parts.prepend(item.name);
            current = item.parentId;
        }
        return parts.join(QStringLiteral(" / "));
    };

    auto* name = new QLineEdit(rule.name, &dialog);
    auto* parent = new QComboBox(&dialog);
    parent->addItem(QStringLiteral("无（顶层规则）"), 0);
    for (const RuleRecord& item : allRules) {
        if (invalidParentIds.contains(item.id))
            continue;
        parent->addItem(localRulePath(item.id), item.id);
    }
    parent->setCurrentIndex(parent->findData(rule.parentId));
    if (parent->currentIndex() < 0)
        parent->setCurrentIndex(0);

    auto* pattern = new QLineEdit(rule.pattern, &dialog);
    auto* type = new QComboBox(&dialog);
    type->addItem(QStringLiteral("通配符"), "glob");
    type->addItem(QStringLiteral("正则表达式"), "regex");
    type->setCurrentIndex(type->findData(rule.ruleType));
    if (type->currentIndex() < 0)
        type->setCurrentIndex(0);

    auto* target = new QComboBox(&dialog);
    target->addItem(QStringLiteral("文件名（无后缀）"), "filename_stem");
    target->addItem(QStringLiteral("文件名（有后缀）"), "filename");
    target->addItem(QStringLiteral("相对路径"), "relative_path");
    target->addItem(QStringLiteral("完整路径"), "absolute_path");
    target->addItem(QStringLiteral("父目录"), "parent_dir");
    target->setCurrentIndex(target->findData(rule.matchTarget));
    if (target->currentIndex() < 0)
        target->setCurrentIndex(0);

    auto* priority = new QSpinBox(&dialog);
    priority->setRange(-100000, 100000);
    priority->setValue(rule.priority);
    auto* enabled = new QCheckBox(QStringLiteral("启用"), &dialog);
    enabled->setChecked(rule.enabled);
    auto* allowConflict = new QCheckBox(QStringLiteral("允许冲突"), &dialog);
    allowConflict->setChecked(rule.allowConflict);
    auto* caseSensitive = new QCheckBox(QStringLiteral("区分大小写"), &dialog);
    caseSensitive->setChecked(rule.caseSensitive);
    auto* wholeMatch = new QCheckBox(QStringLiteral("全字匹配"), &dialog);
    wholeMatch->setChecked(rule.wholeMatch);
    auto* note = new QLineEdit(rule.note, &dialog);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("规则名称"), name);
    form->addRow(QStringLiteral("父规则"), parent);
    form->addRow(QStringLiteral("规则内容"), pattern);
    form->addRow(QStringLiteral("规则类型"), type);
    form->addRow(QStringLiteral("匹配目标"), target);
    form->addRow(QStringLiteral("优先级"), priority);
    form->addRow(QString(), enabled);
    form->addRow(QString(), allowConflict);
    form->addRow(QString(), caseSensitive);
    form->addRow(QString(), wholeMatch);
    form->addRow(QStringLiteral("备注"), note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    while (dialog.exec() == QDialog::Accepted) {
        const QString ruleName = name->text().trimmed();
        const QString rulePattern = pattern->text().trimmed();
        if (ruleName.isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("规则名称为空"), QStringLiteral("请输入规则名称。"));
            continue;
        }
        if (rulePattern.isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("规则为空"), QStringLiteral("请输入规则内容。"));
            continue;
        }
        if (type->currentData().toString() == "regex") {
            QRegularExpression re(rulePattern);
            if (!re.isValid()) {
                QMessageBox::warning(&dialog, QStringLiteral("正则无效"), re.errorString());
                continue;
            }
        }
        rule.name = ruleName;
        rule.parentId = parent->currentData().toInt();
        rule.pattern = rulePattern;
        rule.ruleType = type->currentData().toString();
        rule.matchTarget = target->currentData().toString();
        rule.priority = priority->value();
        rule.enabled = enabled->isChecked();
        rule.allowConflict = allowConflict->isChecked();
        rule.caseSensitive = caseSensitive->isChecked();
        rule.wholeMatch = wholeMatch->isChecked();
        rule.note = note->text();
        return true;
    }
    return false;
}

QString MainWindow::rulePath(int ruleId, const QHash<int, RuleRecord>& rulesById) const
{
    QStringList parts;
    int current = ruleId;
    QSet<int> seen;
    while (current != 0 && rulesById.contains(current) && !seen.contains(current)) {
        seen.insert(current);
        const RuleRecord rule = rulesById.value(current);
        parts.prepend(rule.name);
        current = rule.parentId;
    }
    return parts.join(QStringLiteral(" / "));
}

QString MainWindow::conflictReason(int ruleA, int ruleB, const QHash<int, RuleRecord>& rulesById) const
{
    const RuleRecord a = rulesById.value(ruleA);
    const RuleRecord b = rulesById.value(ruleB);
    if (a.allowConflict || b.allowConflict)
        return QStringLiteral("无冲突：至少一个规则允许冲突。");
    if (m_ruleEngine && m_ruleEngine->isAncestorRule(ruleA, ruleB))
        return QStringLiteral("无冲突：%1 是 %2 的祖先规则。").arg(rulePath(ruleA, rulesById), rulePath(ruleB, rulesById));
    if (m_ruleEngine && m_ruleEngine->isAncestorRule(ruleB, ruleA))
        return QStringLiteral("无冲突：%1 是 %2 的祖先规则。").arg(rulePath(ruleB, rulesById), rulePath(ruleA, rulesById));
    return QStringLiteral("存在冲突：两个规则不在同一祖先链上，且未设置允许冲突。");
}

void MainWindow::reloadImages(const ImageFilter& filter)
{
    if (!m_imageModel)
        return;
    QElapsedTimer timer;
    timer.start();
    m_imageModel->reload(filter);
    updateFilterStatus(timer.elapsed());
}

void MainWindow::updateFilterStatus(qint64 elapsedMs)
{
    if (!m_imageModel || !m_status)
        return;
    const int total = m_imageModel->totalImageCount();
    const int classified = m_imageModel->statusCount(ImageStatus::Classified);
    const int unclassified = m_imageModel->statusCount(ImageStatus::Unclassified);
    const int conflicts = m_imageModel->statusCount(ImageStatus::Conflict);
    m_status->setText(QStringLiteral("当前筛选：%1 张 | 已分类：%2 | 未分类：%3 | 冲突：%4 | 用时：%5 ms")
        .arg(total)
        .arg(classified)
        .arg(unclassified)
        .arg(conflicts)
        .arg(elapsedMs));
}

void MainWindow::scanResourceDirectory()
{
    if (!m_database.db().isOpen()) {
        openProject();
        if (!m_database.db().isOpen())
            return;
    }
    if (!m_scanner) {
        QMessageBox::warning(this, QStringLiteral("未打开项目"), QStringLiteral("请先新建或打开项目数据库。"));
        return;
    }
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择资源目录"));
    if (dir.isEmpty())
        return;
    if (projectWritesWouldTouchResourceDir(dir)) {
        QMessageBox::warning(this,
            QStringLiteral("项目目录不能位于资源目录内"),
            QStringLiteral("为保证资源目录完全只读，项目数据库和 .project_cache 缩略图缓存必须保存在资源目录之外。\n\n请把项目 .db 文件放到其他目录后再扫描。"));
        return;
    }
    m_progress->setVisible(true);
    m_progress->setRange(0, 0);
    m_status->setText(QStringLiteral("准备扫描：%1").arg(dir));
    m_scanner->scan(dir);
}

bool MainWindow::projectWritesWouldTouchResourceDir(const QString& resourceDir) const
{
    const QString resourcePath = QDir(resourceDir).canonicalPath();
    const QString projectPath = QDir(m_projectDir).canonicalPath();
    if (resourcePath.isEmpty() || projectPath.isEmpty())
        return false;

    const QString normalizedResource = QDir::cleanPath(resourcePath).toLower();
    const QString normalizedProject = QDir::cleanPath(projectPath).toLower();
    return normalizedProject == normalizedResource
        || normalizedProject.startsWith(normalizedResource + QDir::separator());
}

void MainWindow::saveRule(const RuleRecord& input)
{
    if (!m_database.db().isOpen())
        return;
    RuleRecord rule = input;
    if (rule.name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("规则名称为空"), QStringLiteral("请输入规则名称，例如“按钮”。"));
        return;
    }
    if (rule.pattern.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("规则为空"), QStringLiteral("请输入规则内容。"));
        return;
    }
    if (rule.ruleType == "regex") {
        QRegularExpression re(rule.pattern);
        if (!re.isValid()) {
            QMessageBox::warning(this, QStringLiteral("正则无效"), re.errorString());
            return;
        }
    }
    if (!m_rules.addRule(rule)) {
        QMessageBox::critical(this, QStringLiteral("保存规则失败"), m_rules.lastError());
        return;
    }
    m_rulePanel->reload();
    recalculateRules();
}

void MainWindow::saveChildRule(const RuleRecord& input)
{
    if (!m_database.db().isOpen())
        return;
    if (m_selectedRule.id <= 0) {
        QMessageBox::warning(this, QStringLiteral("未选择父规则"), QStringLiteral("请先在规则树中选择一个父规则。"));
        return;
    }
    RuleRecord rule = input;
    rule.parentId = m_selectedRule.id;
    if (rule.name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("规则名称为空"), QStringLiteral("请输入规则名称，例如“按钮”。"));
        return;
    }
    if (rule.pattern.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("规则为空"), QStringLiteral("请输入规则内容。"));
        return;
    }
    if (rule.ruleType == "regex") {
        QRegularExpression re(rule.pattern);
        if (!re.isValid()) {
            QMessageBox::warning(this, QStringLiteral("正则无效"), re.errorString());
            return;
        }
    }
    if (!m_rules.addRule(rule)) {
        QMessageBox::critical(this, QStringLiteral("保存规则失败"), m_rules.lastError());
        return;
    }
    m_rulePanel->reload();
    recalculateRules();
}

void MainWindow::recalculateRules()
{
    if (m_ruleEngine)
        m_ruleEngine->recalculate();
}

void MainWindow::showImage(const QModelIndex& current)
{
    if (!m_imageModel || !current.isValid())
        return;
    const ImageRecord image = m_imageModel->imageAt(current.row());
    m_preview->setImage(image);
    const QVector<int> matches = m_ruleEngine ? m_ruleEngine->matchedRulesForImage(image.id) : QVector<int>();
    const QVector<RuleRecord> rules = m_rules.fetchRules(false);
    QHash<int, RuleRecord> rulesById;
    for (const RuleRecord& rule : rules)
        rulesById.insert(rule.id, rule);

    QStringList lines;
    lines << QStringLiteral("当前图片：") << image.relativePath << QString();
    lines << QStringLiteral("命中规则：");
    if (matches.isEmpty()) {
        lines << QStringLiteral("- 无");
    } else {
        for (int id : matches) {
            const RuleRecord rule = rulesById.value(id);
            lines << QStringLiteral("- %1  [%2: %3 | 目标: %4%5]")
                .arg(rulePath(id, rulesById),
                     rule.ruleType,
                     rule.pattern,
                     rule.matchTarget,
                     rule.enabled ? QString() : QStringLiteral(" | 已禁用"));
        }
    }

    QSet<int> matchedSet;
    for (int id : matches)
        matchedSet.insert(id);

    QStringList conflictLines;
    QStringList ancestorConflictLines;
    QStringList nonConflictLines;
    for (int id : matches) {
        int parent = rulesById.value(id).parentId;
        while (parent != 0 && rulesById.contains(parent)) {
            const RuleRecord ancestor = rulesById.value(parent);
            if (ancestor.enabled && !matchedSet.contains(parent)) {
                ancestorConflictLines << QStringLiteral("- %1 命中了子规则“%2”，但没有命中启用的祖先规则“%3”。")
                    .arg(image.relativePath,
                         rulePath(id, rulesById),
                         rulePath(parent, rulesById));
                break;
            }
            parent = ancestor.parentId;
        }
    }
    for (int i = 0; i < matches.size(); ++i) {
        for (int j = i + 1; j < matches.size(); ++j) {
            const int a = matches.at(i);
            const int b = matches.at(j);
            const QString pairText = QStringLiteral("- %1  <->  %2\n  %3")
                .arg(rulePath(a, rulesById),
                     rulePath(b, rulesById),
                     conflictReason(a, b, rulesById));
            if (m_ruleEngine && m_ruleEngine->isConflictBetweenRules(a, b))
                conflictLines << pairText;
            else
                nonConflictLines << pairText;
        }
    }

    lines << QString() << QStringLiteral("最终状态：") << imageStatusText(image.status);
    lines << QString() << QStringLiteral("冲突判断：");
    if (matches.size() <= 1 && ancestorConflictLines.isEmpty()) {
        lines << QStringLiteral("- 命中规则数量不超过 1，不存在规则冲突。");
    } else {
        if (!ancestorConflictLines.isEmpty()) {
            lines << QStringLiteral("存在祖先链冲突：");
            lines << ancestorConflictLines;
        }
        if (!conflictLines.isEmpty()) {
            lines << QStringLiteral("存在冲突的规则对：");
            lines << conflictLines;
        } else if (ancestorConflictLines.isEmpty()) {
            lines << QStringLiteral("- 无冲突。所有多重命中规则都在同一祖先链上，或规则允许冲突。");
        }
        if (!nonConflictLines.isEmpty()) {
            lines << QString() << QStringLiteral("非冲突规则对：");
            lines << nonConflictLines;
        }
    }

    if (!conflictLines.isEmpty() || !ancestorConflictLines.isEmpty()) {
        lines << QString() << QStringLiteral("建议处理方式：");
        lines << QStringLiteral("- 将更具体的规则移动为泛化规则的子规则。");
        lines << QStringLiteral("- 为确实可共存的规则启用“允许冲突”。");
        lines << QStringLiteral("- 调整规则内容，避免无关资源被同时命中。");
        lines << QStringLiteral("- 后续可添加排除规则进一步细化分类。");
    }
    m_explain->setText(lines.join(QStringLiteral("\n")));
}

void MainWindow::refreshStats()
{
    if (!m_database.db().isOpen())
        return;
    QSqlQuery q(m_database.db());
    auto scalar = [&q](const QString& sql) {
        q.exec(sql);
        return q.next() ? q.value(0).toInt() : 0;
    };
    const QVector<ImageRecord> images = m_images.fetchImages({});
    const int total = images.size();
    int classified = 0;
    int unclassified = 0;
    int conflicts = 0;
    int multiMatch = 0;
    for (const ImageRecord& image : images) {
        if (image.status == ImageStatus::Classified)
            ++classified;
        else if (image.status == ImageStatus::Unclassified)
            ++unclassified;
        else if (image.status == ImageStatus::Conflict)
            ++conflicts;
        else if (image.status == ImageStatus::MultiMatch)
            ++multiMatch;
    }
    const int rules = scalar("SELECT COUNT(*) FROM rules");
    const int enabled = scalar("SELECT COUNT(*) FROM rules WHERE enabled=1");
    const int transparent = scalar("SELECT COUNT(*) FROM images WHERE has_alpha=1");
    m_stats->setText(QStringLiteral(
        "总图片数：%1\n已分类图片数：%2\n未分类图片数：%3\n冲突图片数：%4\n多重命中图片数：%5\n规则数量：%6\n启用规则数量：%7\n禁用规则数量：%8\n透明图片数量：%9\n不透明图片数量：%10")
        .arg(total).arg(classified).arg(unclassified).arg(conflicts).arg(multiMatch)
        .arg(rules).arg(enabled).arg(rules - enabled).arg(transparent).arg(total - transparent));
}
