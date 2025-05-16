#include "mainwindow.h"
#include <QColor>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_hostsFound(0), m_currentHostIndex(-1)
{
    // 创建UI组件
    createUI();
    createMenus();
    
    // 创建网络扫描器
    m_scanner = new NetworkScanner(this);
    
    // 连接信号和槽
    connect(m_scanButton, &QPushButton::clicked, this, &MainWindow::startScan);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopScan);
    connect(m_clearButton, &QPushButton::clicked, this, &MainWindow::clearResults);
    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::saveResults);
    
    connect(m_scanner, &NetworkScanner::hostFound, this, &MainWindow::onHostFound);
    connect(m_scanner, &NetworkScanner::scanStarted, this, &MainWindow::onScanStarted);
    connect(m_scanner, &NetworkScanner::scanFinished, this, &MainWindow::onScanFinished);
    connect(m_scanner, &NetworkScanner::scanProgress, this, &MainWindow::onScanProgress);
    connect(m_scanner, &NetworkScanner::scanError, this, &MainWindow::onScanError);
    
    connect(m_resultsTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::showHostDetails);
    connect(m_customPortsCheckBox, &QCheckBox::toggled, this, &MainWindow::togglePortScanOptions);
    connect(m_customRangeCheckBox, &QCheckBox::toggled, this, &MainWindow::toggleRangeOptions);
    
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
    
    m_tabWidget = new QTabWidget(m_centralWidget);
    QVBoxLayout *centralLayout = new QVBoxLayout(m_centralWidget);
    centralLayout->addWidget(m_tabWidget);
    
    // 创建扫描结果标签页
    m_scanTab = new QWidget();
    m_mainLayout = new QVBoxLayout(m_scanTab);
    
    // 创建控制区域
    m_controlLayout = new QHBoxLayout();
    m_scanButton = new QPushButton("开始扫描", this);
    m_stopButton = new QPushButton("停止扫描", this);
    m_clearButton = new QPushButton("清除结果", this);
    m_saveButton = new QPushButton("保存结果", this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_statusLabel = new QLabel("就绪", this);
    
    m_controlLayout->addWidget(m_scanButton);
    m_controlLayout->addWidget(m_stopButton);
    m_controlLayout->addWidget(m_clearButton);
    m_controlLayout->addWidget(m_saveButton);
    m_controlLayout->addWidget(m_progressBar);
    m_controlLayout->addWidget(m_statusLabel);
    
    m_mainLayout->addLayout(m_controlLayout);
    
    // 创建结果表格
    m_resultsTable = new QTableWidget(0, 6, this);
    QStringList headers;
    headers << "IP地址" << "主机名" << "MAC地址" << "厂商" << "状态" << "开放端口";
    m_resultsTable->setHorizontalHeaderLabels(headers);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSortingEnabled(true);
    
    m_mainLayout->addWidget(m_resultsTable);
    
    // 创建设置标签页
    createSettingsDialog();
    
    // 创建详情标签页
    m_detailsTab = new QWidget();
    m_detailsLayout = new QVBoxLayout(m_detailsTab);
    m_detailsTextEdit = new QTextEdit(m_detailsTab);
    m_detailsTextEdit->setReadOnly(true);
    m_detailsLayout->addWidget(m_detailsTextEdit);
    
    // 添加标签页到标签页控件
    m_tabWidget->addTab(m_scanTab, "扫描结果");
    m_tabWidget->addTab(m_settingsTab, "扫描设置");
    m_tabWidget->addTab(m_detailsTab, "主机详情");
    
    // 创建状态栏
    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);
}

void MainWindow::createMenus()
{
    // 创建菜单
    m_fileMenu = menuBar()->addMenu("文件");
    m_toolsMenu = menuBar()->addMenu("工具");
    m_helpMenu = menuBar()->addMenu("帮助");
    
    // 文件菜单
    m_exportAction = new QAction("导出结果...", this);
    m_exitAction = new QAction("退出", this);
    m_fileMenu->addAction(m_exportAction);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAction);
    
    // 工具菜单
    m_settingsAction = new QAction("设置", this);
    m_toolsMenu->addAction(m_settingsAction);
    
    // 帮助菜单
    m_aboutAction = new QAction("关于", this);
    m_helpMenu->addAction(m_aboutAction);
    
    // 连接菜单动作
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::exportToCSV);
    connect(m_exitAction, &QAction::triggered, this, &QCoreApplication::quit);
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::createSettingsDialog()
{
    m_settingsTab = new QWidget();
    m_settingsLayout = new QVBoxLayout(m_settingsTab);
    
    // 端口设置组
    m_portsGroupBox = new QGroupBox("端口扫描设置", m_settingsTab);
    QVBoxLayout *portsLayout = new QVBoxLayout(m_portsGroupBox);
    
    m_customPortsCheckBox = new QCheckBox("使用自定义端口列表");
    m_portsLineEdit = new QLineEdit();
    m_portsLineEdit->setPlaceholderText("端口列表 (例如: 21,22,23,80,443)");
    m_portsLineEdit->setEnabled(false);
    
    QHBoxLayout *timeoutLayout = new QHBoxLayout();
    QLabel *timeoutLabel = new QLabel("连接超时 (毫秒):");
    m_timeoutSpinBox = new QSpinBox();
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
    
    m_customRangeCheckBox = new QCheckBox("使用自定义IP范围");
    
    QHBoxLayout *ipRangeLayout = new QHBoxLayout();
    m_startIPLineEdit = new QLineEdit();
    m_startIPLineEdit->setPlaceholderText("起始IP (例如: 192.168.1.1)");
    m_startIPLineEdit->setEnabled(false);
    QLabel *toLabel = new QLabel(" 到 ");
    m_endIPLineEdit = new QLineEdit();
    m_endIPLineEdit->setPlaceholderText("结束IP (例如: 192.168.1.254)");
    m_endIPLineEdit->setEnabled(false);
    ipRangeLayout->addWidget(m_startIPLineEdit);
    ipRangeLayout->addWidget(toLabel);
    ipRangeLayout->addWidget(m_endIPLineEdit);
    
    rangeLayout->addWidget(m_customRangeCheckBox);
    rangeLayout->addLayout(ipRangeLayout);
    
    // 添加到设置布局
    m_settingsLayout->addWidget(m_portsGroupBox);
    m_settingsLayout->addWidget(m_rangeGroupBox);
    m_settingsLayout->addStretch();
    
    // 添加应用按钮
    QPushButton *applyButton = new QPushButton("应用设置");
    connect(applyButton, &QPushButton::clicked, this, &MainWindow::applySettings);
    m_settingsLayout->addWidget(applyButton);
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
    
    // 加载端口设置
    bool useCustomPorts = settings.value("UseCustomPorts", false).toBool();
    QString customPorts = settings.value("CustomPorts", "21,22,23,80,443").toString();
    int timeout = settings.value("Timeout", 500).toInt();
    
    m_customPortsCheckBox->setChecked(useCustomPorts);
    m_portsLineEdit->setText(customPorts);
    m_timeoutSpinBox->setValue(timeout);
    
    // 加载IP范围设置
    bool useCustomRange = settings.value("UseCustomRange", false).toBool();
    QString startIP = settings.value("StartIP", "").toString();
    QString endIP = settings.value("EndIP", "").toString();
    
    m_customRangeCheckBox->setChecked(useCustomRange);
    m_startIPLineEdit->setText(startIP);
    m_endIPLineEdit->setText(endIP);
    
    // 应用这些设置
    togglePortScanOptions(useCustomPorts);
    toggleRangeOptions(useCustomRange);
    applySettings();
}

void MainWindow::saveSettings()
{
    QSettings settings("NetScanner", "NetworkScanner");
    
    // 保存端口设置
    settings.setValue("UseCustomPorts", m_customPortsCheckBox->isChecked());
    settings.setValue("CustomPorts", m_portsLineEdit->text());
    settings.setValue("Timeout", m_timeoutSpinBox->value());
    
    // 保存IP范围设置
    settings.setValue("UseCustomRange", m_customRangeCheckBox->isChecked());
    settings.setValue("StartIP", m_startIPLineEdit->text());
    settings.setValue("EndIP", m_endIPLineEdit->text());
}

void MainWindow::applySettings()
{
    // 应用超时设置
    m_scanner->setScanTimeout(m_timeoutSpinBox->value());
    
    // 应用端口设置
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
    
    // 应用IP范围设置
    if (m_customRangeCheckBox->isChecked() && 
        !m_startIPLineEdit->text().isEmpty() && 
        !m_endIPLineEdit->text().isEmpty()) {
        m_scanner->setCustomIPRange(m_startIPLineEdit->text(), m_endIPLineEdit->text());
    }
    
    QMessageBox::information(this, "设置", "设置已应用");
}

void MainWindow::startScan()
{
    // 清空上次扫描结果
    clearResults();
    
    // 开始新的扫描
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
                       "网络扫描器 V1.1\n\n"
                       "一个用于扫描本地网络中计算机的工具，可以检测IP地址、主机名、MAC地址和开放端口。\n\n"
                       "© 2023 网络扫描器团队");
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
        
        // 查找更详细的端口信息
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
    // 添加新行
    int row = m_resultsTable->rowCount();
    m_resultsTable->insertRow(row);
    
    // 填充数据
    m_resultsTable->setItem(row, 0, new QTableWidgetItem(host.ipAddress));
    m_resultsTable->setItem(row, 1, new QTableWidgetItem(host.hostName));
    m_resultsTable->setItem(row, 2, new QTableWidgetItem(host.macAddress));
    m_resultsTable->setItem(row, 3, new QTableWidgetItem(host.macVendor));
    m_resultsTable->setItem(row, 4, new QTableWidgetItem(host.isReachable ? "可达" : "不可达"));
    
    // 收集开放端口信息
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
    
    // 如果主机可达，设置背景色为浅绿色
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