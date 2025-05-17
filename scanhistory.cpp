#include "scanhistory.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

// ScanSession 方法实现
int ScanSession::reachableHosts() const
{
    int count = 0;
    for (const HostInfo &host : hosts) {
        if (host.isReachable) {
            count++;
        }
    }
    return count;
}

QMap<int, int> ScanSession::portDistribution() const
{
    QMap<int, int> result;
    
    for (const HostInfo &host : hosts) {
        if (!host.isReachable) continue;
        
        QMapIterator<int, bool> it(host.openPorts);
        while (it.hasNext()) {
            it.next();
            if (it.value()) {  // 如果端口开放
                result[it.key()]++;
            }
        }
    }
    
    return result;
}

// ScanHistory 实现
ScanHistory::ScanHistory(QObject *parent)
    : QObject(parent)
{
}

void ScanHistory::addSession(const QList<HostInfo> &hosts, const QString &description)
{
    if (hosts.isEmpty()) {
        return;
    }
    
    ScanSession session;
    session.scanTime = QDateTime::currentDateTime();
    session.hosts = hosts;
    
    if (description.isEmpty()) {
        session.description = QString("扫描于 %1").arg(session.scanTime.toString("yyyy-MM-dd hh:mm:ss"));
    } else {
        session.description = description;
    }
    
    m_sessions.prepend(session); // 最新的会话放在最前面
    
    // 限制历史记录数量 (可选)
    // const int MaxSessions = 20;
    // if (m_sessions.size() > MaxSessions) {
    //     m_sessions.removeLast();
    // }
    
    emit historyChanged();
}

ScanSession ScanHistory::getSession(int index) const
{
    if (index >= 0 && index < m_sessions.size()) {
        return m_sessions.at(index);
    }
    return ScanSession(); // 返回一个空会话
}

void ScanHistory::clearHistory()
{
    m_sessions.clear();
    emit historyChanged();
}

// 添加 removeSession 方法
void ScanHistory::removeSession(int index)
{
    if (index >= 0 && index < m_sessions.size()) {
        m_sessions.removeAt(index);
        emit historyChanged();
    }
}

QPair<QList<HostInfo>, QList<HostInfo>> ScanHistory::compareScans(int sessionIndex1, int sessionIndex2) const
{
    QPair<QList<HostInfo>, QList<HostInfo>> result;
    
    if (sessionIndex1 < 0 || sessionIndex1 >= m_sessions.size() ||
        sessionIndex2 < 0 || sessionIndex2 >= m_sessions.size()) {
        return result; // 无效索引
    }
    
    const ScanSession &session1 = m_sessions.at(sessionIndex1);
    const ScanSession &session2 = m_sessions.at(sessionIndex2);
    
    QMap<QString, HostInfo> hostsInSession1;
    for (const HostInfo &host : session1.hosts) {
        hostsInSession1[host.ipAddress] = host;
    }
    
    QMap<QString, HostInfo> hostsInSession2;
    for (const HostInfo &host : session2.hosts) {
        hostsInSession2[host.ipAddress] = host;
    }
    
    // 查找在session2中但不在session1中的主机 (新增)
    for (const HostInfo &host2 : session2.hosts) {
        if (!hostsInSession1.contains(host2.ipAddress)) {
            result.first.append(host2);
        }
    }
    
    // 查找在session1中但不在session2中的主机 (消失)
    for (const HostInfo &host1 : session1.hosts) {
        if (!hostsInSession2.contains(host1.ipAddress)) {
            result.second.append(host1);
        }
    }
    
    return result;
}

bool ScanHistory::saveToFile(const QString &filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法打开文件进行写入:" << filename;
        return false;
    }
    
    QJsonArray sessionsArray;
    for (const ScanSession &session : m_sessions) {
        QJsonObject sessionObject;
        sessionObject["scanTime"] = session.scanTime.toString(Qt::ISODate);
        sessionObject["description"] = session.description;
        
        QJsonArray hostsArray;
        for (const HostInfo &host : session.hosts) {
            QJsonObject hostObject;
            hostObject["ipAddress"] = host.ipAddress;
            hostObject["hostName"] = host.hostName;
            hostObject["macAddress"] = host.macAddress;
            hostObject["macVendor"] = host.macVendor;
            hostObject["isReachable"] = host.isReachable;
            hostObject["scanTime"] = host.scanTime.toString(Qt::ISODate);
            
            QJsonObject openPortsObject;
            QMapIterator<int, bool> it(host.openPorts);
            while (it.hasNext()) {
                it.next();
                openPortsObject[QString::number(it.key())] = it.value();
            }
            hostObject["openPorts"] = openPortsObject;
            hostsArray.append(hostObject);
        }
        sessionObject["hosts"] = hostsArray;
        sessionsArray.append(sessionObject);
    }
    
    QJsonDocument doc(sessionsArray);
    file.write(doc.toJson());
    file.close();
    
    return true;
}

bool ScanHistory::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件进行读取:" << filename;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        qWarning() << "JSON数据格式错误，根元素不是数组";
        return false;
    }
    
    m_sessions.clear();
    QJsonArray sessionsArray = doc.array();
    
    for (const QJsonValue &sessionValue : sessionsArray) {
        if (!sessionValue.isObject()) continue;
        
        QJsonObject sessionObject = sessionValue.toObject();
        ScanSession session;
        session.scanTime = QDateTime::fromString(sessionObject["scanTime"].toString(), Qt::ISODate);
        session.description = sessionObject["description"].toString();
        
        if (sessionObject.contains("hosts") && sessionObject["hosts"].isArray()) {
            QJsonArray hostsArray = sessionObject["hosts"].toArray();
            for (const QJsonValue &hostValue : hostsArray) {
                if (!hostValue.isObject()) continue;
                
                QJsonObject hostObject = hostValue.toObject();
                HostInfo host;
                host.ipAddress = hostObject["ipAddress"].toString();
                host.hostName = hostObject["hostName"].toString();
                host.macAddress = hostObject["macAddress"].toString();
                host.macVendor = hostObject["macVendor"].toString();
                host.isReachable = hostObject["isReachable"].toBool();
                host.scanTime = QDateTime::fromString(hostObject["scanTime"].toString(), Qt::ISODate);
                
                if (hostObject.contains("openPorts") && hostObject["openPorts"].isObject()) {
                    QJsonObject openPortsObject = hostObject["openPorts"].toObject();
                    for (auto it = openPortsObject.begin(); it != openPortsObject.end(); ++it) {
                        bool ok;
                        int port = it.key().toInt(&ok);
                        if (ok) {
                            host.openPorts[port] = it.value().toBool();
                        }
                    }
                }
                session.hosts.append(host);
            }
        }
        m_sessions.append(session);
    }
    
    emit historyChanged();
    return true;
} 