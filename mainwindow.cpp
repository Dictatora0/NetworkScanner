#include "mainwindow.h"
#include <QColor>
#include <QDateTime>
#include <QStandardPaths>
#include <QTimer>
#include <QPalette>
#include <QPixmap>
#include <QPainter>
#include <QStyleFactory>
#include <QInputDialog>
#include <QCalendarWidget>
#include <QScrollArea>
#include <QTimeEdit>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_hostsFound(0), m_currentHostIndex(-1), m_darkModeEnabled(false),
    m_deviceTypeChartView(nullptr), m_vendorChartView(nullptr), m_portDistributionChartView(nullptr),
    m_deviceAnalyzer(nullptr), m_scanHistory(nullptr), m_scanner(nullptr), m_networkTopology(nullptr)
{
    // 创建UI组件
    createUI();
    createMenus();
    
    // 创建网络扫描器
    m_scanner = new NetworkScanner(this);
    
    // 创建设备分析器
    m_deviceAnalyzer = new DeviceAnalyzer(this);
    
    // 创建扫描历史管理器
    m_scanHistory = new ScanHistory(this);
    
    // 设置图表
    if (m_deviceTypeChartView && m_deviceAnalyzer) {
        m_deviceTypeChartView->setChart(m_deviceAnalyzer->getDeviceTypeChart());
    }
    
    if (m_vendorChartView && m_deviceAnalyzer) {
        m_vendorChartView->setChart(m_deviceAnalyzer->getVendorDistributionChart());
    }
    
    if (m_portDistributionChartView && m_deviceAnalyzer) {
        m_portDistributionChartView->setChart(m_deviceAnalyzer->getPortDistributionChart());
    }
    
    // 连接信号和槽
    setupConnections();
    
    // 加载保存的设置
    loadSettings();
    
    // 设置初始状态
    m_stopButton->setEnabled(false);
    m_statusBar->showMessage("网络扫描器就绪");
}

MainWindow::~MainWindow()
{
    // 保存设置
    saveSettings();
}

void MainWindow::createUI()
{
    // 创建中央部件和标签页
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    QVBoxLayout *centralLayout = new QVBoxLayout(m_centralWidget);
    m_tabWidget = new QTabWidget(m_centralWidget);
    centralLayout->addWidget(m_tabWidget);
    
    // 创建扫描结果标签页
    m_scanTab = new QWidget(m_tabWidget);
    m_mainLayout = new QVBoxLayout(m_scanTab);
    
    // 创建控制区域
    m_controlLayout = new QHBoxLayout();
    m_scanButton = new QPushButton("开始扫描", m_scanTab);
    m_stopButton = new QPushButton("停止扫描", m_scanTab);
    m_clearButton = new QPushButton("清除结果", m_scanTab);
    m_saveButton = new QPushButton("保存结果", m_scanTab);
    m_progressBar = new QProgressBar(m_scanTab);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_statusLabel = new QLabel("就绪", m_scanTab);
    
    m_controlLayout->addWidget(m_scanButton);
    m_controlLayout->addWidget(m_stopButton);
    m_controlLayout->addWidget(m_clearButton);
    m_controlLayout->addWidget(m_saveButton);
    m_controlLayout->addWidget(m_progressBar);
    m_controlLayout->addWidget(m_statusLabel);
    
    // 创建过滤控件
    m_filterWidget = new QWidget(m_scanTab);
    QHBoxLayout *filterLayout = new QHBoxLayout(m_filterWidget);
    
    QLabel *filterLabel = new QLabel("过滤:", m_filterWidget);
    m_filterIPLineEdit = new QLineEdit(m_filterWidget);
    m_filterIPLineEdit->setPlaceholderText("IP地址");
    
    m_filterVendorComboBox = new QComboBox(m_filterWidget);
    m_filterVendorComboBox->addItem("所有厂商");
    
    m_filterTypeComboBox = new QComboBox(m_filterWidget);
    m_filterTypeComboBox->addItem("所有设备类型");
    m_filterTypeComboBox->addItem("路由器");
    m_filterTypeComboBox->addItem("服务器");
    m_filterTypeComboBox->addItem("个人电脑");
    m_filterTypeComboBox->addItem("移动设备");
    m_filterTypeComboBox->addItem("打印机");
    m_filterTypeComboBox->addItem("智能设备");
    
    m_filterButton = new QPushButton("应用过滤", m_filterWidget);
    m_clearFilterButton = new QPushButton("清除过滤", m_filterWidget);
    
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_filterIPLineEdit);
    filterLayout->addWidget(m_filterVendorComboBox);
    filterLayout->addWidget(m_filterTypeComboBox);
    filterLayout->addWidget(m_filterButton);
    filterLayout->addWidget(m_clearFilterButton);
    
    m_mainLayout->addLayout(m_controlLayout);
    m_mainLayout->addWidget(m_filterWidget);
    
    // 创建结果表格
    m_resultsTable = new QTableWidget(0, 6, m_scanTab);
    QStringList headers;
    headers << "IP地址" << "主机名" << "MAC地址" << "厂商" << "状态" << "开放端口";
    m_resultsTable->setHorizontalHeaderLabels(headers);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSortingEnabled(true);
    
    m_mainLayout->addWidget(m_resultsTable);
    
    // 创建其他标签页 - 这些方法内部会处理父子关系
    createSettingsDialog();
    createDetailsTab();
    createTopologyTab();
    createStatisticsTab();
    createHistoryTab();
    
    // 添加标签页到标签页控件
    m_tabWidget->addTab(m_scanTab, "扫描结果");
    if (m_settingsTab) m_tabWidget->addTab(m_settingsTab, "扫描设置");
    if (m_detailsTab) m_tabWidget->addTab(m_detailsTab, "主机详情");
    if (m_topologyTab) m_tabWidget->addTab(m_topologyTab, "网络拓扑");
    if (m_statisticsTab) m_tabWidget->addTab(m_statisticsTab, "统计分析");
    if (m_historyTab) m_tabWidget->addTab(m_historyTab, "扫描历史");
    
    // 创建状态栏
    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);
}

void MainWindow::createDetailsTab() {
    m_detailsTab = new QWidget(m_tabWidget);
    m_detailsLayout = new QVBoxLayout(m_detailsTab);
    m_detailsTextEdit = new QTextEdit(m_detailsTab);
    m_detailsTextEdit->setReadOnly(true);
    m_detailsLayout->addWidget(m_detailsTextEdit);
}

void MainWindow::createMenus()
{
    // 创建菜单
    m_fileMenu = menuBar()->addMenu("文件");
    m_viewMenu = menuBar()->addMenu("视图");
    m_toolsMenu = menuBar()->addMenu("工具");
    m_helpMenu = menuBar()->addMenu("帮助");
    
    // 文件菜单
    m_exportAction = new QAction("导出结果...", this);
    m_saveHistoryAction = new QAction("保存扫描历史...", this);
    m_loadHistoryAction = new QAction("加载扫描历史...", this);
    m_saveTopologyAction = new QAction("保存网络拓扑图...", this);
    m_exitAction = new QAction("退出", this);
    
    m_fileMenu->addAction(m_exportAction);
    m_fileMenu->addAction(m_saveHistoryAction);
    m_fileMenu->addAction(m_loadHistoryAction);
    m_fileMenu->addAction(m_saveTopologyAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);
    
    // 视图菜单
    m_darkModeAction = new QAction("暗色模式", this);
    m_darkModeAction->setCheckable(true);
    
    m_viewMenu->addAction(m_darkModeAction);
    
    // 工具菜单
    m_settingsAction = new QAction("设置", this);
    m_scheduleScanAction = new QAction("计划扫描...", this);
    
    m_toolsMenu->addAction(m_settingsAction);
    m_toolsMenu->addAction(m_scheduleScanAction);
    
    // 帮助菜单
    m_aboutAction = new QAction("关于", this);
    m_helpMenu->addAction(m_aboutAction);
    
    // 连接菜单动作
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::exportToCSV);
    connect(m_saveHistoryAction, &QAction::triggered, this, &MainWindow::saveHistoryToFile);
    connect(m_loadHistoryAction, &QAction::triggered, this, &MainWindow::loadHistoryFromFile);
    connect(m_saveTopologyAction, &QAction::triggered, this, &MainWindow::saveTopologyImage);
    connect(m_exitAction, &QAction::triggered, this, &QCoreApplication::quit);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    connect(m_darkModeAction, &QAction::toggled, this, &MainWindow::toggleDarkMode);
    connect(m_scheduleScanAction, &QAction::triggered, this, &MainWindow::scheduleScan);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::createSettingsDialog()
{
    m_settingsTab = new QWidget(m_tabWidget);
    m_settingsLayout = new QVBoxLayout(m_settingsTab);
    
    // 端口设置组
    m_portsGroupBox = new QGroupBox("端口扫描设置", m_settingsTab);
    QVBoxLayout *portsLayout = new QVBoxLayout(m_portsGroupBox);
    
    m_customPortsCheckBox = new QCheckBox("使用自定义端口列表", m_portsGroupBox);
    m_portsLineEdit = new QLineEdit(m_portsGroupBox);
    m_portsLineEdit->setPlaceholderText("端口列表 (例如: 21,22,23,80,443)");
    m_portsLineEdit->setEnabled(false);
    
    QHBoxLayout *timeoutLayout = new QHBoxLayout();
    QLabel *timeoutLabel = new QLabel("连接超时 (毫秒):", m_portsGroupBox);
    m_timeoutSpinBox = new QSpinBox(m_portsGroupBox);
    m_timeoutSpinBox->setRange(100, 10000);
    m_timeoutSpinBox->setValue(500);
    m_timeoutSpinBox->setSingleStep(100);
    timeoutLayout->addWidget(timeoutLabel);
    timeoutLayout->addWidget(m_timeoutSpinBox);
    
    portsLayout->addWidget(m_customPortsCheckBox);
    portsLayout->addWidget(m_portsLineEdit);
    portsLayout->addLayout(timeoutLayout);
    
    // IP范围设置组
    m_rangeGroupBox = new QGroupBox("IP范围设置", m_settingsTab);
    QVBoxLayout *rangeLayout = new QVBoxLayout(m_rangeGroupBox);
    
    m_customRangeCheckBox = new QCheckBox("使用自定义IP范围", m_rangeGroupBox);
    
    QHBoxLayout *ipRangeLayout = new QHBoxLayout();
    m_startIPLineEdit = new QLineEdit(m_rangeGroupBox);
    m_startIPLineEdit->setPlaceholderText("起始IP (例如: 192.168.1.1)");
    m_startIPLineEdit->setEnabled(false);
    QLabel *toLabel = new QLabel(" 到 ", m_rangeGroupBox);
    m_endIPLineEdit = new QLineEdit(m_rangeGroupBox);
    m_endIPLineEdit->setPlaceholderText("结束IP (例如: 192.168.1.254)");
    m_endIPLineEdit->setEnabled(false);
    ipRangeLayout->addWidget(m_startIPLineEdit);
    ipRangeLayout->addWidget(toLabel);
    ipRangeLayout->addWidget(m_endIPLineEdit);
    
    rangeLayout->addWidget(m_customRangeCheckBox);
    rangeLayout->addLayout(ipRangeLayout);
    
    m_settingsLayout->addWidget(m_portsGroupBox);
    m_settingsLayout->addWidget(m_rangeGroupBox);
    m_settingsLayout->addStretch();
    
    QPushButton *applyButton = new QPushButton("应用设置", m_settingsTab);
    connect(applyButton, &QPushButton::clicked, this, &MainWindow::applySettings);
    m_settingsLayout->addWidget(applyButton);
}

void MainWindow::createTopologyTab()
{
    m_topologyTab = new QWidget(m_tabWidget);
    QVBoxLayout *topologyLayout = new QVBoxLayout(m_topologyTab);
    
    // 创建工具栏
    QWidget *toolbarWidget = new QWidget(m_topologyTab);
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    
    QPushButton *zoomInButton = new QPushButton("放大", toolbarWidget);
    QPushButton *zoomOutButton = new QPushButton("缩小", toolbarWidget);
    QPushButton *resetViewButton = new QPushButton("重置视图", toolbarWidget);
    QPushButton *saveImageButton = new QPushButton("保存图像", toolbarWidget);
    
    toolbarLayout->addWidget(zoomInButton);
    toolbarLayout->addWidget(zoomOutButton);
    toolbarLayout->addWidget(resetViewButton);
    toolbarLayout->addWidget(saveImageButton);
    toolbarLayout->addStretch();
    
    // 创建网络拓扑组件
    m_networkTopology = new NetworkTopology(m_topologyTab);
    
    // 将工具栏和拓扑组件添加到布局
    topologyLayout->addWidget(toolbarWidget);
    topologyLayout->addWidget(m_networkTopology);
    
    // 连接信号和槽
    connect(zoomInButton, &QPushButton::clicked, this, [this]() {
        if (m_networkTopology) {
            // 调用网络拓扑的缩放方法
            // 例如：m_networkTopology->zoomIn();
        }
    });
    
    connect(zoomOutButton, &QPushButton::clicked, this, [this]() {
        if (m_networkTopology) {
            // 调用网络拓扑的缩放方法
            // 例如：m_networkTopology->zoomOut();
        }
    });
    
    connect(resetViewButton, &QPushButton::clicked, this, [this]() {
        if (m_networkTopology) {
            // 调用网络拓扑的重置视图方法
            // 例如：m_networkTopology->resetView();
        }
    });
    
    connect(saveImageButton, &QPushButton::clicked, this, &MainWindow::saveTopologyImage);
    
    // 连接设备选择信号
    if (m_networkTopology) {
        connect(m_networkTopology, &NetworkTopology::deviceSelected, this, [this](const HostInfo &host) {
            // 处理设备选择事件
            for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
                if (m_resultsTable->item(row, 0)->text() == host.ipAddress) {
                    m_resultsTable->selectRow(row);
                    showHostDetails(row, 0);
                    break;
                }
            }
        });
    }
}

void MainWindow::createStatisticsTab()
{
    m_statisticsTab = new QWidget(m_tabWidget);
    QVBoxLayout *statsLayout = new QVBoxLayout(m_statisticsTab);
    
    // 创建一个分割器
    QSplitter *splitter = new QSplitter(Qt::Vertical, m_statisticsTab);
    statsLayout->addWidget(splitter);
    
    // 创建图表区域
    QWidget *chartsWidget = new QWidget(splitter);
    QGridLayout *chartsLayout = new QGridLayout(chartsWidget);
    
    // 创建设备类型分布图表
    QGroupBox *deviceTypeBox = new QGroupBox("设备类型分布", chartsWidget);
    QVBoxLayout *deviceTypeLayout = new QVBoxLayout(deviceTypeBox);
    m_deviceTypeChartView = new QChartView(deviceTypeBox);
    m_deviceTypeChartView->setRenderHint(QPainter::Antialiasing);
    deviceTypeLayout->addWidget(m_deviceTypeChartView);
    
    // 创建厂商分布图表
    QGroupBox *vendorBox = new QGroupBox("厂商分布", chartsWidget);
    QVBoxLayout *vendorLayout = new QVBoxLayout(vendorBox);
    m_vendorChartView = new QChartView(vendorBox);
    m_vendorChartView->setRenderHint(QPainter::Antialiasing);
    vendorLayout->addWidget(m_vendorChartView);
    
    // 创建端口分布图表
    QGroupBox *portBox = new QGroupBox("常见端口分布", chartsWidget);
    QVBoxLayout *portLayout = new QVBoxLayout(portBox);
    m_portDistributionChartView = new QChartView(portBox);
    m_portDistributionChartView->setRenderHint(QPainter::Antialiasing);
    portLayout->addWidget(m_portDistributionChartView);
    
    // 添加图表到网格布局
    chartsLayout->addWidget(deviceTypeBox, 0, 0);
    chartsLayout->addWidget(vendorBox, 0, 1);
    chartsLayout->addWidget(portBox, 1, 0, 1, 2);
    
    // 创建安全报告区域
    QGroupBox *securityBox = new QGroupBox("网络安全报告", splitter);
    QVBoxLayout *securityLayout = new QVBoxLayout(securityBox);
    
    m_securityReportText = new QTextEdit(securityBox);
    m_securityReportText->setReadOnly(true);
    securityLayout->addWidget(m_securityReportText);
    
    QPushButton *generateReportButton = new QPushButton("生成安全报告", securityBox);
    securityLayout->addWidget(generateReportButton);
    
    connect(generateReportButton, &QPushButton::clicked, this, &MainWindow::generateSecurityReport);
    
    // 添加图表区域和安全报告区域到分割器
    splitter->addWidget(chartsWidget);
    splitter->addWidget(securityBox);
    
    // 设置各部分的初始大小
    splitter->setSizes(QList<int>() << 500 << 300);
}

void MainWindow::createHistoryTab()
{
    m_historyTab = new QWidget(m_tabWidget);
    QVBoxLayout *historyLayout = new QVBoxLayout(m_historyTab);
    
    QHBoxLayout *sessionLayout = new QHBoxLayout();
    QLabel *sessionLabel = new QLabel("扫描会话:", m_historyTab);
    m_sessionComboBox = new QComboBox(m_historyTab);
    QPushButton *compareButton = new QPushButton("比较会话", m_historyTab);
    QPushButton *loadButton = new QPushButton("加载会话", m_historyTab);
    QPushButton *deleteButton = new QPushButton("删除会话", m_historyTab);
    
    sessionLayout->addWidget(sessionLabel);
    sessionLayout->addWidget(m_sessionComboBox);
    sessionLayout->addWidget(compareButton);
    sessionLayout->addWidget(loadButton);
    sessionLayout->addWidget(deleteButton);
    sessionLayout->addStretch();
    
    m_historyTable = new QTableWidget(0, 6, m_historyTab);
    QStringList headers;
    headers << "IP地址" << "主机名" << "MAC地址" << "厂商" << "状态" << "开放端口";
    m_historyTable->setHorizontalHeaderLabels(headers);
    m_historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    historyLayout->addLayout(sessionLayout);
    historyLayout->addWidget(m_historyTable);
    
    connect(compareButton, &QPushButton::clicked, this, &MainWindow::compareScanResults);
    connect(m_sessionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            [this](int index) {
                if (!m_scanHistory || !m_historyTable) return;
                if (index >= 0 && index < m_scanHistory->sessionCount()) {
                    ScanSession session = m_scanHistory->getSession(index);
                    m_historyTable->setRowCount(0);
                    for (const HostInfo &host : session.hosts) {
                        int row = m_historyTable->rowCount();
                        m_historyTable->insertRow(row);
                        m_historyTable->setItem(row, 0, new QTableWidgetItem(host.ipAddress));
                        m_historyTable->setItem(row, 1, new QTableWidgetItem(host.hostName));
                        m_historyTable->setItem(row, 2, new QTableWidgetItem(host.macAddress));
                        m_historyTable->setItem(row, 3, new QTableWidgetItem(host.macVendor));
                        m_historyTable->setItem(row, 4, new QTableWidgetItem(host.isReachable ? "可达" : "不可达"));
                        QStringList openPortsList;
                        QMapIterator<int, bool> i(host.openPorts);
                        while (i.hasNext()) {
                            i.next();
                            if (i.value()) {
                                openPortsList << QString::number(i.key());
                            }
                        }
                        m_historyTable->setItem(row, 5, new QTableWidgetItem(openPortsList.join(", ")));
                        if (host.isReachable) {
                            for (int col = 0; col < 6; ++col) {
                                if(m_historyTable->item(row, col))
                                    m_historyTable->item(row, col)->setBackground(QColor(200, 255, 200));
                            }
                        }
                    }
                }
            });
    
    connect(loadButton, &QPushButton::clicked, [this]() {
        if (!m_scanHistory || !m_sessionComboBox || !m_tabWidget || !m_scanTab) return;
        int index = m_sessionComboBox->currentIndex();
        if (index >= 0 && index < m_scanHistory->sessionCount()) {
            ScanSession session = m_scanHistory->getSession(index);
            clearResults();
            for (const HostInfo &host : session.hosts) {
                onHostFound(host);
            }
            m_tabWidget->setCurrentWidget(m_scanTab);
            updateStatistics();
            updateNetworkTopology();
        }
    });
    
    connect(deleteButton, &QPushButton::clicked, [this]() {
        if (!m_scanHistory || !m_sessionComboBox) return;
        int index = m_sessionComboBox->currentIndex();
        if (index >= 0 && index < m_scanHistory->sessionCount()) {
            if (QMessageBox::question(this, "确认删除", 
                                   QString("确定要删除会话 '%1' 吗?").arg(m_sessionComboBox->currentText()),
                                   QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                m_scanHistory->removeSession(index);
            }
        }
    });
}

void MainWindow::setupConnections()
{
    // 基本控制按钮
    if (m_scanButton) {
        connect(m_scanButton, &QPushButton::clicked, this, &MainWindow::startScan);
    }
    if (m_stopButton) {
        connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopScan);
    }
    if (m_clearButton) {
        connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::clearResults);
    }
    if (m_saveButton) {
        connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveResults);
    }
    
    // 扫描仪信号连接
    if (m_scanner) {
        connect(m_scanner, &NetworkScanner::hostFound, this, &MainWindow::onHostFound);
        connect(m_scanner, &NetworkScanner::scanStarted, this, &MainWindow::onScanStarted);
        connect(m_scanner, &NetworkScanner::scanFinished, this, &MainWindow::onScanFinished);
        connect(m_scanner, &NetworkScanner::scanProgress, this, &MainWindow::onScanProgress);
        connect(m_scanner, &NetworkScanner::scanError, this, &MainWindow::onScanError);
    }
    
    // 表格和选项
    if (m_resultsTable) {
        connect(m_resultsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::showHostDetails);
    }
    if (m_customPortsCheckBox) {
        connect(m_customPortsCheckBox, &QCheckBox::toggled, this, &MainWindow::togglePortScanOptions);
    }
    if (m_customRangeCheckBox) {
        connect(m_customRangeCheckBox, &QCheckBox::toggled, this, &MainWindow::toggleRangeOptions);
    }
    
    // 过滤按钮
    if (m_filterButton) {
        connect(m_filterButton, &QPushButton::clicked, this, &MainWindow::filterResults);
    }
    if (m_clearFilterButton) {
        connect(m_clearFilterButton, &QPushButton::clicked, this, &MainWindow::clearFilters);
    }
    
    // 设备分析器信号
    if (m_deviceAnalyzer) {
        connect(m_deviceAnalyzer, &DeviceAnalyzer::analysisCompleted, this, [this]() {
            if (m_deviceTypeChartView && m_deviceAnalyzer) {
                m_deviceTypeChartView->chart()->update();
            }
            
            if (m_vendorChartView && m_deviceAnalyzer) {
                m_vendorChartView->chart()->update();
            }
            
            if (m_portDistributionChartView && m_deviceAnalyzer) {
                m_portDistributionChartView->chart()->update();
            }
        });
    }
    
    // 扫描历史信号
    if (m_scanHistory && m_sessionComboBox) {
        connect(m_scanHistory, &ScanHistory::historyChanged, this, [this]() {
            if (!m_sessionComboBox) return;
            
            m_sessionComboBox->clear();
            
            QList<ScanSession> sessions = m_scanHistory->getSessions();
            for (const ScanSession &session : sessions) {
                QString sessionText = session.description + 
                                     QString(" (%1 台设备)").arg(session.totalHosts());
                m_sessionComboBox->addItem(sessionText);
            }
        });
    }
    
    // 扫描错误处理
    if (m_scanner) {
        connect(m_scanner, &NetworkScanner::scanError, this, [this](const QString &errorMessage) {
            QMessageBox::critical(this, "扫描错误", QString("扫描过程中发生错误：%1").arg(errorMessage));
        });
    }
}

void MainWindow::togglePortScanOptions(bool checked)
{
    m_portsLineEdit->setEnabled(checked);
}

void MainWindow::toggleRangeOptions(bool checked)
{
    m_startIPLineEdit->setEnabled(checked);
    m_endIPLineEdit->setEnabled(checked);
}

void MainWindow::loadSettings()
{
    QSettings settings("NetScanner", "NetworkScanner");
    
    bool useCustomPorts = settings.value("UseCustomPorts", false).toBool();
    QString customPorts = settings.value("CustomPorts", "21,22,23,80,443").toString();
    int timeout = settings.value("Timeout", 500).toInt();
    
    m_customPortsCheckBox->setChecked(useCustomPorts);
    m_portsLineEdit->setText(customPorts);
    m_timeoutSpinBox->setValue(timeout);
    
    bool useCustomRange = settings.value("UseCustomRange", false).toBool();
    QString startIP = settings.value("StartIP", "").toString();
    QString endIP = settings.value("EndIP", "").toString();
    
    m_customRangeCheckBox->setChecked(useCustomRange);
    m_startIPLineEdit->setText(startIP);
    m_endIPLineEdit->setText(endIP);
    
    bool darkMode = settings.value("DarkMode", false).toBool();
    m_darkModeAction->setChecked(darkMode);
    applyTheme(darkMode);
    
    togglePortScanOptions(useCustomPorts);
    toggleRangeOptions(useCustomRange);
    applySettings();
}

void MainWindow::saveSettings()
{
    QSettings settings("NetScanner", "NetworkScanner");
    
    settings.setValue("UseCustomPorts", m_customPortsCheckBox->isChecked());
    settings.setValue("CustomPorts", m_portsLineEdit->text());
    settings.setValue("Timeout", m_timeoutSpinBox->value());
    
    settings.setValue("UseCustomRange", m_customRangeCheckBox->isChecked());
    settings.setValue("StartIP", m_startIPLineEdit->text());
    settings.setValue("EndIP", m_endIPLineEdit->text());
    
    settings.setValue("DarkMode", m_darkModeEnabled);
}

void MainWindow::applySettings()
{
    m_scanner->setScanTimeout(m_timeoutSpinBox->value());
    
    if (m_customPortsCheckBox->isChecked() && !m_portsLineEdit->text().isEmpty()) {
        QList<int> portsList;
        QString portsText = m_portsLineEdit->text();
        QStringList portStrings = portsText.split(",", Qt::SkipEmptyParts);
        
        for (const QString &portStr : portStrings) {
            bool ok;
            int port = portStr.trimmed().toInt(&ok);
            if (ok && port > 0 && port < 65536) {
                portsList.append(port);
            }
        }
        
        if (!portsList.isEmpty()) {
            m_scanner->setCustomPortsToScan(portsList);
        }
    }
    
    if (m_customRangeCheckBox->isChecked() && 
        !m_startIPLineEdit->text().isEmpty() && 
        !m_endIPLineEdit->text().isEmpty()) {
        m_scanner->setCustomIPRange(m_startIPLineEdit->text(), m_endIPLineEdit->text());
    }
    
    QMessageBox::information(this, "设置", "设置已应用");
}

void MainWindow::startScan()
{
    clearResults();
    
    m_scanner->startScan();
}

void MainWindow::stopScan()
{
    m_scanner->stopScan();
}

void MainWindow::clearResults()
{
    m_resultsTable->setRowCount(0);
    m_hostsFound = 0;
    m_detailsTextEdit->clear();
    m_statusBar->showMessage("网络扫描器就绪");
}

void MainWindow::saveResults()
{
    exportToCSV();
}

void MainWindow::exportToCSV()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出扫描结果", 
                                                  QDir::homePath() + "/网络扫描结果.csv", 
                                                  "CSV文件 (*.csv)");
    if (!fileName.isEmpty()) {
        m_scanner->saveResultsToFile(fileName);
        m_statusBar->showMessage("结果已保存到: " + fileName);
    }
}

void MainWindow::showSettings()
{
    m_tabWidget->setCurrentWidget(m_settingsTab);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "关于网络扫描器", 
                       "网络扫描器 V2.0\n\n"
                       "一个用于扫描本地网络中计算机的工具，可以检测IP地址、主机名、MAC地址和开放端口。\n\n"
                       "新增功能:\n"
                       "- 网络拓扑可视化\n"
                       "- 设备类型识别\n"
                       "- 扫描历史记录\n"
                       "- 网络安全分析\n\n"
                       "© 2023-2024 网络扫描器团队");
}

void MainWindow::showHostDetails(int row, int column)
{
    Q_UNUSED(column);
    
    if (row >= 0 && row < m_resultsTable->rowCount()) {
        QString ip = m_resultsTable->item(row, 0)->text();
        QString hostname = m_resultsTable->item(row, 1)->text();
        QString mac = m_resultsTable->item(row, 2)->text();
        QString vendor = m_resultsTable->item(row, 3)->text();
        QString status = m_resultsTable->item(row, 4)->text();
        QString openPorts = m_resultsTable->item(row, 5)->text();
        
        QString details = QString("主机详情:\n\n"
                                 "IP地址: %1\n"
                                 "主机名: %2\n"
                                 "MAC地址: %3\n"
                                 "厂商: %4\n"
                                 "状态: %5\n"
                                 "开放端口: %6\n\n")
                         .arg(ip).arg(hostname).arg(mac).arg(vendor).arg(status).arg(openPorts);
        
        QList<HostInfo> hosts = m_scanner->getScannedHosts();
        for (const HostInfo &host : hosts) {
            if (host.ipAddress == ip) {
                details += "详细端口信息:\n";
                QMapIterator<int, bool> i(host.openPorts);
                while (i.hasNext()) {
                    i.next();
                    if (i.value()) {
                        details += QString("端口 %1: 开放\n").arg(i.key());
                    }
                }
                details += QString("\n扫描时间: %1").arg(host.scanTime.toString("yyyy-MM-dd hh:mm:ss"));
                break;
            }
        }
        
        m_detailsTextEdit->setText(details);
        m_tabWidget->setCurrentWidget(m_detailsTab);
    }
}

void MainWindow::onHostFound(const HostInfo &host)
{
    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);
    
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(host.ipAddress));
    m_resultsTable->setItem(row, 1, new QTableWidgetItem(host.hostName));
    m_resultsTable->setItem(row, 2, new QTableWidgetItem(host.macAddress));
    m_resultsTable->setItem(row, 3, new QTableWidgetItem(host.macVendor));
    m_resultsTable->setItem(row, 4, new QTableWidgetItem(host.isReachable ? "可达" : "不可达"));
    
    QStringList openPortsList;
    QMapIterator<int, bool> i(host.openPorts);
    while (i.hasNext()) {
        i.next();
        if (i.value()) {
            openPortsList << QString::number(i.key());
        }
    }
    QString openPorts = openPortsList.join(", ");
    m_resultsTable->setItem(row, 5, new QTableWidgetItem(openPorts));
    
    if (host.isReachable) {
        for (int col = 0; col < 6; ++col) {
            m_resultsTable->item(row, col)->setBackground(QColor(200, 255, 200));
        }
    }
    
    m_hostsFound++;
    m_statusBar->showMessage(QString("已发现 %1 台主机").arg(m_hostsFound));
}

void MainWindow::onScanStarted()
{
    m_scanButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_clearButton->setEnabled(false);
    m_saveButton->setEnabled(false);
    m_progressBar->setValue(0);
    m_statusLabel->setText("扫描中...");
    m_statusBar->showMessage("开始扫描网络...");
    
    // 切换到扫描结果标签页
    m_tabWidget->setCurrentWidget(m_scanTab);
}

void MainWindow::onScanFinished()
{
    m_scanButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_clearButton->setEnabled(true);
    m_saveButton->setEnabled(true);
    m_progressBar->setValue(100);
    m_statusLabel->setText("扫描完成");
    m_statusBar->showMessage(QString("扫描完成，共发现 %1 台主机").arg(m_hostsFound));
    
    // 获取扫描结果并添加到历史记录
    QList<HostInfo> hosts = m_scanner->getScannedHosts();
    if (!hosts.isEmpty()) {
        // 先更新图表数据，再添加到历史记录
        // 避免因历史记录信号触发的UI刷新与当前更新冲突
        updateStatistics();
        updateNetworkTopology();
        
        // 添加到历史记录
        m_scanHistory->addSession(hosts);
    }
    
    if (m_hostsFound == 0) {
        QMessageBox::information(this, "扫描结果", "未发现任何可达的主机。");
    }
}

void MainWindow::onScanProgress(int progress)
{
    m_progressBar->setValue(progress);
}

void MainWindow::onScanError(const QString &errorMessage)
{
    QMessageBox::warning(this, "扫描错误", errorMessage);
}

// 新增功能方法实现

void MainWindow::showTopologyView()
{
    m_tabWidget->setCurrentWidget(m_topologyTab);
}

void MainWindow::showStatisticsView()
{
    m_tabWidget->setCurrentWidget(m_statisticsTab);
}

void MainWindow::showHistoryView()
{
    m_tabWidget->setCurrentWidget(m_historyTab);
}

void MainWindow::generateSecurityReport()
{
    // 获取当前扫描结果
    QList<HostInfo> hosts = m_scanner->getScannedHosts();
    
    if (hosts.isEmpty()) {
        QMessageBox::information(this, "安全报告", "没有可用的扫描结果来生成报告。请先执行扫描。");
        return;
    }
    
    // 生成安全报告
    QString report = m_deviceAnalyzer->generateSecurityReport(hosts);
    
    // 显示报告
    m_securityReportText->setText(report);
    m_tabWidget->setCurrentWidget(m_statisticsTab);
}

void MainWindow::saveTopologyImage()
{
    QString fileName = QFileDialog::getSaveFileName(this, "保存网络拓扑图", 
                                                  QDir::homePath() + "/网络拓扑图.png", 
                                                  "图像文件 (*.png *.jpg)");
    if (!fileName.isEmpty() && m_networkTopology) {
        // 创建一个QPixmap来捕获拓扑视图
        QPixmap pixmap(m_networkTopology->size());
        m_networkTopology->render(&pixmap);
        
        // 保存图像
        if (pixmap.save(fileName)) {
            m_statusBar->showMessage("网络拓扑图已保存到: " + fileName);
        } else {
            QMessageBox::warning(this, "保存失败", "无法保存网络拓扑图到文件: " + fileName);
        }
    }
}

void MainWindow::toggleDarkMode(bool enable)
{
    m_darkModeEnabled = enable;
    applyTheme(enable);
}

void MainWindow::applyTheme(bool darkMode)
{
    if (darkMode) {
        // 暗色主题
        QPalette darkPalette;
        QColor darkColor = QColor(45, 45, 45);
        QColor disabledColor = QColor(127, 127, 127);
        
        darkPalette.setColor(QPalette::Window, darkColor);
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(18, 18, 18));
        darkPalette.setColor(QPalette::AlternateBase, darkColor);
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
        darkPalette.setColor(QPalette::Button, darkColor);
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);
        
        qApp->setPalette(darkPalette);
        
        // 设置样式表
        qApp->setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }");
    } else {
        // 浅色主题 (系统默认)
        qApp->setPalette(QApplication::style()->standardPalette());
        qApp->setStyleSheet("");
    }
    
    // 通知主题已更改
    emit onThemeChanged();
}

void MainWindow::compareScanResults()
{
    // 显示对话框选择要比较的两个扫描会话
    if (m_scanHistory->sessionCount() < 2) {
        QMessageBox::information(this, "比较会话", "需要至少两个扫描会话才能进行比较。");
        return;
    }
    
    // 创建会话选择对话框
    QDialog dialog(this);
    dialog.setWindowTitle("选择要比较的扫描会话");
    
    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
    
    QLabel *label1 = new QLabel("会话 1 (较早):", &dialog);
    QComboBox *session1ComboBox = new QComboBox(&dialog);
    
    QLabel *label2 = new QLabel("会话 2 (较新):", &dialog);
    QComboBox *session2ComboBox = new QComboBox(&dialog);
    
    // 填充会话下拉框
    for (int i = 0; i < m_scanHistory->sessionCount(); ++i) {
        ScanSession session = m_scanHistory->getSession(i);
        QString sessionText = session.description + 
                             QString(" (%1 台设备)").arg(session.totalHosts());
        
        session1ComboBox->addItem(sessionText, i);
        session2ComboBox->addItem(sessionText, i);
    }
    
    // 默认选择最新的两个会话
    if (m_scanHistory->sessionCount() >= 2) {
        session1ComboBox->setCurrentIndex(m_scanHistory->sessionCount() - 2);  // 次新的会话
        session2ComboBox->setCurrentIndex(m_scanHistory->sessionCount() - 1);  // 最新的会话
    }
    
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    
    dialogLayout->addWidget(label1);
    dialogLayout->addWidget(session1ComboBox);
    dialogLayout->addWidget(label2);
    dialogLayout->addWidget(session2ComboBox);
    dialogLayout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        int index1 = session1ComboBox->currentData().toInt();
        int index2 = session2ComboBox->currentData().toInt();
        
        if (index1 == index2) {
            QMessageBox::warning(this, "比较会话", "不能比较同一个会话。");
            return;
        }
        
        // 比较会话
        QPair<QList<HostInfo>, QList<HostInfo>> result;
        try {
            result = m_scanHistory->compareScans(index1, index2);
        } catch (const std::exception &e) {
            QMessageBox::warning(this, "比较失败", QString("无法比较扫描会话: %1").arg(e.what()));
            return;
        }
        
        // 创建比较结果对话框
        QDialog resultDialog(this);
        resultDialog.setWindowTitle("扫描会话比较结果");
        resultDialog.resize(600, 400);
        
        QVBoxLayout *resultLayout = new QVBoxLayout(&resultDialog);
        
        // 创建标签页
        QTabWidget *tabWidget = new QTabWidget(&resultDialog);
        
        // 新增主机标签页
        QWidget *newTab = new QWidget(tabWidget);
        QVBoxLayout *newLayout = new QVBoxLayout(newTab);
        
        QLabel *newLabel = new QLabel(QString("新增主机: %1").arg(result.first.size()), newTab);
        QTableWidget *newTable = new QTableWidget(0, 3, newTab);
        newTable->setHorizontalHeaderLabels(QStringList() << "IP地址" << "主机名" << "MAC地址");
        newTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        
        for (const HostInfo &host : result.first) {
            int row = newTable->rowCount();
            newTable->insertRow(row);
            newTable->setItem(row, 0, new QTableWidgetItem(host.ipAddress));
            newTable->setItem(row, 1, new QTableWidgetItem(host.hostName));
            newTable->setItem(row, 2, new QTableWidgetItem(host.macAddress));
        }
        
        newLayout->addWidget(newLabel);
        newLayout->addWidget(newTable);
        
        // 消失主机标签页
        QWidget *missingTab = new QWidget(tabWidget);
        QVBoxLayout *missingLayout = new QVBoxLayout(missingTab);
        
        QLabel *missingLabel = new QLabel(QString("消失主机: %1").arg(result.second.size()), missingTab);
        QTableWidget *missingTable = new QTableWidget(0, 3, missingTab);
        missingTable->setHorizontalHeaderLabels(QStringList() << "IP地址" << "主机名" << "MAC地址");
        missingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        
        for (const HostInfo &host : result.second) {
            int row = missingTable->rowCount();
            missingTable->insertRow(row);
            missingTable->setItem(row, 0, new QTableWidgetItem(host.ipAddress));
            missingTable->setItem(row, 1, new QTableWidgetItem(host.hostName));
            missingTable->setItem(row, 2, new QTableWidgetItem(host.macAddress));
        }
        
        missingLayout->addWidget(missingLabel);
        missingLayout->addWidget(missingTable);
        
        // 添加标签页
        tabWidget->addTab(newTab, "新增主机");
        tabWidget->addTab(missingTab, "消失主机");
        
        resultLayout->addWidget(tabWidget);
        
        // 确定按钮
        QDialogButtonBox *resultButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok, &resultDialog);
        connect(resultButtonBox, &QDialogButtonBox::accepted, &resultDialog, &QDialog::accept);
        
        resultLayout->addWidget(resultButtonBox);
        
        resultDialog.exec();
    }
}

void MainWindow::scheduleScan()
{
    // 创建计划扫描对话框
    QDialog dialog(this);
    dialog.setWindowTitle("计划扫描");
    dialog.resize(400, 300);
    
    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
    
    // 扫描日期和时间选择
    QGroupBox *timeGroupBox = new QGroupBox("扫描时间", &dialog);
    QVBoxLayout *timeLayout = new QVBoxLayout(timeGroupBox);
    
    QCalendarWidget *calendar = new QCalendarWidget(timeGroupBox);
    calendar->setSelectedDate(QDate::currentDate());
    
    QHBoxLayout *timeEditLayout = new QHBoxLayout();
    QLabel *timeLabel = new QLabel("时间:", timeGroupBox);
    QTimeEdit *timeEdit = new QTimeEdit(QTime::currentTime().addSecs(3600), timeGroupBox);  // 默认为一小时后
    
    timeEditLayout->addWidget(timeLabel);
    timeEditLayout->addWidget(timeEdit);
    
    timeLayout->addWidget(calendar);
    timeLayout->addLayout(timeEditLayout);
    
    // 扫描选项
    QGroupBox *optionsGroupBox = new QGroupBox("扫描选项", &dialog);
    QVBoxLayout *optionsLayout = new QVBoxLayout(optionsGroupBox);
    
    QCheckBox *useCurrentSettingsCheckBox = new QCheckBox("使用当前扫描设置", optionsGroupBox);
    useCurrentSettingsCheckBox->setChecked(true);
    
    QCheckBox *saveResultsCheckBox = new QCheckBox("自动保存扫描结果", optionsGroupBox);
    saveResultsCheckBox->setChecked(true);
    
    QCheckBox *notifyCheckBox = new QCheckBox("扫描完成后通知", optionsGroupBox);
    notifyCheckBox->setChecked(true);
    
    optionsLayout->addWidget(useCurrentSettingsCheckBox);
    optionsLayout->addWidget(saveResultsCheckBox);
    optionsLayout->addWidget(notifyCheckBox);
    
    // 按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    
    // 添加所有组件到对话框
    dialogLayout->addWidget(timeGroupBox);
    dialogLayout->addWidget(optionsGroupBox);
    dialogLayout->addWidget(buttonBox);
    
    // 连接信号和槽
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    if (dialog.exec() == QDialog::Accepted) {
        // 获取用户选择的日期和时间
        QDateTime scheduleTime(calendar->selectedDate(), timeEdit->time());
        
        // 检查是否为有效的未来时间
        if (scheduleTime <= QDateTime::currentDateTime()) {
            QMessageBox::warning(this, "计划扫描", "请选择一个未来的时间点来安排扫描。");
            return;
        }
        
        // 计算时间差并创建一个定时器
        qint64 msecToScan = QDateTime::currentDateTime().msecsTo(scheduleTime);
        
        QTimer *scanTimer = new QTimer(this);
        scanTimer->setSingleShot(true);
        
        // 保存对话框中的选项值，避免在lambda中使用可能已经销毁的界面元素
        bool shouldSaveResults = saveResultsCheckBox->isChecked();
        bool shouldNotify = notifyCheckBox->isChecked();
        
        connect(scanTimer, &QTimer::timeout, [this, shouldSaveResults, shouldNotify]() {
            // 执行扫描
            startScan();
            
            // 如果扫描完成后需要通知
            if (shouldNotify) {
                // 假设我们将捕获onScanFinished信号并显示通知
                QObject *context = new QObject(this);
                connect(m_scanner, &NetworkScanner::scanFinished, context, [this, shouldSaveResults, context]() {
                    // 显示系统通知
                    QMessageBox::information(this, "计划扫描完成", 
                                           QString("计划的网络扫描已完成，发现了 %1 台设备。").arg(m_hostsFound));
                    
                    // 如果需要自动保存结果
                    if (shouldSaveResults) {
                        QString fileName = QDir::homePath() + QString("/网络扫描_%1.csv")
                                         .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
                        m_scanner->saveResultsToFile(fileName);
                        m_statusBar->showMessage("结果已自动保存到: " + fileName);
                    }
                    
                    // 清理上下文对象
                    context->deleteLater();
                });
            }
        });
        
        scanTimer->start(msecToScan);
        
        QMessageBox::information(this, "计划扫描", 
                               QString("扫描已计划在 %1 执行。").arg(scheduleTime.toString("yyyy-MM-dd hh:mm:ss")));
    }
}

void MainWindow::saveHistoryToFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, "保存扫描历史", 
                                                  QDir::homePath() + "/扫描历史.json", 
                                                  "JSON文件 (*.json)");
    if (!fileName.isEmpty()) {
        if (m_scanHistory->saveToFile(fileName)) {
            m_statusBar->showMessage("扫描历史已保存到: " + fileName);
        } else {
            QMessageBox::warning(this, "保存失败", "无法保存扫描历史到文件: " + fileName);
        }
    }
}

void MainWindow::loadHistoryFromFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "加载扫描历史", 
                                                  QDir::homePath(), 
                                                  "JSON文件 (*.json)");
    if (!fileName.isEmpty()) {
        if (m_scanHistory->loadFromFile(fileName)) {
            m_statusBar->showMessage("扫描历史已从文件加载: " + fileName);
        } else {
            QMessageBox::warning(this, "加载失败", "无法从文件加载扫描历史: " + fileName);
        }
    }
}

void MainWindow::updateNetworkTopology()
{
    QList<HostInfo> hosts = m_scanner->getScannedHosts();
    if (!hosts.isEmpty() && m_networkTopology) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_networkTopology->updateTopology(hosts);
        QApplication::restoreOverrideCursor();
    }
}

void MainWindow::refreshTopology()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    updateNetworkTopology();
    QApplication::restoreOverrideCursor();
}

void MainWindow::updateStatistics()
{
    QList<HostInfo> hosts = m_scanner->getScannedHosts();
    if (!hosts.isEmpty()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_deviceAnalyzer->analyzeHosts(hosts);
        QApplication::restoreOverrideCursor();
    }
}

void MainWindow::filterResults()
{
    QString ipFilter = m_filterIPLineEdit->text();
    QString vendorFilter = m_filterVendorComboBox->currentText();
    QString typeFilter = m_filterTypeComboBox->currentText();
    
    // 如果所有过滤器都为空，显示所有结果
    if (ipFilter.isEmpty() && (vendorFilter == "所有厂商") && (typeFilter == "所有设备类型")) {
        clearFilters();
        return;
    }
    
    // 遍历所有行，根据过滤条件隐藏或显示
    for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
        bool showRow = true;
        
        // 检查IP过滤器
        if (!ipFilter.isEmpty()) {
            QString ip = m_resultsTable->item(row, 0)->text();
            if (!ip.contains(ipFilter, Qt::CaseInsensitive)) {
                showRow = false;
            }
        }
        
        // 检查厂商过滤器
        if (vendorFilter != "所有厂商") {
            QString vendor = m_resultsTable->item(row, 3)->text();
            if (vendor != vendorFilter) {
                showRow = false;
            }
        }
        
        // 检查设备类型过滤器
        if (typeFilter != "所有设备类型") {
            // 注意：这里需要根据实际实现来确定如何判断设备类型
            // 这里假设有一种方法可以从IP或MAC地址来判断设备类型
            bool isOfType = false;
            QString ip = m_resultsTable->item(row, 0)->text();
            QString mac = m_resultsTable->item(row, 2)->text();
            QString vendor = m_resultsTable->item(row, 3)->text();
            
            if (typeFilter == "路由器") {
                isOfType = ip.endsWith(".1") || ip.endsWith(".254") || 
                          vendor.contains("路由", Qt::CaseInsensitive);
            } else if (typeFilter == "个人电脑") {
                isOfType = !ip.endsWith(".1") && !ip.endsWith(".254") &&
                          !vendor.contains("路由", Qt::CaseInsensitive) &&
                          !vendor.contains("服务器", Qt::CaseInsensitive);
            } else if (typeFilter == "服务器") {
                isOfType = vendor.contains("服务器", Qt::CaseInsensitive) ||
                          vendor.contains("server", Qt::CaseInsensitive);
            }
            
            if (!isOfType) {
                showRow = false;
            }
        }
        
        // 显示或隐藏此行
        m_resultsTable->setRowHidden(row, !showRow);
    }
}

void MainWindow::clearFilters()
{
    // 清除所有过滤器
    m_filterIPLineEdit->clear();
    m_filterVendorComboBox->setCurrentIndex(0);  // "所有厂商"
    m_filterTypeComboBox->setCurrentIndex(0);    // "所有设备类型"
    
    // 显示所有行
    for (int row = 0; row < m_resultsTable->rowCount(); ++row) {
        m_resultsTable->setRowHidden(row, false);
    }
}

void MainWindow::onThemeChanged()
{
    // 触发图表刷新以应用新主题
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // 更新图表设置
    m_deviceTypeChartView->setChart(m_deviceAnalyzer->getDeviceTypeChart());
    m_vendorChartView->setChart(m_deviceAnalyzer->getVendorDistributionChart());
    m_portDistributionChartView->setChart(m_deviceAnalyzer->getPortDistributionChart());
    
    // 设置抗锯齿渲染
    m_deviceTypeChartView->setRenderHint(QPainter::Antialiasing);
    m_vendorChartView->setRenderHint(QPainter::Antialiasing);
    m_portDistributionChartView->setRenderHint(QPainter::Antialiasing);
    
    // 刷新图表和其他UI元素
    m_deviceTypeChartView->repaint();
    m_vendorChartView->repaint();
    m_portDistributionChartView->repaint();
    
    QApplication::restoreOverrideCursor();
} 