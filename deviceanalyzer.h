#ifndef DEVICEANALYZER_H
#define DEVICEANALYZER_H

#include <QWidget>
#include <QMap>
#include <QVector>
#include <QPair>
#include <QString>
#include <QtCharts/QChart>
#include <QtCharts/QPieSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChartView>
#include "networkscanner.h"

// 设备分析器类 - 提供对扫描结果的统计分析
class DeviceAnalyzer : public QWidget
{
    Q_OBJECT
    
public:
    DeviceAnalyzer(QWidget *parent = nullptr);
    
    // 分析扫描结果并更新统计图表
    void analyzeHosts(const QList<HostInfo> &hosts);
    void clear();
    
    // 获取各种统计信息
    int getTotalHostsCount() const { return m_totalHosts; }
    int getReachableHostsCount() const { return m_reachableHosts; }
    int getUnreachableHostsCount() const { return m_totalHosts - m_reachableHosts; }
    
    // 获取各种图表
    QChart* getDeviceTypeChart() const { return m_deviceTypeChart; }
    QChart* getPortDistributionChart() const { return m_portDistributionChart; }
    QChart* getVendorDistributionChart() const { return m_vendorDistributionChart; }
    
    // 创建安全风险报告
    QString generateSecurityReport(const QList<HostInfo> &hosts);
    
signals:
    void analysisCompleted();
    
private:
    // 扫描统计
    int m_totalHosts;
    int m_reachableHosts;
    
    // 设备类型分布图表
    QChart *m_deviceTypeChart;
    QPieSeries *m_deviceTypeSeries;
    
    // 端口分布图表
    QChart *m_portDistributionChart;
    QBarSeries *m_portSeries;
    
    // 厂商分布图表
    QChart *m_vendorDistributionChart;
    QPieSeries *m_vendorSeries;
    
    // 创建各类图表
    void createDeviceTypeChart();
    void createPortDistributionChart();
    void createVendorDistributionChart();
    
    // 设备类型判断
    QString determineDeviceType(const HostInfo &host);
};

#endif // DEVICEANALYZER_H 