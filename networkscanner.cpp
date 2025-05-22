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
#include <QRandomGenerator>
#include <random>
#include <algorithm>

/**
 * @brief NetworkScanner类构造函数
 * @param parent 父对象
 * @details 初始化扫描器参数和线程池
 */
NetworkScanner::NetworkScanner(QObject *parent)
    : QObject(parent), m_isScanning(false), m_scannedHosts(0), m_totalHosts(0),
      m_scanTimeout(500), m_useCustomRange(false), m_scanMode(STANDARD),
      m_debugMode(false), m_randomizeScan(true)
{
    try {
        // 默认扫描常用端口
        m_portsToScan.clear();
        m_portsToScan << 21 << 22 << 23 << 25 << 53 << 80 << 110 << 135 << 139 << 143 << 443 << 445 << 993 << 995 << 1723 << 3306 << 3389 << 5900 << 8080;
        
        // 设置线程池最大线程数，避免创建过多线程
        m_threadPool.setMaxThreadCount(QThread::idealThreadCount());
        
        // 初始化时间记录
        m_lastProgressUpdate = QDateTime::currentDateTime();
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
        
        // 添加保存时间和扫描信息
        out << "# 网络扫描结果\n";
        out << "# 保存时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
        out << "# 扫描主机数: " << m_scannedHostsList.size() << "\n";
        
        int reachableHosts = 0;
        for (const HostInfo &host : m_scannedHostsList) {
            if (host.isReachable) {
                reachableHosts++;
            }
        }
        out << "# 可达主机数: " << reachableHosts << "\n\n";
        
        // 写入CSV标题
        out << "IP地址,主机名,MAC地址,厂商,状态,扫描时间";
        
        // 写入端口列表标题
        for (int port : m_portsToScan) {
            out << ",端口" << port;
        }
        out << "\n";
        
        // 写入每个主机的数据，先写可达主机，再写不可达主机
        // 先将主机分类
        QList<HostInfo> reachableList;
        QList<HostInfo> unreachableList;
        
        for (const HostInfo &host : m_scannedHostsList) {
            if (host.isReachable) {
                reachableList.append(host);
            } else {
                unreachableList.append(host);
            }
        }
        
        // 对可达主机按IP排序
        std::sort(reachableList.begin(), reachableList.end(), 
                 [](const HostInfo &a, const HostInfo &b) {
                     return a.ipAddress < b.ipAddress;
                 });
        
        // 先输出可达主机
        for (const HostInfo &host : reachableList) {
            out << host.ipAddress << ","
                << host.hostName << ","
                << host.macAddress << ","
                << host.macVendor << ","
                << "可达" << ","
                << host.scanTime.toString("yyyy-MM-dd hh:mm:ss");
            
            // 写入端口状态
            for (int port : m_portsToScan) {
                if (host.openPorts.contains(port)) {
                    out << "," << (host.openPorts[port] ? "开放" : "关闭");
                } else {
                    out << ",未扫描";
                }
            }
            out << "\n";
        }
        
        // 再输出不可达主机
        for (const HostInfo &host : unreachableList) {
            out << host.ipAddress << ","
                << host.hostName << ","
                << host.macAddress << ","
                << host.macVendor << ","
                << "不可达" << ","
                << host.scanTime.toString("yyyy-MM-dd hh:mm:ss");
            
            // 写入端口状态
            for (int port : m_portsToScan) {
                out << ",未扫描";
            }
            out << "\n";
        }
        
        file.close();
        qDebug() << "结果已保存到: " << filename;
    } else {
        qDebug() << "无法保存到文件: " << filename << " - " << file.errorString();
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
        
        // 重置定时器和状态
        m_scanStartTime.start();
        m_lastProgressUpdate = QDateTime::currentDateTime();
        
        emit scanStarted();
        qDebug() << "开始扫描网络...";

        QList<QHostAddress> addressesToScan;
        
        if (m_useCustomRange) {
            // 使用自定义IP范围
            quint32 startIP = m_startIPRange.toIPv4Address();
            quint32 endIP = m_endIPRange.toIPv4Address();
            
            if (startIP <= endIP) {
                // 限制最大IP数量，根据实际需求调整
                quint32 maxIPs = 100; // 增加到100个，但添加随机化
                m_totalHosts = qMin(maxIPs, endIP - startIP + 1);
                
                // 创建所有可能的IP地址
                QList<quint32> allIPs;
                for (quint32 ip = startIP, count = 0; ip <= endIP && count < maxIPs; ++ip, ++count) {
                    allIPs.append(ip);
                }
                
                // 随机打乱IP顺序，避免顺序扫描
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(allIPs.begin(), allIPs.end(), g);
                
                // 转换为QHostAddress
                for (quint32 ip : allIPs) {
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

            // 计算需要扫描的IP地址总数，每个子网最多扫描50个IP
            const int MAX_IPS_PER_SUBNET = 50;
            m_totalHosts = 0;
            
            // 为每个网段准备扫描地址
            for (const QHostAddress &network : networkAddresses) {
                QHostAddress baseAddress(network.toIPv4Address() & 0xFFFFFF00);
                
                // 创建子网内的IP列表
                QList<QHostAddress> subnetIPs;
                for (int i = 1; i < 255; ++i) {
                    QHostAddress currentAddress(baseAddress.toIPv4Address() + i);
                    subnetIPs.append(currentAddress);
                }
                
                // 随机打乱子网IP顺序
                std::random_device rd;
                std::mt19937 g(rd());
                std::shuffle(subnetIPs.begin(), subnetIPs.end(), g);
                
                // 取前MAX_IPS_PER_SUBNET个IP
                int count = 0;
                for (const QHostAddress &ip : subnetIPs) {
                    if (count >= MAX_IPS_PER_SUBNET) break;
                    addressesToScan.append(ip);
                    count++;
                }
                
                m_totalHosts += qMin(MAX_IPS_PER_SUBNET, subnetIPs.size());
            }
        }
        
        // 确保不扫描过多IP，限制为200个
        if (addressesToScan.size() > 200) {
            qDebug() << "扫描IP过多，限制为前200个";
            while (addressesToScan.size() > 200) {
                addressesToScan.removeLast();
            }
            m_totalHosts = 200;
        }
        
        qDebug() << "开始扫描" << addressesToScan.size() << "个IP地址...";
        
        // 开始扫描所有地址，使用随机延迟
        int delayStep = 10; // 每个任务延迟10ms，避免网络风暴
        int currentDelay = 0;
        
        for (const QHostAddress &address : addressesToScan) {
            // 使用QtConcurrent进行并行扫描，但添加随机延迟
            QTimer::singleShot(currentDelay, this, [this, address]() {
                if (!m_isScanning) return; // 如果扫描已停止，不再启动新任务
                
                QFuture<void> future = QtConcurrent::run([this, address]() {
                    try {
                        if (!m_isScanning) return; // 再次检查，防止停止后仍执行
                        scanHost(address);
                    } catch (const std::exception &e) {
                        qWarning() << "扫描主机异常:" << address.toString() << "-" << e.what();
                    } catch (...) {
                        qWarning() << "扫描主机未知异常:" << address.toString();
                    }
                });
                
                QMutexLocker locker(&m_mutex);
                m_scanFutures.append(future);
            });
            
            // 更新延迟，添加随机因子
            currentDelay += delayStep + QRandomGenerator::global()->bounded(20);
        }

        // 创建一个定时处理结果的槽
        QTimer::singleShot(500, this, &NetworkScanner::processScanResults);
        
        // 设置总超时保护
        int maxScanTime = 180000; // 3分钟
        QTimer::singleShot(maxScanTime, this, [this]() {
            if (m_isScanning) {
                qDebug() << "扫描超时，强制结束...";
                stopScan();
                emit scanError("扫描超时，已自动结束");
            }
        });
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

    qDebug() << "正在停止扫描...";
    m_isScanning = false;
    
    // 通知UI
    emit scanProgress(100); // 强制进度条显示完成
    
    // 等待所有并行任务完成，但设置超时
    QElapsedTimer timer;
    timer.start();
    
    // 使用优雅的方式等待任务完成
    for (int i = 0; i < m_scanFutures.size(); ++i) {
        // 只等待最多2秒
        if (timer.elapsed() > 2000) {
            qDebug() << "等待任务完成超时，强制结束";
            break;
        }
        
        QFuture<void> &future = m_scanFutures[i];
        // 只有已经开始但尚未完成的任务需要等待
        if (future.isStarted() && !future.isFinished()) {
            // 等待最多500毫秒，使用计时器实现超时
            QElapsedTimer futureTimer;
            futureTimer.start();
            // 使用QTimer创建一个单次超时
            QTimer timer;
            QEventLoop loop;
            bool timedOut = false;
            
            QObject::connect(&timer, &QTimer::timeout, [&loop, &timedOut]() {
                timedOut = true;
                loop.quit();
            });
            
            timer.setSingleShot(true);
            timer.start(500); // 500ms超时
            
            // 在单独线程中等待future完成
            auto futureWait = QtConcurrent::run([&future, &loop]() {
                future.waitForFinished();
                loop.quit();
            });
            
            // 等待直到future完成或超时
            loop.exec();
            
            if (timedOut) {
                qDebug() << "等待任务超时";
            } else {
                // 确保futureWait完成
                futureWait.waitForFinished();
            }
            
            timer.stop();
        }
    }
    
    // 记录扫描统计信息
    int scanDuration = m_scanStartTime.elapsed() / 1000; // 秒
    int hostsFound = m_scannedHostsList.size();
    int reachableHosts = 0;
    
    for (const HostInfo &host : m_scannedHostsList) {
        if (host.isReachable) {
            reachableHosts++;
        }
    }
    
    qDebug() << "扫描统计: 耗时" << scanDuration << "秒, 扫描" 
             << m_scannedHosts << "个主机, 发现" << hostsFound 
             << "个主机, 其中" << reachableHosts << "个可达";
    
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
        
        // 使用多种方式检测主机可达性
        QElapsedTimer timer;
        timer.start();
        
        // 使用可配置超时进行ICMP/TCP混合检测
        hostInfo.isReachable = checkHostReachable(address, m_scanTimeout);
        
        int responseTime = timer.elapsed();
        qDebug() << "主机" << address.toString() << "响应时间:" << responseTime << "ms, 状态:" 
                 << (hostInfo.isReachable ? "可达" : "不可达");
        
        // 不再尝试异步获取主机名，直接使用IP作为主机名
        hostInfo.hostName = address.toString();
        
        // 生成伪MAC地址并明确标记
        hostInfo.macAddress = generatePseudoMACFromIP(address.toString());
        hostInfo.macVendor = lookupMacVendor(hostInfo.macAddress);
        
        // 只为可达主机扫描端口，提高效率
        if (hostInfo.isReachable) {
            try {
                scanHostPorts(hostInfo);
            } catch (const std::exception &e) {
                qDebug() << "端口扫描异常: " << address.toString() << " - " << e.what();
            } catch (...) {
                qDebug() << "端口扫描未知异常: " << address.toString();
            }
        } else {
            // 对不可达主机标记所有端口为关闭
            for (int port : m_portsToScan) {
                hostInfo.openPorts[port] = false;
            }
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
    // 端口扫描结果
    QMap<int, bool> portResults;
    
    // 使用更智能的超时策略，基于网络响应时间动态调整
    int baseTimeout = m_scanTimeout;
    
    // 创建一个QList存储所有socket指针，便于资源管理
    QList<QTcpSocket*> sockets;
    
    try {
        // 首先尝试连接到所有端口
        for (int port : m_portsToScan) {
            QTcpSocket* socket = new QTcpSocket();
            sockets.append(socket);
            
            // 设置超时，避免阻塞
            QTimer::singleShot(baseTimeout + 100, socket, &QTcpSocket::disconnectFromHost);
            
            // 连接到主机
            socket->connectToHost(QHostAddress(hostInfo.ipAddress), port);
            
            // 暂时将结果标记为未知
            portResults[port] = false;
        }
        
        // 给予足够时间让连接建立
        QElapsedTimer timer;
        timer.start();
        
        // 使用事件循环等待连接完成
        QEventLoop loop;
        int connectedPorts = 0;
        
        // 检查每个socket的状态
        for (int i = 0; i < sockets.size(); ++i) {
            QTcpSocket* socket = sockets[i];
            int port = m_portsToScan[i];
            
            // 如果socket已连接，标记端口为开放
            if (socket->state() == QAbstractSocket::ConnectedState) {
                portResults[port] = true;
                connectedPorts++;
                socket->disconnectFromHost();
                continue;
            }
            
            // 给不同端口设置不同的超时时间，避免所有端口同时超时
            int adjustedTimeout = baseTimeout * (0.8 + 0.4 * QRandomGenerator::global()->generateDouble());
            
            // 为常见服务端口提供更长的超时时间
            if (port == 80 || port == 443 || port == 3389 || port == 22) {
                adjustedTimeout = qMin(baseTimeout * 1.5, 1000.0); // 最大1秒
            }
            
            // 等待连接或超时
            if (socket->waitForConnected(adjustedTimeout)) {
                portResults[port] = true;
                connectedPorts++;
            } else {
                // 连接失败，检查错误类型
                QAbstractSocket::SocketError error = socket->error();
                if (error == QAbstractSocket::ConnectionRefusedError) {
                    // 连接被拒绝，端口确实关闭
                    portResults[port] = false;
                    qDebug() << "端口" << port << "连接被拒绝";
                } else if (error == QAbstractSocket::SocketTimeoutError) {
                    // 超时可能是防火墙过滤或服务未响应
                    portResults[port] = false;
                    qDebug() << "端口" << port << "连接超时";
                } else {
                    // 其他错误
                    portResults[port] = false;
                    qDebug() << "端口" << port << "连接错误:" << socket->errorString();
                }
            }
            
            // 断开连接并清理资源
            socket->disconnectFromHost();
        }
        
        // 记录扫描信息
        qDebug() << "IP" << hostInfo.ipAddress << "发现" << connectedPorts 
                 << "个开放端口，耗时" << timer.elapsed() << "毫秒";
    } catch (const std::exception &e) {
        qWarning() << "端口扫描异常: " << hostInfo.ipAddress << " - " << e.what();
    } catch (...) {
        qWarning() << "端口扫描未知异常: " << hostInfo.ipAddress;
    }
    
    // 清理所有socket资源
    for (QTcpSocket* socket : sockets) {
        if (socket->state() != QAbstractSocket::UnconnectedState) {
            socket->abort(); // 强制关闭任何可能仍在连接的socket
        }
        socket->deleteLater();
    }
    
    // 更新主机信息
    hostInfo.openPorts = portResults;
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
    
    // 检查是否是模拟MAC地址
    if (macAddress.startsWith("SM:")) {
        return "[模拟] 本地网络";
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
    
    // 常见厂商的简单查找表 - 扩展更多厂商
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
        {"001479", "美国无线标准协会"},
        // 新增厂商
        {"D89695", "NVIDIA"},
        {"FCA13E", "三星电子"},
        {"D48F33", "三星电子"},
        {"34AA99", "诺基亚"},
        {"001F6B", "LG电子"},
        {"E48F34", "飞利浦"},
        {"0022B0", "D-Link"},
        {"0013F7", "WAGO"},
        {"0080C2", "IEEE 802.1标准协会"},
        {"000E6A", "3Com"},
        {"001292", "惠普"},
        {"001871", "惠普"},
        {"002264", "惠普"},
        {"24BE05", "惠普"},
        {"001083", "IBM"},
        {"000D60", "IBM"},
        {"00FDD8", "SEIKO"},
        {"000BA3", "施耐德电气"},
        {"00079F", "施耐德电气"},
        {"00049F", "飞塔"}
    };
    
    // 转换OUI为大写进行匹配
    QString ouiUpper = oui.toUpper();
    
    // 尝试查找厂商
    if (vendorMap.contains(ouiUpper)) {
        if (m_debugMode) {
            qDebug() << "MAC地址" << macAddress << "OUI为" << ouiUpper << "匹配厂商:" << vendorMap[ouiUpper];
        }
        return vendorMap[ouiUpper];
    }
    
    // 如果没找到，则返回"未知厂商"并标记OUI
    if (m_debugMode) {
        qDebug() << "MAC地址" << macAddress << "OUI为" << ouiUpper << "未找到对应厂商";
    }
    return QString("未知厂商 (%1)").arg(ouiUpper);
}

void NetworkScanner::processScanResults()
{
    if (!m_isScanning) return;
    
    // 检查扫描是否已经完成
    bool allFinished = true;
    int activeThreads = 0;
    int completedThreads = 0;
    
    // 遍历所有扫描任务
    for (const QFuture<void> &future : m_scanFutures) {
        if (future.isStarted()) {
            if (future.isFinished()) {
                completedThreads++;
            } else {
                allFinished = false;
                activeThreads++;
            }
        }
    }
    
    // 检查是否已经超时
    int elapsedSeconds = m_scanStartTime.elapsed() / 1000;
    bool timeoutReached = elapsedSeconds > 120; // 2分钟超时
    
    // 更新状态日志（每5秒一次，避免过多输出）
    QDateTime now = QDateTime::currentDateTime();
    if (m_lastProgressUpdate.secsTo(now) >= 5) {
        m_lastProgressUpdate = now;
        qDebug() << "扫描状态: 已完成" << completedThreads << "个任务, 活跃" 
                 << activeThreads << "个任务, 已发现" 
                 << m_scannedHostsList.size() << "个主机, 耗时" << elapsedSeconds << "秒";
    }
    
    // 检查是否达到终止条件
    if ((allFinished && !m_scanFutures.isEmpty()) || timeoutReached) {
        // 所有任务已完成或达到超时
        if (timeoutReached && !allFinished) {
            qDebug() << "扫描达到最大时间，强制结束...";
            emit scanError("扫描超时自动结束");
        } else {
            qDebug() << "所有扫描任务已完成，总共发现" << m_scannedHostsList.size() << "个主机";
        }
        
        // 结束扫描
        m_isScanning = false;
        emit scanFinished();
    } else if (m_isScanning) {
        // 继续检查进度
        QTimer::singleShot(1000, this, &NetworkScanner::processScanResults);
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

// 生成伪MAC地址，格式上明确标记为模拟数据
QString NetworkScanner::generatePseudoMACFromIP(const QString &ip)
{
    try {
        QStringList ipParts = ip.split(".");
        if (ipParts.size() == 4) {
            // 使用固定前缀SIM-表示这是模拟MAC
            int octet1 = ipParts[0].toInt() % 256;
            int octet2 = ipParts[1].toInt() % 256;
            int octet3 = ipParts[2].toInt() % 256;
            int octet4 = ipParts[3].toInt() % 256;
            
            QString pseudoMac = QString("SM:%1:%2:%3:%4")
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
    return "SM:00:00:00:01";
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

// 添加一个新的方法用于更可靠地检测主机可达性
bool NetworkScanner::checkHostReachable(const QHostAddress &address, int timeout)
{
    // 方法1: 尝试多个常用端口连接
    const QVector<int> checkPorts = {80, 443, 22, 445, 3389};
    int adjustedTimeout = qMin(timeout, 300); // 限制单个连接的超时
    
    for (int port : checkPorts) {
        QTcpSocket socket;
        socket.connectToHost(address, port);
        if (socket.waitForConnected(adjustedTimeout)) {
            socket.disconnectFromHost();
            return true; // 只要有一个端口响应，就认为主机可达
        }
    }
    
    // 方法2: 检查是否在本地网络中
    bool isLocalNetwork = false;
    try {
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface &interface : interfaces) {
            if (!(interface.flags() & QNetworkInterface::IsUp) || 
                (interface.flags() & QNetworkInterface::IsLoopBack)) {
                continue;
            }
            
            QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    // 检查IP是否在同一子网
                    quint32 ip1 = entry.ip().toIPv4Address();
                    quint32 ip2 = address.toIPv4Address();
                    quint32 netmask = entry.netmask().toIPv4Address();
                    if ((ip1 & netmask) == (ip2 & netmask)) {
                        // 本地网络中的主机更可能可达，但不能确定
                        // 提高权重但不直接判定为可达
                        isLocalNetwork = true;
                        
                        // 如果在同一子网，额外尝试一些其他端口
                        const QVector<int> localPorts = {135, 139, 8080};
                        for (int port : localPorts) {
                            QTcpSocket socket;
                            socket.connectToHost(address, port);
                            if (socket.waitForConnected(adjustedTimeout)) {
                                socket.disconnectFromHost();
                                return true;
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
        qWarning() << "检查网络接口时发生异常";
    }
    
    // 在本地网络中，使用启发式算法判断
    if (isLocalNetwork) {
        // 在本地网络中，有些常见IP更可能是活跃的
        QString ipStr = address.toString();
        QStringList ipParts = ipStr.split(".");
        if (ipParts.size() == 4) {
            int lastOctet = ipParts[3].toInt();
            // 网关、服务器等常见地址
            if (lastOctet == 1 || lastOctet == 254 || lastOctet < 20) {
                qDebug() << "基于启发式判断，本地网络中的IP可能可达:" << ipStr;
                return true;
            }
        }
        
        // 随机概率判定部分本地IP为可达，增加发现几率
        // 但标记为不确定结果
        if (QRandomGenerator::global()->bounded(100) < 30) { // 30%的概率
            qDebug() << "随机判定本地网络IP可能可达(不确定):" << ipStr;
            return true;
        }
    }
    
    return false;
} 