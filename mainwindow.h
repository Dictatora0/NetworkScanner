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

#include "networkscanner.h"

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
    
    // 新增功能槽
    void saveResults();
    void clearResults();
    void showSettings();
    void applySettings();
    void showAbout();
    void showHostDetails(int row, int column);
    void exportToCSV();
    void togglePortScanOptions(bool checked);
    void toggleRangeOptions(bool checked);

private:
    void createUI();
    void createMenus();
    void createSettingsDialog();
    void updatePortsList();
    void loadSettings();
    void saveSettings();
    
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
    
    // 菜单项
    QMenu *m_fileMenu;
    QMenu *m_toolsMenu;
    QMenu *m_helpMenu;
    QAction *m_exportAction;
    QAction *m_exitAction;
    QAction *m_settingsAction;
    QAction *m_aboutAction;
    
    // 网络扫描器
    NetworkScanner *m_scanner;

    // 扫描的主机数量
    int m_hostsFound;
    
    // 当前查看的主机索引
    int m_currentHostIndex;
};

#endif // MAINWINDOW_H 