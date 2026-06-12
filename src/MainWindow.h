#pragma once

#include "services/ProjectSession.h"
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
    // UI setup
    void buildUi();
    QWidget* createLeftPane(QSplitter* splitter, QButtonGroup** listBackgroundGroup);
    QTabWidget* createDetailTabs(QSplitter* splitter);
    void setupStatusBar();
    void connectFilterPanelSignals(QButtonGroup* listBackgroundGroup);
    void setupProjectMenu();
    void setupImageColumnMenu();
    void updateImageColumnVisibility();
    void setThumbnailBackgroundPreset(const QColor& color, bool checkerboard);
    void applyThumbnailBackgroundColor();

    // Project lifecycle
    void setProject(const QString& dbPath);
    void bindProjectModels();
    void clearProjectViews();
    void removeRulePanelTab();
    void installRulePanelTab();
    void connectProjectSignals();
    bool isCurrentProjectGeneration(quint64 projectGeneration) const;
    bool hasOpenProject();
    bool ensureProjectForScanning();

    // Rule workflow
    void connectRulePanelSignals();
    void applySelectedRuleFilter(const RuleRecord& rule);
    void selectRuleForFilter(const RuleRecord& rule);
    void clearSelectedRule(bool clearFilterBinding);
    void editSelectedRule(const RuleRecord& selectedRule);
    void deleteSelectedRule(const RuleRecord& rule);
    void toggleSelectedRuleEnabled(const RuleRecord& selectedRule);
    void updateRecentProjectsMenu();
    void addRecentProject(const QString& dbPath);
    bool confirmReplaceRules(int ruleCount);
    bool applyImportedRules(const QVector<RuleRecord>& rules);
    bool editRuleWithDialog(RuleRecord& rule, const QString& title);
    bool saveUpdatedRule(const RuleRecord& rule);
    bool addRuleAndRecalculate(const RuleRecord& rule);
    void reloadRulePanel();
    void showRuleSaveFailure();

    // Refresh and status
    void connectScannerSignals(quint64 projectGeneration);
    void connectRuleEngineSignals(quint64 projectGeneration);
    void reloadImages(const ImageFilter& filter = {});
    void reloadImagesAndStats(const ImageFilter& filter);
    void reloadAfterRuleRecalculation();
    void updateFilterStatus(qint64 elapsedMs);
    void showScannerProgress(int current, int total, const QString& path);
    void showRuleEngineProgress(int current, int total);
    void showIndeterminateProgress(const QString& statusText);
    void hideProgressWithStatus(const QString& statusText);
    void showProgressFailure(const QString& title, const QString& error);
    const QVector<RuleRecord>& ruleExplanationRules();
    void invalidateRuleExplanationRules();
    void refreshStats();
    void showImage(const QModelIndex& current);

    ProjectSession m_project;

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
    QVector<RuleRecord> m_ruleExplanationRules;
    bool m_ruleExplanationRulesDirty = true;
    RuleRecord m_selectedRule;
};
