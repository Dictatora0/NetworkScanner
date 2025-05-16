#ifndef NETWORKSCANNER_H
#define NETWORKSCANNER_H

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QHostInfo>
#include <QFuture>
#include <QtConcurrent>
#include <QMutex>
#include <QMap>
#include <QDateTime>

struct HostInfo {
    QString ipAddress;
    QString hostName;
    QString macAddress;
    QString macVendor;
    bool isReachable;
    QDateTime scanTime;
    QMap<int, bool> openPorts;
};

class NetworkScanner : public QObject
{
    Q_OBJECT

public:
    explicit NetworkScanner(QObject *parent = nullptr);
    ~NetworkScanner();

    void startScan();
    void stopScan();
    bool isScanning() const;
    
    // 新增功能
    void setCustomPortsToScan(const QList<int> &ports);
    void setScanTimeout(int msecs);
    void setCustomIPRange(const QString &startIP, const QString &endIP);
    QList<HostInfo> getScannedHosts() const;
    void saveResultsToFile(const QString &filename) const;

signals:
    void scanStarted();
    void scanFinished();
    void hostFound(const HostInfo &host);
    void scanProgress(int progress);
    void scanError(const QString &errorMessage);

private slots:
    void processScanResults();

private:
    bool m_isScanning;
    QList<QHostAddress> getLocalNetworkAddresses();
    void scanHost(const QHostAddress &address);
    bool pingHost(const QHostAddress &address);
    QString lookupHostName(const QHostAddress &address);
    QString lookupMacAddress(const QHostAddress &address);
    QString lookupMacVendor(const QString &macAddress);
    void scanHostPorts(HostInfo &hostInfo);
    
    QList<QFuture<void>> m_scanFutures;
    QMutex m_mutex;
    int m_scannedHosts;
    int m_totalHosts;
    
    QList<int> m_portsToScan;
    int m_scanTimeout;
    QHostAddress m_startIPRange;
    QHostAddress m_endIPRange;
    bool m_useCustomRange;
    QList<HostInfo> m_scannedHostsList;
};

#endif // NETWORKSCANNER_H 