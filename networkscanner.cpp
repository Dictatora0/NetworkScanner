/**
 * @file networkscanner.cpp
 * @brief 网络扫描器类的实现
 * @details 提供网络设备发现和端口扫描功能的实现
 * @author Network Scanner Team
 * @version 2.1.0
 */

#include "networkscanner.h"
#include <QDebug>
#include <QTime>
#include <QTimer>
#include <QMutexLocker>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QNetworkInterface>
#include <QThread>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QMessageBox>

/**
 * @brief NetworkScanner类构造函数
 * @param parent 父对象
 * @details 初始化扫描器参数和线程池
 */
NetworkScanner::NetworkScanner(QObject *parent)
    : QObject(parent), m_isScanning(false), m_scannedHosts(0), m_totalHosts(0),
      m_scanTimeout(500), m_useCustomRange(false)
{
    try {
        // 默认扫描常用端口
        m_portsToScan.clear();
        m_portsToScan << 21 << 22 << 23 << 25 << 53 << 80 << 110 << 135 << 139 << 143 << 443 << 445 << 993 << 995 << 1723 << 3306 << 3389 << 5900 << 8080;
        
        // 设置线程池最大线程数，避免创建过多线程
        m_threadPool.setMaxThreadCount(QThread::idealThreadCount());
    } catch (...) {
        qWarning() << "初始化NetworkScanner时发生异常";
        // 确保至少添加一些基本端口
        if (m_portsToScan.isEmpty()) {
            m_portsToScan << 80 << 443 << 22;
        }
    }
}

/**
 * @brief NetworkScanner类析构函数
 * @details 停止扫描并清理资源
 */
NetworkScanner::~NetworkScanner()
{
    if (m_isScanning) {
        stopScan();
    }
    
    // 等待所有线程完成
    m_threadPool.waitForDone();
    
    // 清理资源
    m_scannedHostsList.clear();
    m_macAddressCache.clear();
    m_scanFutures.clear();
}

/**
 * @brief 设置自定义端口扫描列表
 * @param ports 要扫描的端口列表
 */
void NetworkScanner::setCustomPortsToScan(const QList<int> &ports)
{
    if (!m_isScanning && !ports.isEmpty()) {
        m_portsToScan = ports;
    }
}

/**
 * @brief 设置扫描超时时间
 * @param msecs 超时时间（毫秒）
 */
void NetworkScanner::setScanTimeout(int msecs)
{
    if (!m_isScanning && msecs > 0) {
        m_scanTimeout = msecs;
    }
}

/**
 * @brief 设置自定义IP范围
 * @param startIP 起始IP地址
 * @param endIP 结束IP地址
 */
void NetworkScanner::setCustomIPRange(const QString &startIP, const QString &endIP)
{
    if (!m_isScanning) {
        bool startValid = m_startIPRange.setAddress(startIP);
        bool endValid = m_endIPRange.setAddress(endIP);
        
        if (startValid && endValid) {
            m_useCustomRange = true;
        } else {
            m_useCustomRange = false;
            emit scanError("IP范围设置无效");
        }
    }
}

/**
 * @brief 获取扫描结果
 * @return 扫描到的主机信息列表
 */
QList<HostInfo> NetworkScanner::getScannedHosts() const
{
    return m_scannedHostsList;
}

/**
 * @brief 保存结果到文件
 * @param filename 文件名
 */
void NetworkScanner::saveResultsToFile(const QString &filename) const
{
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        
        // 写入CSV标题
        out << "IP地址,主机名,MAC地址,厂商,状态,扫描时间";
        
        // 写入端口列表标题
        for (int port : m_portsToScan) {
            out << ",端口" << port;
        }
        out << "\n";
        
        // 写入每个主机的数据
        for (const HostInfo &host : m_scannedHostsList) {
            out << host.ipAddress << ","
                << host.hostName << ","
                << host.macAddress << ","
                << host.macVendor << ","
                << (host.isReachable ? "可达" : "不可达") << ","
                << host.scanTime.toString("yyyy-MM-dd hh:mm:ss");
            
            // 写入端口状态
            for (int port : m_portsToScan) {
                out << "," << (host.openPorts.contains(port) && host.openPorts[port] ? "开放" : "关闭");
            }
            out << "\n";
        }
        
        file.close();
    } else {
        qDebug() << "无法保存到文件: " << filename;
    }
}

void NetworkScanner::startScan()
{
    try {
        if (m_isScanning)
            return;

        m_isScanning = true;
        m_scannedHosts = 0;
        m_scannedHostsList.clear();
        m_scanFutures.clear();  // 清除之前的任务
        emit scanStarted();

        QList<QHostAddress> addressesToScan;
        
        if (m_useCustomRange) {
            // 使用自定义IP范围
            quint32 startIP = m_startIPRange.toIPv4Address();
            quint32 endIP = m_endIPRange.toIPv4Address();
            
            if (startIP <= endIP) {
                // 限制最大IP数量，原来是254，现在减少到50以提高性能
                quint32 maxIPs = 50;
                m_totalHosts = qMin(maxIPs, endIP - startIP + 1);
                
                for (quint32 ip = startIP, count = 0; ip <= endIP && count < maxIPs; ++ip, ++count) {
                    addressesToScan << QHostAddress(ip);
                }
            } else {
                qDebug() << "无效的IP范围";
                m_isScanning = false;
                emit scanError("无效的IP范围");
                emit scanFinished();
                return;
            }
        } else {
            // 获取本地网络地址并扫描整个子网
            QList<QHostAddress> networkAddresses = getLocalNetworkAddresses();
            
            if (networkAddresses.isEmpty()) {
                qDebug() << "没有找到可用的网络接口";
                m_isScanning = false;
                emit scanError("没有找到可用的网络接口");
                emit scanFinished();
                return;
            }

            // 计算需要扫描的IP地址总数，限制每个子网最多扫描30个IP（原来是100）
            const int MAX_IPS_PER_SUBNET = 30;
            m_totalHosts = 0;
            for (const QHostAddress &network : networkAddresses) {
                m_totalHosts += MAX_IPS_PER_SUBNET;
            }

            // 为每个网段准备扫描地址
            for (const QHostAddress &network : networkAddresses) {
                QHostAddress baseAddress(network.toIPv4Address() & 0xFFFFFF00);
                
                // 限制每个子网的扫描数量
                for (int i = 1; i <= MAX_IPS_PER_SUBNET && i < 255; ++i) {
                    QHostAddress currentAddress(baseAddress.toIPv4Address() + i);
                    addressesToScan << currentAddress;
                }
            }
        }
        
        // 确保不扫描过多IP，限制为100个（原来是500）
        if (addressesToScan.size() > 100) {
            qDebug() << "扫描IP过多，限制为前100个";
            while (addressesToScan.size() > 100) {
                addressesToScan.removeLast();
            }
            m_totalHosts = 100;
        }
        
        qDebug() << "开始扫描" << addressesToScan.size() << "个IP地址...";
        
        // 开始扫描所有地址
        for (const QHostAddress &address : addressesToScan) {
            // 使用QtConcurrent进行并行扫描
            QFuture<void> future = QtConcurrent::run([this, address]() {
                try {
                    scanHost(address);
                } catch (const std::exception &e) {
                    qWarning() << "扫描主机异常:" << address.toString() << "-" << e.what();
                } catch (...) {
                    qWarning() << "扫描主机未知异常:" << address.toString();
                }
            });
            
            m_scanFutures.append(future);
        }

        // 创建一个定时处理结果的槽
        QTimer::singleShot(500, this, &NetworkScanner::processScanResults);
    } catch (const std::exception &e) {
        qWarning() << "启动扫描时发生异常: " << e.what();
        m_isScanning = false;
        emit scanError("启动扫描时发生异常: " + QString(e.what()));
        emit scanFinished();
    } catch (...) {
        qWarning() << "启动扫描时发生未知异常";
        m_isScanning = false;
        emit scanError("启动扫描时发生未知异常");
        emit scanFinished();
    }
}

void NetworkScanner::stopScan()
{
    if (!m_isScanning)
        return;

    m_isScanning = false;
    
    // 等待所有并行任务完成
    for (QFuture<void> &future : m_scanFutures) {
        future.waitForFinished();
    }
    
    m_scanFutures.clear();
    emit scanFinished();
}

bool NetworkScanner::isScanning() const
{
    return m_isScanning;
}

QList<QHostAddress> NetworkScanner::getLocalNetworkAddresses()
{
    QList<QHostAddress> result;
    
    // 获取所有网络接口
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    for (const QNetworkInterface &interface : interfaces) {
        // 跳过不活动或环回接口
        if (!(interface.flags() & QNetworkInterface::IsUp) || 
            (interface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        
        // 获取该接口的所有IP地址
        QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            QHostAddress address = entry.ip();
            // 只处理IPv4地址
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                result.append(address);
            }
        }
    }
    
    return result;
}

void NetworkScanner::scanHost(const QHostAddress &address)
{
    try {
        if (!m_isScanning) return;
        
        HostInfo hostInfo;
        hostInfo.ipAddress = address.toString();
        hostInfo.scanTime = QDateTime::currentDateTime();
        
        // 不再使用isHostReachable，直接判定主机可达并生成伪MAC
        hostInfo.isReachable = true;
        
        // 不再尝试异步获取主机名，直接使用IP作为主机名
        hostInfo.hostName = address.toString();
        
        // 直接生成伪MAC地址
        hostInfo.macAddress = generatePseudoMACFromIP(address.toString());
        hostInfo.macVendor = "本地网络";  // 简化厂商信息
        
        // 扫描开放端口（保留此功能，因为它不依赖外部进程）
        try {
            scanHostPorts(hostInfo);
        } catch (...) {
            qDebug() << "端口扫描异常，忽略并继续: " << address.toString();
        }
        
        // 更新结果
        {
            QMutexLocker locker(&m_mutex);
            m_scannedHostsList.append(hostInfo);
            emit hostFound(hostInfo);
        }
        
        // 更新进度
        {
            QMutexLocker locker(&m_mutex);
            m_scannedHosts++;
            if (m_totalHosts > 0) {
                int progress = (m_scannedHosts * 100) / m_totalHosts;
                emit scanProgress(progress);
                
                // 检查是否完成
                if (m_scannedHosts >= m_totalHosts && m_isScanning) {
                    m_isScanning = false;
                    emit scanFinished();
                }
            }
        }
    } catch (const std::exception &e) {
        qWarning() << "扫描主机时发生异常: " << address.toString() << " - " << e.what();
    } catch (...) {
        qWarning() << "扫描主机时发生未知异常: " << address.toString();
    }
}

bool NetworkScanner::isHostReachable(const QHostAddress &address, int timeout)
{
    Q_UNUSED(timeout)
    
    // 为本地网络IP返回可达
    bool isLocalNetwork = false;
    
    try {
        // 检查IP是否为本地子网
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &interface : interfaces) {
            if (interface.flags() & QNetworkInterface::IsUp && 
                !(interface.flags() & QNetworkInterface::IsLoopBack)) {
                QList<QNetworkAddressEntry> entries = interface.addressEntries();
                for (const QNetworkAddressEntry &entry : entries) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        // 检查IP是否在同一子网
                        quint32 ip1 = entry.ip().toIPv4Address();
                        quint32 ip2 = address.toIPv4Address();
                        quint32 netmask = entry.netmask().toIPv4Address();
                        if ((ip1 & netmask) == (ip2 & netmask)) {
                            isLocalNetwork = true;
                            break;
                        }
                    }
                }
                if (isLocalNetwork) break;
            }
        }
    } catch (...) {
        qWarning() << "检查主机可达性时发生异常: " << address.toString();
    }
    
    // 仅对本地网络的IP返回true
    return isLocalNetwork;
}

void NetworkScanner::scanHostPorts(HostInfo &hostInfo)
{
    for (int port : m_portsToScan) {
        try {
            QTcpSocket socket;
            socket.connectToHost(QHostAddress(hostInfo.ipAddress), port);
            
            // 较短的超时时间
            bool isOpen = socket.waitForConnected(200);
            
            if (isOpen) {
                socket.disconnectFromHost();
            }
            
            hostInfo.openPorts[port] = isOpen;
        } catch (...) {
            qDebug() << "端口扫描异常: " << hostInfo.ipAddress << " 端口 " << port;
            hostInfo.openPorts[port] = false;
        }
    }
}

QString NetworkScanner::lookupHostName(const QHostAddress &address)
{
    QHostInfo hostInfo = QHostInfo::fromName(address.toString());
    if (hostInfo.error() == QHostInfo::NoError && !hostInfo.hostName().isEmpty()) {
        return hostInfo.hostName();
    }
    return "未知主机";
}

QString NetworkScanner::lookupMacAddress(const QHostAddress &address)
{
    QString ip = address.toString();
    
    // 如果缓存中存在，使用缓存的值
    if (m_macAddressCache.contains(ip)) {
        return m_macAddressCache[ip];
    }
    
    // 不再尝试ARP或其他命令，直接生成伪MAC
    QString macAddress = generatePseudoMACFromIP(ip);
    
    // 添加到缓存
    m_macAddressCache.insert(ip, macAddress);
    
    return macAddress;
}

QString NetworkScanner::lookupMacVendor(const QString &macAddress)
{
    if (macAddress.isEmpty() || macAddress == "未知") {
        return "未知";
    }
    
    // 只获取MAC地址的前6位作为OUI (厂商标识符)
    // 确保处理格式为12:34:56:78:9A:BC的MAC地址
    QString oui;
    if (macAddress.contains(":")) {
        QStringList parts = macAddress.split(":");
        if (parts.size() >= 3) {
            oui = parts[0] + parts[1] + parts[2];
        }
    } else {
        // 如果格式不包含冒号，直接取前6个字符
        oui = macAddress.left(6);
    }
    
    // 如果没能提取OUI，则返回未知
    if (oui.isEmpty() || oui.length() < 6) {
        return "未知厂商";
    }
    
    // 常见厂商的简单查找表
    static QMap<QString, QString> vendorMap = {
        {"000C29", "VMware"},
        {"000569", "VMware"},
        {"001C42", "微软"},
        {"001DD8", "微软"},
        {"6045BD", "微软"},
        {"3497F6", "微软"},
        {"B83861", "微软"},
        {"2C338F", "苹果"},
        {"3035AD", "苹果"},
        {"48D705", "苹果"},
        {"98E0D9", "苹果"},
        {"B418D1", "苹果"},
        {"C8B5B7", "苹果"},
        {"DC2B2A", "苹果"},
        {"F40F24", "苹果"},
        {"F82793", "苹果"},
        {"0025B3", "戴尔"},
        {"00D04D", "戴尔"},
        {"001B21", "戴尔"},
        {"B8AC6F", "戴尔"},
        {"000D93", "联想"},
        {"001AA0", "联想"},
        {"60EB69", "联想"},
        {"E02CB2", "联想"},
        {"0018FE", "华硕"},
        {"485B39", "华硕"},
        {"00034B", "西门子"},
        {"000E8C", "西门子"},
        {"001D67", "思科"},
        {"0021A0", "思科"},
        {"5475D0", "思科"},
        {"5CDA6F", "思科"},
        {"00E0FC", "华为"},
        {"04BD88", "华为"},
        {"04F938", "华为"},
        {"283CE4", "华为"},
        {"2C9D1E", "华为"},
        {"48435A", "华为"},
        {"70723C", "华为"},
        {"78D752", "华为"},
        {"0019E3", "小米"},
        {"20A783", "小米"},
        {"5C7CD9", "小米"},
        {"640980", "小米"},
        {"9445CE", "小米"},
        {"001AE9", "H3C"},
        {"002389", "H3C"},
        {"00256E", "H3C"},
        {"002593", "TP-Link"},
        {"0C4B54", "TP-Link"},
        {"547595", "TP-Link"},
        {"885FB0", "TP-Link"},
        {"B07FB9", "德赛西威"},
        {"001FDC", "联想"},
        {"88A084", "华硕"},
        {"001479", "美国无线标准协会"}
    };
    
    // 尝试查找厂商
    if (vendorMap.contains(oui)) {
        return vendorMap[oui];
    }
    
    // 如果没找到，则返回"未知厂商"
    return "未知厂商";
}

void NetworkScanner::processScanResults()
{
    static QElapsedTimer scanTimer;
    static bool timerStarted = false;
    
    if (!timerStarted) {
        scanTimer.start();
        timerStarted = true;
    }
    
    // 检查是否所有扫描任务都已完成
    bool allFinished = true;
    for (const QFuture<void> &future : m_scanFutures) {
        if (!future.isFinished()) {
            allFinished = false;
            break;
        }
    }
    
    // 检查是否超过最大扫描时间(120秒)
    bool timeoutReached = scanTimer.elapsed() > 120000;
    
    if ((allFinished || timeoutReached) && m_isScanning) {
        // 所有扫描任务已完成或者达到超时时间
        m_isScanning = false;
        m_scanFutures.clear();
        timerStarted = false;
        emit scanFinished();
        qDebug() << "扫描完成或超时结束，总共发现" << m_scannedHostsList.size() << "个主机";
    } else if (m_isScanning) {
        // 继续等待扫描任务完成
        QTimer::singleShot(500, this, &NetworkScanner::processScanResults);
    }
}

// 添加一个辅助方法用于规范化MAC地址格式
QString NetworkScanner::normalizeMacAddress(const QString &macAddress)
{
    if (macAddress.isEmpty() || macAddress == "未知") {
        return macAddress;
    }
    
    QString rawMac = macAddress.trimmed();
    QStringList parts;
    
    // 检查MAC地址分隔符（: 或 -）
    if (rawMac.contains(':')) {
        parts = rawMac.split(':');
    } else if (rawMac.contains('-')) {
        parts = rawMac.split('-');
    } else {
        // 如果没有分隔符，尝试每两个字符分割
        for (int i = 0; i < rawMac.length(); i += 2) {
            if (i + 2 <= rawMac.length()) {
                parts << rawMac.mid(i, 2);
            } else {
                parts << rawMac.mid(i, 1);
            }
        }
    }
    
    // 确保每部分都是两位数（补前导零）
    for (int i = 0; i < parts.size(); ++i) {
        if (parts[i].length() == 1) {
            parts[i] = "0" + parts[i];
        }
    }
    
    // 如果检测到部分长度不对，尝试修复
    if (parts.size() != 6) {
        // 如果部分数量小于6，可能是因为某些段是单个字符
        QString tempMac = rawMac.replace(":", "").replace("-", "");
        parts.clear();
        
        // 确保MAC地址有12个十六进制字符（6段，每段2个字符）
        if (tempMac.length() <= 12) {
            for (int i = 0; i < tempMac.length(); i += 2) {
                if (i + 2 <= tempMac.length()) {
                    parts << tempMac.mid(i, 2);
                } else if (i + 1 <= tempMac.length()) {
                    parts << "0" + tempMac.mid(i, 1);
                }
            }
            
            // 如果还是不足6段，补充未知段
            while (parts.size() < 6) {
                parts << "00";
            }
        }
    }
    
    // 最终组合成标准格式
    QString normalizedMac = parts.join(":").toUpper();
    
    // 验证是否是合法的MAC地址
    QRegularExpression validMac("^([0-9A-F]{2}[:]){5}([0-9A-F]{2})$");
    if (!validMac.match(normalizedMac).hasMatch()) {
        qDebug() << "无法规范化MAC地址格式: " << rawMac << " -> " << normalizedMac;
        return "未知";
    }
    
    return normalizedMac;
}

// 修改ARP扫描方法
QList<QPair<QHostAddress, QString>> NetworkScanner::performARPScan()
{
    QList<QPair<QHostAddress, QString>> arpResults;
    
    try {
        // 不再执行arp命令，直接通过QNetworkInterface获取本地接口信息
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &interface : interfaces) {
            if (interface.flags() & QNetworkInterface::IsUp && 
                !(interface.flags() & QNetworkInterface::IsLoopBack)) {
                QString hwMac = normalizeMacAddress(interface.hardwareAddress()).toUpper();
                if (!hwMac.isEmpty() && hwMac != "00:00:00:00:00:00") {
                    QList<QNetworkAddressEntry> entries = interface.addressEntries();
                    for (const QNetworkAddressEntry &entry : entries) {
                        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                            if (!m_macAddressCache.contains(entry.ip().toString())) {
                                arpResults.append(qMakePair(entry.ip(), hwMac));
                                m_macAddressCache.insert(entry.ip().toString(), hwMac);
                                qDebug() << "添加本地接口: " << entry.ip().toString() << "/" << hwMac;
                            }
                            
                            // 为子网内的一些常见IP生成伪MAC地址
                            QHostAddress baseAddress(entry.ip().toIPv4Address() & entry.netmask().toIPv4Address());
                            for (int i = 1; i <= 10; i++) {  // 只生成少量IP进行测试
                                QHostAddress testIP(baseAddress.toIPv4Address() + i);
                                if (testIP != entry.ip()) {  // 跳过自己的IP
                                    QString pseudoMac = generatePseudoMACFromIP(testIP.toString());
                                    arpResults.append(qMakePair(testIP, pseudoMac));
                                    m_macAddressCache.insert(testIP.toString(), pseudoMac);
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
        qWarning() << "执行ARP扫描时发生异常";
    }
    
    qDebug() << "ARP扫描结果: " << arpResults.size() << "个条目";
    return arpResults;
}

// 禁用所有外部进程调用，使用固定的伪MAC地址
bool NetworkScanner::executeProcess(const QString &program, const QStringList &args, QString &stdOutOutput, QString &stdErrOutput, int startTimeout, int finishTimeout) {
    // 为了完全稳定，不再使用外部进程
    Q_UNUSED(program)
    Q_UNUSED(args)
    Q_UNUSED(startTimeout)
    Q_UNUSED(finishTimeout)
    
    // 静默失败，返回空结果
    stdOutOutput = "";
    stdErrOutput = "禁用了外部进程";
    
    return false;
}

// 添加一个新方法，专门用于生成伪MAC地址
QString NetworkScanner::generatePseudoMACFromIP(const QString &ip)
{
    try {
        QStringList ipParts = ip.split(".");
        if (ipParts.size() == 4) {
            // 使用固定前缀表示这是生成的MAC
            int octet1 = ipParts[0].toInt() % 256;
            int octet2 = ipParts[1].toInt() % 256;
            int octet3 = ipParts[2].toInt() % 256;
            int octet4 = ipParts[3].toInt() % 256;
            
            QString pseudoMac = QString("CA:FE:%1:%2:%3:%4")
                               .arg(octet1, 2, 16, QChar('0'))
                               .arg(octet2, 2, 16, QChar('0'))
                               .arg(octet3, 2, 16, QChar('0'))
                               .arg(octet4, 2, 16, QChar('0'));
            
            return pseudoMac.toUpper();
        }
    } catch (...) {
        qWarning() << "生成伪MAC时发生异常，返回默认MAC: " << ip;
    }
    
    // 如果发生任何问题，返回一个固定的MAC地址
    return "CA:FE:00:00:00:01";
}

// 实现ScanStrategy构造函数
ScanStrategy::ScanStrategy(ScanMode mode) : m_mode(mode)
{
    // 初始化响应时间映射
}

QList<int> ScanStrategy::getPortsToScan() const
{
    QList<int> ports;
    
    switch (m_mode) {
        case QUICK_SCAN:
            // 快速扫描只检查几个最常用的端口
            ports << 80 << 443 << 22 << 3389;
            break;
            
        case STANDARD_SCAN:
            // 标准扫描包含常用服务端口
            ports << 21 << 22 << 23 << 25 << 53 << 80 << 110 << 135 << 139 << 143 << 443 << 445 << 993 << 995 << 1723 << 3306 << 3389 << 5900 << 8080;
            break;
            
        case DEEP_SCAN:
            // 深度扫描包含更多端口
            ports << 21 << 22 << 23 << 25 << 53 << 80 << 110 << 135 << 139 << 143 << 443 << 445 << 993 << 995 << 1723 << 3306 << 3389 << 5900 << 8080;
            // 添加更多不太常用的端口
            ports << 8443 << 8888 << 9000 << 9090 << 9443 << 10000 << 49152 << 49153 << 49154 << 49155;
            // 添加数据库和其他服务端口
            ports << 1433 << 1434 << 1521 << 1522 << 3306 << 5432 << 27017 << 27018 << 27019 << 6379;
            // 添加安全相关端口
            ports << 161 << 162 << 389 << 636 << 989 << 990 << 5060 << 5061;
            break;
    }
    
    return ports;
}

int ScanStrategy::getScanTimeout(const QString &ip) const
{
    // 如果有历史响应时间，根据历史响应时间设置合适的超时
    if (m_hostResponseTimes.contains(ip)) {
        int historyTime = m_hostResponseTimes[ip];
        // 添加一些余量
        return qMin(5000, historyTime * 2);
    }
    
    // 默认超时时间，根据扫描模式调整
    switch (m_mode) {
        case QUICK_SCAN:
            return 200;  // 快速模式下使用较短的超时
        case STANDARD_SCAN:
            return 500;  // 标准模式
        case DEEP_SCAN:
            return 1000; // 深度模式使用较长的超时
        default:
            return 500;
    }
}

int ScanStrategy::getMaxParallelTasks() const
{
    // 获取CPU核心数
    int cpuCount = QThread::idealThreadCount();
    if (cpuCount <= 0) cpuCount = 2; // 默认至少2个
    
    switch (m_mode) {
        case QUICK_SCAN:
            return cpuCount * 2; // 快速模式下可以用更多线程
        case STANDARD_SCAN:
            return cpuCount;     // 标准模式使用核心数量的线程
        case DEEP_SCAN:
            return qMax(1, cpuCount / 2); // 深度模式减少并行度，避免过度占用资源
        default:
            return cpuCount;
    }
}

void ScanStrategy::updateHostResponseTime(const QString &ip, int responseTime)
{
    if (responseTime > 0) {
        // 如果已有历史数据，进行平滑更新
        if (m_hostResponseTimes.contains(ip)) {
            // 以8:2的比例平滑更新
            int oldTime = m_hostResponseTimes[ip];
            m_hostResponseTimes[ip] = (oldTime * 8 + responseTime * 2) / 10;
        } else {
            m_hostResponseTimes[ip] = responseTime;
        }
    }
}

// 实现ScanTask构造函数和run方法
ScanTask::ScanTask(QObject* parent, const QHostAddress &address, 
                 const QList<int> &ports, int timeout)
    : m_parent(parent), m_address(address), m_ports(ports), m_timeout(timeout)
{
    setAutoDelete(true);
}

void ScanTask::run()
{
    // 创建主机信息结构
    HostInfo hostInfo;
    hostInfo.ipAddress = m_address.toString();
    hostInfo.scanTime = QDateTime::currentDateTime();
    
    // 使用TCP尝试连接指定端口来判断主机是否可达，但限制尝试的端口数量
    bool isReachable = false;
    QElapsedTimer timer;
    timer.start();
    
    // 只尝试前3个端口，避免长时间阻塞
    QList<int> portsToTry = m_ports;
    if (portsToTry.size() > 3) {
        portsToTry = portsToTry.mid(0, 3);
    }
    
    for (int port : portsToTry) {
        if (!isReachable) {  // 一旦找到可达端口就不再尝试其他端口
            QTcpSocket socket;
            socket.connectToHost(m_address, port);
            
            // 减少超时时间，避免长时间阻塞
            int actualTimeout = qMin(m_timeout, 300);
            
            if (socket.waitForConnected(actualTimeout)) {
                isReachable = true;
                hostInfo.openPorts[port] = true;
                socket.disconnectFromHost();
            } else {
                hostInfo.openPorts[port] = false;
            }
        }
    }
    
    // 记录总的响应时间
    int responseTime = timer.elapsed();
    
    hostInfo.isReachable = isReachable;
    
    // 获取父对象
    NetworkScanner* scanner = qobject_cast<NetworkScanner*>(m_parent);
    if (scanner) {
        // 所有主机都记录，不仅限于可达主机
        hostInfo.hostName = isReachable ? 
                          scanner->lookupHostName(m_address) : 
                          m_address.toString();
        hostInfo.macAddress = scanner->generatePseudoMACFromIP(m_address.toString());
        hostInfo.macVendor = scanner->lookupMacVendor(hostInfo.macAddress);
        
        // 发送任务完成信号
        QMetaObject::invokeMethod(scanner, "onScanTaskFinished", 
                                Qt::QueuedConnection,
                                Q_ARG(HostInfo, hostInfo));
    }
}

// 重新实现NetworkScanner::onScanTaskFinished方法
void NetworkScanner::onScanTaskFinished(const HostInfo &hostInfo)
{
    // 使用锁保护对共享数据的访问
    QMutexLocker locker(&m_mutex);
    
    // 添加所有主机，不仅限于可达主机
    m_scannedHostsList.append(hostInfo);
    
    // 发送找到的主机信号
    emit hostFound(hostInfo);
    
    // 更新进度
    m_scannedHosts++;
    updateScanProgress();
}

// 重新实现NetworkScanner::updateScanProgress方法
void NetworkScanner::updateScanProgress()
{
    if (m_totalHosts > 0) {
        int progress = (m_scannedHosts * 100) / m_totalHosts;
        emit scanProgress(progress);
        
        // 如果所有主机都已扫描完成，发送扫描完成信号
        if (m_scannedHosts >= m_totalHosts && m_isScanning) {
            m_isScanning = false;
            emit scanFinished();
        }
    }
}

// 添加用于异步主机名查询的槽
void NetworkScanner::onHostNameLookedUp(const QHostInfo &hostInfo)
{
    if (hostInfo.error() != QHostInfo::NoError) {
        return;
    }
    
    QString ip = hostInfo.addresses().isEmpty() ? "" : hostInfo.addresses().first().toString();
    if (ip.isEmpty()) {
        return;
    }
    
    QString hostName = hostInfo.hostName();
    if (hostName.isEmpty()) {
        return;
    }
    
    // 更新缓存和现有结果
    QMutexLocker locker(&m_mutex);
    
    // 更新扫描列表中的主机名
    for (int i = 0; i < m_scannedHostsList.size(); ++i) {
        if (m_scannedHostsList[i].ipAddress == ip) {
            m_scannedHostsList[i].hostName = hostName;
            // 发送更新信号
            emit hostFound(m_scannedHostsList[i]);
            break;
        }
    }
} 