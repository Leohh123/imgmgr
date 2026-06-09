#include "MainWindow.h"

#include "utils/ImageTableUtils.h"
#include "utils/PaintUtils.h"
#include "utils/RecentProjectsStore.h"
#include "utils/RuleExplanationBuilder.h"
#include "utils/RuleUtils.h"
#include "utils/UiUtils.h"
#include "widgets/RuleEditDialog.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QDir>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QPainter>
#include <QProgressBar>
#include <QRegularExpression>
#include <QSplitter>
#include <QSqlQuery>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTabWidget>
#include <QTextEdit>
#include <QSet>
#include <QVBoxLayout>

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
        PaintUtils::paintBackgroundPreset(painter, option.rect.adjusted(1, 1, -1, -1), background, checkerboard);

        if (pixmap.isNull())
            return;

        const QRect content = option.rect.adjusted(0, 4, 0, -4);
        const QPoint topLeft(
            content.left() + (content.width() - pixmap.width()) / 2,
            content.top() + (content.height() - pixmap.height()) / 2);
        painter->drawPixmap(topLeft, pixmap);
    }
};

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
    UiUtils::addBackgroundPresetRadios(listBackgroundLayout, listBackgroundGroup, left);
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
        if (f.ruleType == RuleUtils::regexRuleType() && !f.pattern.trimmed().isEmpty()) {
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
        const UiUtils::BackgroundPreset preset = UiUtils::backgroundPresetForId(id);
        setThumbnailBackgroundPreset(preset.color, preset.checkerboard);
    });
}

void MainWindow::setupImageColumnMenu()
{
    m_viewMenu = menuBar()->addMenu(QStringLiteral("查看"));
    auto* imageColumnsMenu = m_viewMenu->addMenu(QStringLiteral("文件列表显示信息"));

    for (int column = 0; column < ImageListModel::ColumnCount; ++column) {
        QAction* action = imageColumnsMenu->addAction(ImageTableUtils::columnTitle(column));
        action->setCheckable(true);
        action->setChecked(ImageTableUtils::isDefaultColumnVisible(column));
        m_imageColumnActions.insert(column, action);
        connect(action, &QAction::toggled, this, [this](bool) {
            updateImageColumnVisibility();
        });
    }

}

void MainWindow::updateImageColumnVisibility()
{
    if (!m_table)
        return;
    ImageTableUtils::configureColumns(m_table);
    for (int column = 0; column < ImageListModel::ColumnCount; ++column) {
        QAction* action = m_imageColumnActions.value(column, nullptr);
        const bool visible = action ? action->isChecked() : ImageTableUtils::isDefaultColumnVisible(column);
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
    ImageTableUtils::configureColumns(m_table);
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
    const QStringList projects = RecentProjectsStore::projects();
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
        RecentProjectsStore::setProjects({});
        updateRecentProjectsMenu();
    });
}

void MainWindow::addRecentProject(const QString& dbPath)
{
    RecentProjectsStore::addProject(dbPath);
    updateRecentProjectsMenu();
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
    QHash<int, QVector<RuleRecord>> childrenByParent;
    for (const RuleRecord& rule : rules)
        childrenByParent[rule.parentId].append(rule);
    for (const RuleRecord& rule : childrenByParent.value(0))
        ruleArray.append(RuleUtils::ruleTreeToJson(rule, childrenByParent));

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("imgmgr.rules"));
    root.insert(QStringLiteral("version"), 2);
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

    QString error;
    QVector<RuleRecord> rules;
    if (!RuleUtils::appendRulesFromJsonTree(ruleArray, 0, &rules, &error)) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), error);
        return;
    }

    if (!RuleUtils::validateImportedRules(rules, &error)) {
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
    const QVector<RuleRecord> allRules = m_rules.fetchRules(false);
    QSet<int> invalidParentIds;
    if (rule.id > 0) {
        invalidParentIds.insert(rule.id);
        for (int childId : m_rules.childRuleIdsRecursive(rule.id))
            invalidParentIds.insert(childId);
    }
    RuleEditDialog dialog(rule, allRules, invalidParentIds, title, this);
    if (dialog.exec() != QDialog::Accepted)
        return false;
    rule = dialog.rule();
    return true;
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
        || normalizedProject.startsWith(normalizedResource + QLatin1Char('/'));
}

void MainWindow::saveRule(const RuleRecord& input)
{
    if (!m_database.db().isOpen())
        return;
    addRuleAndRecalculate(input);
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
    addRuleAndRecalculate(rule);
}

bool MainWindow::validateRuleForSave(const RuleRecord& rule)
{
    if (rule.name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("规则名称为空"), QStringLiteral("请输入规则名称，例如“按钮”。"));
        return false;
    }
    if (rule.pattern.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("规则为空"), QStringLiteral("请输入规则内容。"));
        return false;
    }
    if (rule.ruleType == RuleUtils::regexRuleType()) {
        QRegularExpression re(rule.pattern);
        if (!re.isValid()) {
            QMessageBox::warning(this, QStringLiteral("正则无效"), re.errorString());
            return false;
        }
    }
    return true;
}

bool MainWindow::addRuleAndRecalculate(const RuleRecord& rule)
{
    if (!validateRuleForSave(rule))
        return false;
    if (!m_rules.addRule(rule)) {
        QMessageBox::critical(this, QStringLiteral("保存规则失败"), m_rules.lastError());
        return false;
    }
    m_rulePanel->reload();
    recalculateRules();
    return true;
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
    m_explain->setText(RuleExplanationBuilder::build(image, matches, rules));
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
    const QVector<ImageRecord> images = m_images.fetchAllImages();
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
