#include "MainWindow.h"

#include "utils/FileIoUtils.h"
#include "utils/ImageTableUtils.h"
#include "utils/PaintUtils.h"
#include "utils/ProjectFileDialogs.h"
#include "utils/RecentProjectsStore.h"
#include "utils/RuleExplanationBuilder.h"
#include "utils/RuleUtils.h"
#include "utils/UiUtils.h"
#include "services/ProjectPathService.h"
#include "services/ProjectStatsService.h"
#include "services/RuleJsonService.h"
#include "services/RuleValidationService.h"
#include "widgets/RuleEditDialog.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QDir>
#include <QDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QPainter>
#include <QProgressBar>
#include <QSplitter>
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
        RuleValidationService::ValidationError error;
        if (!RuleValidationService::validateFilterPattern(f, &error)) {
            QMessageBox::warning(this, error.title, error.message);
            return;
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
    const QString dbPath = ProjectFileDialogs::selectNewProjectDatabase(this);
    if (!dbPath.isEmpty())
        setProject(dbPath);
}

void MainWindow::openProject()
{
    const QString dbPath = ProjectFileDialogs::selectExistingProjectDatabase(this);
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
    ++m_projectGeneration;
    m_images.setDatabase(m_database.db());
    m_rules.setDatabase(m_database.db());

    resetProjectComponents();
    createProjectComponents();
    bindProjectModels();
    connectProjectSignals();

    setWindowTitle(QStringLiteral("imgmgr - %1").arg(QDir::toNativeSeparators(dbPath)));
    addRecentProject(dbPath);
    refreshStats();
}

void MainWindow::resetProjectComponents()
{
    delete m_rulePanel;
    m_rulePanel = nullptr;
    delete m_thumbnails;
    m_thumbnails = nullptr;
    delete m_imageModel;
    m_imageModel = nullptr;
    delete m_ruleModel;
    m_ruleModel = nullptr;
    delete m_scanner;
    m_scanner = nullptr;
    delete m_ruleEngine;
    m_ruleEngine = nullptr;
}

void MainWindow::createProjectComponents()
{
    m_thumbnails = new ThumbnailCache(&m_images, this);
    m_thumbnails->setCacheDir(QDir(m_projectDir).filePath(".project_cache/thumbnails"));
    m_imageModel = new ImageListModel(&m_images, m_thumbnails, this);
    m_ruleModel = new RuleTreeModel(&m_rules, this);
    m_scanner = new ProjectScanner(&m_images, this);
    m_ruleEngine = new RuleEngine(&m_images, &m_rules, this);
}

void MainWindow::bindProjectModels()
{
    m_table->setModel(m_imageModel);
    m_table->setItemDelegateForColumn(ImageListModel::ThumbnailColumn, new ThumbnailDelegate(m_table));
    ImageTableUtils::configureColumns(m_table);
    updateImageColumnVisibility();
    reloadImages({});
    clearSelectedRule(false);

    auto* tabs = findChild<QTabWidget*>();
    if (tabs && !m_rulePanel) {
        m_rulePanel = new RulePanel(m_ruleModel, tabs);
        tabs->removeTab(1);
        tabs->insertTab(1, m_rulePanel, QStringLiteral("规则树"));
    }
    m_rulePanel->reload();
}

void MainWindow::connectProjectSignals()
{
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::showImage);
    connectRulePanelSignals();
    connectScannerSignals(m_projectGeneration);
    connectRuleEngineSignals(m_projectGeneration);
}

bool MainWindow::isCurrentProjectGeneration(quint64 projectGeneration) const
{
    return projectGeneration == m_projectGeneration;
}

void MainWindow::selectRuleForFilter(const RuleRecord& rule)
{
    m_selectedRule = rule;
    if (!m_filter)
        return;
    m_filter->setChildRuleEnabled(rule.id > 0);
    m_filter->setRule(rule, rule.enabled);
}

void MainWindow::clearSelectedRule(bool clearFilterBinding)
{
    m_selectedRule = {};
    if (!m_filter)
        return;
    if (clearFilterBinding)
        m_filter->clearRuleBinding();
    m_filter->setChildRuleEnabled(false);
}

void MainWindow::connectRulePanelSignals()
{
    connect(m_rulePanel, &RulePanel::ruleSelected, this, [this](const RuleRecord& rule) {
        selectRuleForFilter(rule);
        ImageFilter f = m_filter->filter();
        if (!rule.enabled)
            f.currentRuleId = 0;
        reloadImages(f);
    });
    connect(m_rulePanel, &RulePanel::ruleSelectionCleared, this, [this] {
        clearSelectedRule(true);
    });
    connect(m_rulePanel, &RulePanel::editRuleRequested, this, &MainWindow::editSelectedRule);
    connect(m_rulePanel, &RulePanel::deleteRuleRequested, this, &MainWindow::deleteSelectedRule);
    connect(m_rulePanel, &RulePanel::toggleRuleRequested, this, &MainWindow::toggleSelectedRuleEnabled);
}

void MainWindow::editSelectedRule(const RuleRecord& selectedRule)
{
    RuleRecord rule = m_rules.fetchRule(selectedRule.id);
    if (rule.id == 0)
        return;
    if (!editRuleWithDialog(rule, QStringLiteral("编辑规则")))
        return;
    if (!m_rules.updateRule(rule))
        QMessageBox::critical(this, QStringLiteral("保存规则失败"), m_rules.lastError());
    recalculateRules();
}

void MainWindow::deleteSelectedRule(const RuleRecord& rule)
{
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
}

void MainWindow::toggleSelectedRuleEnabled(const RuleRecord& selectedRule)
{
    RuleRecord rule = m_rules.fetchRule(selectedRule.id);
    if (rule.id == 0)
        return;
    rule.enabled = !rule.enabled;
    if (!m_rules.updateRule(rule)) {
        QMessageBox::critical(this, QStringLiteral("保存规则失败"), m_rules.lastError());
        return;
    }
    recalculateRules();
}

void MainWindow::connectScannerSignals(quint64 projectGeneration)
{
    connect(m_scanner, &ProjectScanner::progress, this, [this, projectGeneration](int current, int total, const QString& path) {
        if (!isCurrentProjectGeneration(projectGeneration))
            return;
        showScannerProgress(current, total, path);
    });
    connect(m_scanner, &ProjectScanner::finished, this, [this, projectGeneration](int count) {
        if (!isCurrentProjectGeneration(projectGeneration))
            return;
        hideProgressWithStatus(QStringLiteral("扫描完成：%1 张图片").arg(count));
        reloadImagesAndStats({});
    });
    connect(m_scanner, &ProjectScanner::failed, this, [this, projectGeneration](const QString& error) {
        if (!isCurrentProjectGeneration(projectGeneration))
            return;
        showProgressFailure(QStringLiteral("扫描失败"), error);
    });
}

void MainWindow::connectRuleEngineSignals(quint64 projectGeneration)
{
    connect(m_ruleEngine, &RuleEngine::progress, this, [this, projectGeneration](int current, int total) {
        if (!isCurrentProjectGeneration(projectGeneration))
            return;
        showRuleEngineProgress(current, total);
    });
    connect(m_ruleEngine, &RuleEngine::finished, this, [this, projectGeneration] {
        if (!isCurrentProjectGeneration(projectGeneration))
            return;
        hideProgressWithStatus(QStringLiteral("规则重算完成"));
        reloadAfterRuleRecalculation();
    });
    connect(m_ruleEngine, &RuleEngine::failed, this, [this, projectGeneration](const QString& error) {
        if (!isCurrentProjectGeneration(projectGeneration))
            return;
        showProgressFailure(QStringLiteral("规则重算失败"), error);
    });
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

    const QString path = ProjectFileDialogs::selectRuleExportPath(this);
    if (path.isEmpty())
        return;

    const QVector<RuleRecord> rules = m_rules.fetchRules(false);
    const QJsonDocument document = RuleJsonService::buildExportDocument(rules);

    QString error;
    if (!FileIoUtils::writeAll(path, document.toJson(QJsonDocument::Indented), &error)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("已导出 %1 条规则。").arg(rules.size()));
}

void MainWindow::importRulesFromJson()
{
    if (!m_database.db().isOpen()) {
        QMessageBox::warning(this, QStringLiteral("未打开项目"), QStringLiteral("请先打开项目数据库。"));
        return;
    }

    const QString path = ProjectFileDialogs::selectRuleImportPath(this);
    if (path.isEmpty())
        return;

    QString error;
    QByteArray json;
    if (!FileIoUtils::readAll(path, &json, &error)) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), error);
        return;
    }

    QVector<RuleRecord> rules;
    if (!RuleJsonService::parseImportDocument(json, &rules, &error)) {
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

    clearSelectedRule(true);
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

void MainWindow::reloadImagesAndStats(const ImageFilter& filter)
{
    reloadImages(filter);
    refreshStats();
}

void MainWindow::reloadAfterRuleRecalculation()
{
    reloadImages(m_filter ? m_filter->filter() : ImageFilter {});
    if (m_rulePanel)
        m_rulePanel->reload();
    refreshStats();
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

void MainWindow::showScannerProgress(int current, int total, const QString& path)
{
    if (!m_progress || !m_status)
        return;
    m_progress->setVisible(true);
    if (total <= 0) {
        m_progress->setRange(0, 0);
        m_status->setText(QStringLiteral("正在枚举文件，已发现 %1 张图片：%2").arg(current).arg(path));
        return;
    }

    m_progress->setRange(0, total);
    m_progress->setValue(current);
    const int percent = int((100.0 * current) / total);
    m_status->setText(QStringLiteral("扫描中：%1 / %2（%3%） %4").arg(current).arg(total).arg(percent).arg(path));
}

void MainWindow::showRuleEngineProgress(int current, int total)
{
    if (!m_progress || !m_status)
        return;
    m_progress->setVisible(true);
    m_progress->setRange(0, total);
    m_progress->setValue(current);
    m_status->setText(QStringLiteral("规则重算中"));
}

void MainWindow::showIndeterminateProgress(const QString& statusText)
{
    if (!m_progress || !m_status)
        return;
    m_progress->setVisible(true);
    m_progress->setRange(0, 0);
    m_status->setText(statusText);
}

void MainWindow::hideProgressWithStatus(const QString& statusText)
{
    if (m_progress)
        m_progress->setVisible(false);
    if (m_status)
        m_status->setText(statusText);
}

void MainWindow::showProgressFailure(const QString& title, const QString& error)
{
    if (m_progress)
        m_progress->setVisible(false);
    QMessageBox::critical(this, title, error);
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
    const QString dir = ProjectFileDialogs::selectResourceDirectory(this);
    if (dir.isEmpty())
        return;
    if (ProjectPathService::projectWritesWouldTouchResourceDir(m_projectDir, dir)) {
        QMessageBox::warning(this,
            QStringLiteral("项目目录不能位于资源目录内"),
            QStringLiteral("为保证资源目录完全只读，项目数据库和 .project_cache 缩略图缓存必须保存在资源目录之外。\n\n请把项目 .db 文件放到其他目录后再扫描。"));
        return;
    }
    showIndeterminateProgress(QStringLiteral("准备扫描：%1").arg(dir));
    m_scanner->scan(dir);
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

bool MainWindow::addRuleAndRecalculate(const RuleRecord& rule)
{
    RuleValidationService::ValidationError error;
    if (!RuleValidationService::validateRuleForSave(rule, &error)) {
        QMessageBox::warning(this, error.title, error.message);
        return false;
    }
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
    m_stats->setText(ProjectStatsService::buildStatsText(m_database.db(), m_images));
}
