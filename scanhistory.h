#ifndef SCANHISTORY_H
#define SCANHISTORY_H

#include <QObject>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QFile>
#include <QTextStream>
#include "networkscanner.h"

// 扫描会话数据结构
struct ScanSession {
    QDateTime scanTime;
    QString description;
    QList<HostInfo> hosts;
    
    // 会话统计信息
    int totalHosts() const { return hosts.size(); }
    int reachableHosts() const;
    int unreachableHosts() const { return totalHosts() - reachableHosts(); }
    QMap<int, int> portDistribution() const;
};

// 扫描历史管理类
class ScanHistory : public QObject
{
    Q_OBJECT
    
public:
    explicit ScanHistory(QObject *parent = nullptr);
    
    // 添加扫描会话
    void addSession(const QList<HostInfo> &hosts, const QString &description = QString());
    
    // 获取历史扫描会话
    QList<ScanSession> getSessions() const { return m_sessions; }
    
    // 获取特定会话
    ScanSession getSession(int index) const;
    
    // 获取会话数量
    int sessionCount() const { return m_sessions.size(); }
    
    // 清除所有历史记录
    void clearHistory();

    // 删除特定会话
    void removeSession(int index);
    
    // 将两次扫描结果进行比较，找出新增和消失的主机
    QPair<QList<HostInfo>, QList<HostInfo>> compareScans(int sessionIndex1, int sessionIndex2) const;
    
    // 保存和加载扫描历史
    bool saveToFile(const QString &filename) const;
    bool loadFromFile(const QString &filename);
    
signals:
    void historyChanged();
    
private:
    QList<ScanSession> m_sessions;
};

#endif // SCANHISTORY_H 