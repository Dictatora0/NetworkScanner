//
// networkscanner.h
//

/**
 * @file networkscanner.h
 * @brief 网络扫描器类定义
 * @details 提供网络设备发现和端口扫描功能
 * @author Network Scanner Team
 * @version 2.1.0
 */

#ifndef NETWORKSCANNER_H
#define NETWORKSCANNER_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QTcpSocket>
#include <QDateTime>
#include <QNetworkInterface>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <QHostInfo>
#include <QThreadPool>
#include <QRunnable>

/**
 * @struct HostInfo
 * @brief 存储主机信息的结构体
 * @details 包含IP地址、主机名、MAC地址、厂商信息等扫描结果
 */
struct HostInfo {
    QString ipAddress;   ///< 主机IP地址
    QString hostName;    ///< 主机名称
    QString macAddress;  ///< MAC物理地址
    QString macVendor;   ///< MAC地址对应的厂商
    bool isReachable;    ///< 主机是否可达
    QDateTime scanTime;  ///< 扫描时间
    QMap<int, bool> openPorts; ///< 开放的端口及状态 (端口号 -> 是否开放)
};

/**
 * @class ScanStrategy
 * @brief 扫描策略类
 * @details 定义不同的扫描模式和参数，如快速扫描、标准扫描和深度扫描
 */
class ScanStrategy {
public:
    /**
     * @enum ScanMode
     * @brief 扫描模式枚举
     */
    enum ScanMode {
        QUICK_SCAN,      ///< 仅检测主机存活
        STANDARD_SCAN,   ///< 扫描常用端口
        DEEP_SCAN        ///< 全面端口扫描
    };
    
    /**
     * @brief 构造函数
     * @param mode 扫描模式，默认为标准扫描
     */
    ScanStrategy(ScanMode mode = STANDARD_SCAN);
    
    /**
     * @brief 获取要扫描的端口列表
     * @return 端口号列表
     */
    QList<int> getPortsToScan() const;
    
    /**
     * @brief 获取扫描超时时间
     * @param ip 目标IP地址
     * @return 超时时间（毫秒）
     */
    int getScanTimeout(const QString &ip) const;
    
    /**
     * @brief 获取最大并行任务数
     * @return 并行任务数量
     */
    int getMaxParallelTasks() const;
    
    /**
     * @brief 更新主机响应时间记录
     * @param ip 主机IP地址
     * @param responseTime 响应时间（毫秒）
     */
    void updateHostResponseTime(const QString &ip, int responseTime);
    
    /**
     * @brief 获取当前扫描模式
     * @return 扫描模式
     */
    ScanMode getMode() const { return m_mode; }
    
    /**
     * @brief 设置扫描模式
     * @param mode 要设置的扫描模式
     */
    void setMode(ScanMode mode) { m_mode = mode; }
    
private:
    ScanMode m_mode;  ///< 当前扫描模式
    QMap<QString, int> m_hostResponseTimes;  ///< IP地址 -> 响应时间映射
};

/**
 * @class ScanTask
 * @brief 扫描任务类
 * @details 用于并行执行的单个主机扫描任务
 */
class ScanTask : public QRunnable {
public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     * @param address 要扫描的地址
     * @param ports 要扫描的端口列表
     * @param timeout 连接超时时间（毫秒）
     */
    ScanTask(QObject* parent, const QHostAddress &address, 
             const QList<int> &ports, int timeout);
    
    /**
     * @brief 执行扫描任务
     * @details 实现QRunnable的抽象方法
     */
    void run() override;
    
    QObject* m_parent;          ///< 父对象指针
    QHostAddress m_address;     ///< 扫描地址
    QList<int> m_ports;         ///< 扫描端口列表
    int m_timeout;              ///< 超时时间
};

/**
 * @class NetworkScanner
 * @brief 网络扫描器类
 * @details 提供网络设备发现和端口扫描的核心功能
 */
class NetworkScanner : public QObject
{
    Q_OBJECT
    
public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    NetworkScanner(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~NetworkScanner();
    
    /**
     * @brief 设置自定义端口扫描列表
     * @param ports 要扫描的端口列表
     */
    void setCustomPortsToScan(const QList<int> &ports);
    
    /**
     * @brief 设置扫描超时时间
     * @param msecs 超时时间(毫秒)
     */
    void setScanTimeout(int msecs);
    
    /**
     * @brief 设置自定义IP范围
     * @param startIP 起始IP地址
     * @param endIP 结束IP地址
     */
    void setCustomIPRange(const QString &startIP, const QString &endIP);
    
    /**
     * @brief 设置扫描策略
     * @param mode 扫描模式
     */
    void setScanStrategy(ScanStrategy::ScanMode mode);
    
    /**
     * @brief 获取扫描结果
     * @return 扫描到的主机信息列表
     */
    QList<HostInfo> getScannedHosts() const;
    
    /**
     * @brief 保存结果到文件
     * @param filename 文件名
     */
    void saveResultsToFile(const QString &filename) const;
    
    /**
     * @brief 开始扫描
     */
    void startScan();
    
    /**
     * @brief 停止扫描
     */
    void stopScan();
    
    /**
     * @brief 检查是否正在扫描
     * @return 是否正在扫描
     */
    bool isScanning() const;
    
    /**
     * @brief 快速Ping扫描方法
     * @param addresses 要扫描的地址列表
     * @return 活跃的主机地址列表
     */
    QList<QHostAddress> quickPingScan(const QList<QHostAddress> &addresses);
    
    /**
     * @brief 检查主机是否可达
     * @param address 主机地址
     * @param timeout 超时时间(毫秒)
     * @return 主机是否可达
     */
    bool isHostReachable(const QHostAddress &address, int timeout);
    
    /**
     * @brief 检查主机在多个端口上是否可达
     * @param address 主机地址
     * @param ports 端口列表
     * @param timeout 超时时间(毫秒)
     * @return 是否至少有一个端口可达
     */
    bool isReachableOnPorts(const QHostAddress &address, const QList<int> &ports, int timeout);
    
    /**
     * @brief 扫描单个主机
     * @param address 主机地址
     */
    void scanHost(const QHostAddress &address);
    
    /**
     * @brief 查询主机名
     * @param address 主机地址
     * @return 主机名
     */
    QString lookupHostName(const QHostAddress &address);
    
    /**
     * @brief 查询MAC地址
     * @param address 主机地址
     * @return MAC地址
     */
    QString lookupMacAddress(const QHostAddress &address);
    
    /**
     * @brief 查询MAC地址对应的厂商
     * @param macAddress MAC地址
     * @return 厂商名称
     */
    QString lookupMacVendor(const QString &macAddress);
    
    /**
     * @brief 根据IP地址生成伪MAC地址
     * @param ip IP地址
     * @return 生成的伪MAC地址
     */
    QString generatePseudoMACFromIP(const QString &ip);
    
signals:
    /**
     * @brief 找到主机信号
     * @param host 主机信息
     */
    void hostFound(const HostInfo &host);
    
    /**
     * @brief 扫描开始信号
     */
    void scanStarted();
    
    /**
     * @brief 扫描完成信号
     */
    void scanFinished();
    
    /**
     * @brief 扫描进度更新信号
     * @param progress 进度百分比(0-100)
     */
    void scanProgress(int progress);
    
    /**
     * @brief 扫描错误信号
     * @param errorMessage 错误信息
     */
    void scanError(const QString &errorMessage);
    
public slots:
    /**
     * @brief 处理扫描任务完成
     * @param hostInfo 扫描到的主机信息
     */
    void onScanTaskFinished(const HostInfo &hostInfo);
    
    /**
     * @brief 更新扫描进度
     */
    void updateScanProgress();
    
    /**
     * @brief 主机名查询完成处理
     * @param hostInfo 主机信息
     */
    void onHostNameLookedUp(const QHostInfo &hostInfo);
    
private:
    /**
     * @brief 获取本地网络地址列表
     * @return 本地网络地址列表
     */
    QList<QHostAddress> getLocalNetworkAddresses();
    
    /**
     * @brief MAC地址规范化
     * @param macAddress 原始MAC地址
     * @return 规范化的MAC地址
     */
    QString normalizeMacAddress(const QString &macAddress);
    
    /**
     * @brief 使用ping命令检测目标是否可达
     * @param ip 目标IP地址
     * @param timeout 超时时间(毫秒)
     * @return 是否可达
     */
    bool pingTargetWithTimeout(const QString &ip, int timeout);
    
    /**
     * @brief 通过系统调用获取MAC地址
     * @param ip 目标IP地址
     * @return MAC地址
     */
    QString getMacAddressFromSystemCalls(const QString &ip);
    
    /**
     * @brief 执行ARP扫描
     * @return 地址和MAC地址对的列表
     */
    QList<QPair<QHostAddress, QString>> performARPScan();
    
    /**
     * @brief 扫描主机端口
     * @param hostInfo 要更新的主机信息结构
     */
    void scanHostPorts(HostInfo &hostInfo);
    
    /**
     * @brief 获取要扫描的地址列表
     * @return 地址列表
     */
    QList<QHostAddress> getAddressesToScan();
    
    /**
     * @brief 处理扫描结果
     */
    void processScanResults();
    
    bool m_isScanning;  ///< 是否正在扫描
    
    int m_scannedHosts;  ///< 已扫描主机数
    int m_totalHosts;    ///< 总主机数
    int m_scanTimeout;   ///< 扫描超时时间
    
    bool m_useCustomRange;       ///< 是否使用自定义IP范围
    QHostAddress m_startIPRange; ///< 起始IP地址
    QHostAddress m_endIPRange;   ///< 结束IP地址
    
    QList<int> m_portsToScan;  ///< 要扫描的端口列表
    
    QList<HostInfo> m_scannedHostsList;  ///< 扫描结果列表
    QMutex m_mutex;  ///< 线程同步互斥锁
    
    QList<QFuture<void>> m_scanFutures;  ///< 并行扫描任务
    QThreadPool m_threadPool;  ///< 线程池
    
    QMap<QString, QString> m_macAddressCache;  ///< MAC地址缓存
    
    ScanStrategy m_scanStrategy;  ///< 扫描策略
    
    QList<QHostAddress> m_activeHosts;  ///< 活跃主机列表
    
    /**
     * @brief 执行外部进程
     * @param program 程序路径
     * @param args 参数列表
     * @param stdOutOutput 标准输出结果
     * @param stdErrOutput 标准错误结果
     * @param startTimeout 启动超时(毫秒)
     * @param finishTimeout 完成超时(毫秒)
     * @return 是否执行成功
     */
    static bool executeProcess(const QString &program, const QStringList &args, QString &stdOutOutput, QString &stdErrOutput, int startTimeout = 2000, int finishTimeout = 5000);
};

#endif // NETWORKSCANNER_H 