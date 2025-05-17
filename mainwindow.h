#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QHeaderView>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QFileDialog>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QSettings>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QGraphicsView>
#include <QGraphicsScene>

#include "networkscanner.h"
#include "networktopology.h"
#include "deviceanalyzer.h"
#include "scanhistory.h"

// QtCharts命名空间已经在deviceanalyzer.h中引入
// using namespace QtCharts;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startScan();
    void stopScan();
    void onHostFound(const HostInfo &host);
    void onScanStarted();
    void onScanFinished();
    void onScanProgress(int progress);
    void onScanError(const QString &errorMessage);
    
    // 现有功能槽
    void saveResults();
    void clearResults();
    void showSettings();
    void applySettings();
    void showAbout();
    void showHostDetails(int row, int column);
    void exportToCSV();
    void togglePortScanOptions(bool checked);
    void toggleRangeOptions(bool checked);
    
    // 新增功能槽
    void showTopologyView();
    void showStatisticsView();
    void showHistoryView();
    void generateSecurityReport();
    void saveTopologyImage();
    void toggleDarkMode(bool enable);
    void compareScanResults();
    void scheduleScan();
    void saveHistoryToFile();
    void loadHistoryFromFile();
    void updateNetworkTopology();
    void refreshTopology();
    void filterResults();
    void clearFilters();
    void onThemeChanged();

private:
    void createUI();
    void createMenus();
    void createSettingsDialog();
    void createTopologyTab();
    void createStatisticsTab();
    void createHistoryTab();
    void createDetailsTab();
    void createSecurityTab();
    void setupConnections();
    void updatePortsList();
    void loadSettings();
    void saveSettings();
    void updateStatistics();
    void applyTheme(bool darkMode);
    
    // UI元素
    QWidget *m_centralWidget;
    QTabWidget *m_tabWidget;
    
    // 扫描结果标签页
    QWidget *m_scanTab;
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_controlLayout;
    QTableWidget *m_resultsTable;
    QPushButton *m_scanButton;
    QPushButton *m_stopButton;
    QPushButton *m_clearButton;
    QPushButton *m_saveButton;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QStatusBar *m_statusBar;
    
    // 扫描设置标签页
    QWidget *m_settingsTab;
    QVBoxLayout *m_settingsLayout;
    
    // 端口设置区域
    QGroupBox *m_portsGroupBox;
    QCheckBox *m_customPortsCheckBox;
    QLineEdit *m_portsLineEdit;
    QSpinBox *m_timeoutSpinBox;
    
    // IP范围设置区域
    QGroupBox *m_rangeGroupBox;
    QCheckBox *m_customRangeCheckBox;
    QLineEdit *m_startIPLineEdit;
    QLineEdit *m_endIPLineEdit;
    
    // 主机详情标签页
    QWidget *m_detailsTab;
    QVBoxLayout *m_detailsLayout;
    QTextEdit *m_detailsTextEdit;
    
    // 网络拓扑标签页
    QWidget *m_topologyTab;
    NetworkTopology *m_networkTopology;
    QGraphicsView *m_topologyView;
    
    // 统计分析标签页
    QWidget *m_statisticsTab;
    DeviceAnalyzer *m_deviceAnalyzer;
    QChartView *m_deviceTypeChartView;
    QChartView *m_vendorChartView;
    QChartView *m_portDistributionChartView;
    QTextEdit *m_securityReportText;
    
    // 扫描历史标签页
    QWidget *m_historyTab;
    ScanHistory *m_scanHistory;
    QComboBox *m_sessionComboBox;
    QTableWidget *m_historyTable;
    
    // 菜单项
    QMenu *m_fileMenu;
    QMenu *m_viewMenu;
    QMenu *m_toolsMenu;
    QMenu *m_helpMenu;
    QAction *m_exportAction;
    QAction *m_saveHistoryAction;
    QAction *m_loadHistoryAction;
    QAction *m_saveTopologyAction;
    QAction *m_exitAction;
    QAction *m_settingsAction;
    QAction *m_darkModeAction;
    QAction *m_scheduleScanAction;
    QAction *m_aboutAction;
    
    // 过滤控件
    QWidget *m_filterWidget;
    QLineEdit *m_filterIPLineEdit;
    QComboBox *m_filterVendorComboBox;
    QComboBox *m_filterTypeComboBox;
    QPushButton *m_filterButton;
    QPushButton *m_clearFilterButton;
    
    // 网络扫描器
    NetworkScanner *m_scanner;

    // 扫描的主机数量
    int m_hostsFound;
    
    // 当前查看的主机索引
    int m_currentHostIndex;
    
    // 主题设置
    bool m_darkModeEnabled;
};

#endif // MAINWINDOW_H 