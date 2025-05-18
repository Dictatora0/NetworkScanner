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

NetworkScanner::NetworkScanner(QObject *parent)
    : QObject(parent), m_isScanning(false), m_scannedHosts(0), m_totalHosts(0),
      m_scanTimeout(500), m_useCustomRange(false)
{
    // 默认扫描常用端口
    m_portsToScan << 21 << 22 << 23 << 25 << 53 << 80 << 110 << 135 << 139 << 143 << 443 << 445 << 993 << 995 << 1723 << 3306 << 3389 << 5900 << 8080;
}

NetworkScanner::~NetworkScanner()
{
    stopScan();
}

void NetworkScanner::setCustomPortsToScan(const QList<int> &ports)
{
    if (!m_isScanning && !ports.isEmpty()) {
        m_portsToScan = ports;
    }
}

void NetworkScanner::setScanTimeout(int msecs)
{
    if (!m_isScanning && msecs > 0) {
        m_scanTimeout = msecs;
    }
}

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

QList<HostInfo> NetworkScanner::getScannedHosts() const
{
    return m_scannedHostsList;
}

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
    if (m_isScanning)
        return;

    m_isScanning = true;
    m_scannedHosts = 0;
    m_scannedHostsList.clear();
    emit scanStarted();

    QList<QHostAddress> addressesToScan;
    
    if (m_useCustomRange) {
        // 使用自定义IP范围
        quint32 startIP = m_startIPRange.toIPv4Address();
        quint32 endIP = m_endIPRange.toIPv4Address();
        
        if (startIP <= endIP) {
            m_totalHosts = endIP - startIP + 1;
            
            for (quint32 ip = startIP; ip <= endIP; ++ip) {
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

        // 计算需要扫描的IP地址总数
        m_totalHosts = 0;
        for (const QHostAddress &network : networkAddresses) {
            // 假设是一个标准的C类网络(/24)，有254个可扫描地址
            m_totalHosts += 254;
        }

        // 为每个网段准备扫描地址
        for (const QHostAddress &network : networkAddresses) {
            QHostAddress baseAddress(network.toIPv4Address() & 0xFFFFFF00);
            
            // 遍历该网段的所有可能IP地址(1-254)
            for (int i = 1; i < 255; ++i) {
                QHostAddress currentAddress(baseAddress.toIPv4Address() + i);
                addressesToScan << currentAddress;
            }
        }
    }
    
    // 开始扫描所有地址
    for (const QHostAddress &address : addressesToScan) {
        // 使用QtConcurrent进行并行扫描
        QFuture<void> future = QtConcurrent::run([this, address]() {
            scanHost(address);
        });
        
        m_scanFutures.append(future);
    }

    // 创建一个定时处理结果的槽
    QTimer::singleShot(500, this, &NetworkScanner::processScanResults);
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
    HostInfo hostInfo;
    hostInfo.ipAddress = address.toString();
    hostInfo.isReachable = pingHost(address);
    hostInfo.scanTime = QDateTime::currentDateTime();
    
    if (hostInfo.isReachable) {
        // 只对可达的主机进行主机名查询和MAC地址查询
        hostInfo.hostName = lookupHostName(address);
        hostInfo.macAddress = lookupMacAddress(address);
        
        // 查询MAC地址厂商信息
        if (hostInfo.macAddress != "未知") {
            hostInfo.macVendor = lookupMacVendor(hostInfo.macAddress);
        } else {
            hostInfo.macVendor = "未知";
        }
        
        // 扫描开放端口
        scanHostPorts(hostInfo);
        
        // 将主机信息添加到结果列表
        QMutexLocker locker(&m_mutex);
        m_scannedHostsList.append(hostInfo);
        
        // 发送找到的主机信号
        emit hostFound(hostInfo);
    }
    
    // 更新进度
    QMutexLocker locker(&m_mutex);
    m_scannedHosts++;
    int progress = (m_scannedHosts * 100) / m_totalHosts;
    emit scanProgress(progress);
}

bool NetworkScanner::pingHost(const QHostAddress &address)
{
    // 使用QTcpSocket尝试连接常见端口来判断主机是否可达
    for (int port : m_portsToScan) {
        QTcpSocket socket;
        socket.connectToHost(address, port);
        
        // 等待指定的超时时间
        if (socket.waitForConnected(m_scanTimeout)) {
            socket.disconnectFromHost();
            return true;
        }
    }
    
    return false;
}

void NetworkScanner::scanHostPorts(HostInfo &hostInfo)
{
    for (int port : m_portsToScan) {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress(hostInfo.ipAddress), port);
        
        // 等待指定的超时时间
        bool isOpen = socket.waitForConnected(m_scanTimeout);
        
        if (isOpen) {
            socket.disconnectFromHost();
        }
        
        hostInfo.openPorts[port] = isOpen;
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
    // 首先尝试使用QNetworkInterface获取MAC地址（最可靠的方法）
    QString macAddress = "未知";
    QString targetIP = address.toString();
    
    // 方法1: 使用QNetworkInterface直接查找
    // 这主要适用于本机网络接口
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (entry.ip().toString() == targetIP) {
                QString mac = interface.hardwareAddress();
                if (!mac.isEmpty()) {
                    return mac.toUpper();
                }
            }
        }
    }
    
    // 方法2: 使用系统arp命令
#ifdef Q_OS_MACOS
    QProcess process;
    process.start("/usr/sbin/arp", QStringList() << "-n" << targetIP);
    process.waitForFinished(2000);
    
    QString output = process.readAllStandardOutput();
    
    // 记录调试信息
    qDebug() << "ARP命令输出: " << output;
    
    // macOS的arp输出通常看起来像:
    // ? (192.168.1.1) at 11:22:33:44:55:66 on en0 ifscope [ethernet]
    // 改进的MAC地址正则表达式，能处理不规则MAC地址格式
    QRegularExpression macRegex("at\\s+([0-9A-Fa-f]{1,2}:[0-9A-Fa-f]{1,2}:[0-9A-Fa-f]{1,2}:[0-9A-Fa-f]{1,2}:[0-9A-Fa-f]{1,2}:[0-9A-Fa-f]{1,2})");
    QRegularExpressionMatch match = macRegex.match(output);
    if (match.hasMatch()) {
        macAddress = match.captured(1).toUpper();
        
        // 标准化MAC地址格式，确保每段都是两位
        QStringList parts = macAddress.split(":");
        QStringList standardizedParts;
        for (const QString &part : parts) {
            if (part.length() == 1) {
                standardizedParts << "0" + part;
            } else {
                standardizedParts << part;
            }
        }
        if (standardizedParts.size() == 6) {
            return standardizedParts.join(":");
        }
        return macAddress;
    }
    
    // 第二种格式尝试
    QStringList lines = output.split("\n");
    for (const QString &line : lines) {
        if (line.contains(targetIP)) {
            QStringList parts = line.split(" ", Qt::SkipEmptyParts);
            for (int i = 0; i < parts.size(); ++i) {
                QString part = parts[i];
                // 尝试匹配MAC地址格式: xx:xx:xx:xx:xx:xx
                if (part.count(":") == 5 || part.count("-") == 5) {
                    // 标准化MAC地址格式
                    QStringList macParts = part.split(part.contains(":") ? ":" : "-");
                    QStringList standardizedParts;
                    for (const QString &macPart : macParts) {
                        if (macPart.length() == 1) {
                            standardizedParts << "0" + macPart;
                        } else {
                            standardizedParts << macPart;
                        }
                    }
                    if (standardizedParts.size() == 6) {
                        return standardizedParts.join(":");
                    }
                    return part.toUpper().replace("-", ":");
                }
            }
        }
    }
    
    // 最后尝试手动解析
    if (lines.size() >= 2) {
        QString line = lines[1];
        QStringList parts = line.split(" ", Qt::SkipEmptyParts);
        
        if (parts.size() >= 3) {
            for (const QString &part : parts) {
                if (part.count(":") == 5 || part.count("-") == 5) {
                    macAddress = part.toUpper();
                    if (macAddress.contains("-")) {
                        macAddress = macAddress.replace("-", ":");
                    }
                    return macAddress;
                }
            }
        }
    }
#endif

#ifdef Q_OS_LINUX
    QProcess process;
    process.start("/usr/sbin/arp", QStringList() << "-n" << targetIP);
    process.waitForFinished(2000);
    
    QString output = process.readAllStandardOutput();
    
    // Linux的arp输出通常看起来像:
    // Address                  HWtype  HWaddress           Flags Mask            Iface
    // 192.168.1.1              ether   11:22:33:44:55:66   C                     wlan0
    QStringList lines = output.split("\n");
    for (const QString &line : lines) {
        if (line.contains(targetIP)) {
            QStringList parts = line.split(" ", Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                if (part.count(":") == 5) {
                    return part.toUpper();
                }
            }
        }
    }
#endif

#ifdef Q_OS_WIN
    QProcess process;
    process.start("arp", QStringList() << "-a" << targetIP);
    process.waitForFinished(2000);
    
    QString output = process.readAllStandardOutput();
    
    // Windows的arp输出通常看起来像:
    // Interface: 192.168.1.2 --- 0x4
    //   Internet Address      Physical Address      Type
    //   192.168.1.1           11-22-33-44-55-66     dynamic
    QStringList lines = output.split("\n");
    for (const QString &line : lines) {
        if (line.contains(targetIP)) {
            QRegularExpression macRegex("([0-9A-Fa-f]{1,2}-[0-9A-Fa-f]{1,2}-[0-9A-Fa-f]{1,2}-[0-9A-Fa-f]{1,2}-[0-9A-Fa-f]{1,2}-[0-9A-Fa-f]{1,2})");
            QRegularExpressionMatch match = macRegex.match(line);
            if (match.hasMatch()) {
                macAddress = match.captured(1).toUpper().replace("-", ":");
                return macAddress;
            }
            
            // 备用解析方法
            QStringList parts = line.split(" ", Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                if (part.count("-") == 5) {
                    return part.toUpper().replace("-", ":");
                }
            }
        }
    }
#endif

    // 如果上述方法都失败，返回"未知"
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
    // 检查是否所有扫描任务都已完成
    bool allFinished = true;
    for (const QFuture<void> &future : m_scanFutures) {
        if (!future.isFinished()) {
            allFinished = false;
            break;
        }
    }
    
    if (allFinished && m_isScanning) {
        // 所有扫描任务已完成
        m_isScanning = false;
        m_scanFutures.clear();
        emit scanFinished();
    } else if (m_isScanning) {
        // 继续等待扫描任务完成
        QTimer::singleShot(500, this, &NetworkScanner::processScanResults);
    }
} 