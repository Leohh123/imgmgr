#pragma once

#include "database/DatabaseManager.h"
#include "database/ImageRepository.h"
#include "database/RuleRepository.h"
#include "models/ImageListModel.h"
#include "models/RuleTreeModel.h"
#include "services/ProjectScanner.h"
#include "services/RuleEngine.h"
#include "services/ThumbnailCache.h"
#include "widgets/FilterPanel.h"
#include "widgets/ImagePreviewWidget.h"
#include "widgets/RulePanel.h"

#include <QColor>
#include <QMainWindow>
#include <QHash>

class QAction;
class QLabel;
class QMenu;
class QProgressBar;
class QButtonGroup;
class QSplitter;
class QTableView;
class QTextEdit;
class QTabWidget;
class QWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void createProject();
    void openProject();
    void scanResourceDirectory();
    void saveRule(const RuleRecord& rule);
    void saveChildRule(const RuleRecord& rule);
    void recalculateRules();
    void exportRulesToJson();
    void importRulesFromJson();

private:
    void buildUi();
    QWidget* createLeftPane(QSplitter* splitter, QButtonGroup** listBackgroundGroup);
    QTabWidget* createDetailTabs(QSplitter* splitter);
    void setupStatusBar();
    void connectFilterPanelSignals(QButtonGroup* listBackgroundGroup);
    void setProject(const QString& dbPath);
    void resetProjectComponents();
    void createProjectComponents();
    void bindProjectModels();
    void clearProjectViews();
    void removeRulePanelTab();
    void installRulePanelTab();
    void connectProjectSignals();
    void connectRulePanelSignals();
    void connectScannerSignals(quint64 projectGeneration);
    void connectRuleEngineSignals(quint64 projectGeneration);
    bool isCurrentProjectGeneration(quint64 projectGeneration) const;
    void selectRuleForFilter(const RuleRecord& rule);
    void clearSelectedRule(bool clearFilterBinding);
    void editSelectedRule(const RuleRecord& selectedRule);
    void deleteSelectedRule(const RuleRecord& rule);
    void toggleSelectedRuleEnabled(const RuleRecord& selectedRule);
    void updateRecentProjectsMenu();
    void addRecentProject(const QString& dbPath);
    bool hasOpenProject();
    bool ensureProjectForScanning();
    bool confirmReplaceRules(int ruleCount);
    bool applyImportedRules(const QVector<RuleRecord>& rules);
    bool editRuleWithDialog(RuleRecord& rule, const QString& title);
    bool addRuleAndRecalculate(const RuleRecord& rule);
    void reloadImages(const ImageFilter& filter = {});
    void reloadImagesAndStats(const ImageFilter& filter);
    void reloadAfterRuleRecalculation();
    void updateFilterStatus(qint64 elapsedMs);
    void showScannerProgress(int current, int total, const QString& path);
    void showRuleEngineProgress(int current, int total);
    void showIndeterminateProgress(const QString& statusText);
    void hideProgressWithStatus(const QString& statusText);
    void showProgressFailure(const QString& title, const QString& error);
    void setupProjectMenu();
    void setupImageColumnMenu();
    void updateImageColumnVisibility();
    void setThumbnailBackgroundPreset(const QColor& color, bool checkerboard);
    void applyThumbnailBackgroundColor();
    void refreshStats();
    void showImage(const QModelIndex& current);

    DatabaseManager m_database;
    ImageRepository m_images;
    RuleRepository m_rules;
    ThumbnailCache* m_thumbnails = nullptr;
    ImageListModel* m_imageModel = nullptr;
    RuleTreeModel* m_ruleModel = nullptr;
    ProjectScanner* m_scanner = nullptr;
    RuleEngine* m_ruleEngine = nullptr;

    QTableView* m_table = nullptr;
    FilterPanel* m_filter = nullptr;
    ImagePreviewWidget* m_preview = nullptr;
    RulePanel* m_rulePanel = nullptr;
    QTabWidget* m_detailTabs = nullptr;
    QLabel* m_status = nullptr;
    QProgressBar* m_progress = nullptr;
    QTextEdit* m_stats = nullptr;
    QTextEdit* m_explain = nullptr;
    QMenu* m_recentProjectsMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QHash<int, QAction*> m_imageColumnActions;
    QColor m_thumbnailBackgroundColor;
    bool m_thumbnailCheckerboardBackground = true;
    QString m_projectDir;
    quint64 m_projectGeneration = 0;
    RuleRecord m_selectedRule;
};
