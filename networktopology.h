#ifndef NETWORKTOPOLOGY_H
#define NETWORKTOPOLOGY_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QMap>
#include <QPair>
#include <QProcess>
#include "networkscanner.h"

// 定义设备类型枚举
enum DeviceType {
    DEVICE_UNKNOWN,
    DEVICE_ROUTER,
    DEVICE_SERVER,
    DEVICE_PC,
    DEVICE_MOBILE,
    DEVICE_PRINTER,
    DEVICE_IOT
};

// 设备连接类型
enum ConnectionType {
    CONNECTION_UNKNOWN,
    CONNECTION_DIRECT,     // 直接连接
    CONNECTION_WIRELESS,   // 无线连接
    CONNECTION_VPN,        // VPN连接
    CONNECTION_ROUTED      // 通过路由器连接
};

// 拓扑分析器类
class TopologyAnalyzer {
public:
    TopologyAnalyzer();
    
    // 推断设备连接关系
    QMap<QString, QStringList> inferDeviceConnections(const QList<HostInfo> &hosts);
    
    // 分析网络层次结构
    QMap<int, QStringList> analyzeTTLLayers(const QList<HostInfo> &hosts);
    
    // 分析子网结构
    QMap<QString, QStringList> analyzeSubnets(const QList<HostInfo> &hosts);
    
    // 基于响应时间的设备聚类
    QList<QStringList> clusterDevicesByResponseTime(const QList<HostInfo> &hosts);
    
    // 获取特定IP的TTL值
    int getTTLValue(const QString &ipAddress);
    
    // 执行traceroute命令获取路径信息
    QStringList performTraceRoute(const QString &targetIP);
    
    // 计算子网掩码 - 改为公共方法
    QString calculateSubnet(const QString &ip, int prefixLength = 24);
    
    // 判断两个IP是否在同一子网
    bool inSameSubnet(const QString &ip1, const QString &ip2, int prefixLength = 24);
    
private:
    // 已移除private中的calculateSubnet声明
};

// 设备节点
class DeviceNode : public QGraphicsItem
{
public:
    DeviceNode(const HostInfo &host, DeviceType type = DEVICE_UNKNOWN);
    
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    
    void setDeviceType(DeviceType type);
    DeviceType deviceType() const { return m_type; }
    
    QString ipAddress() const { return m_host.ipAddress; }
    HostInfo hostInfo() const { return m_host; }
    void setPosition(const QPointF &pos);
    
    // 新增：设置网络层级
    void setNetworkLayer(int layer) { m_networkLayer = layer; }
    int networkLayer() const { return m_networkLayer; }
    
    // 新增：设置子网组
    void setSubnetGroup(const QString &subnet) { m_subnetGroup = subnet; }
    QString subnetGroup() const { return m_subnetGroup; }
    
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    
private:
    HostInfo m_host;
    DeviceType m_type;
    bool m_highlight;
    QPointF m_dragStartPosition;
    int m_networkLayer;       // 网络层级
    QString m_subnetGroup;    // 子网组
};

// 连接线
class ConnectionLine : public QGraphicsItem
{
public:
    ConnectionLine(DeviceNode *source, DeviceNode *target, 
                  ConnectionType type = CONNECTION_DIRECT);
    
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    
    void updatePosition();
    
    // 设置连接类型
    void setConnectionType(ConnectionType type);
    ConnectionType connectionType() const { return m_connectionType; }
    
private:
    DeviceNode *m_source;
    DeviceNode *m_target;
    ConnectionType m_connectionType;
};

// 拓扑图视图
class NetworkTopologyView : public QGraphicsView
{
    Q_OBJECT
    
public:
    NetworkTopologyView(QWidget *parent = nullptr);
    
    void setHosts(const QList<HostInfo> &hosts);
    void clear();
    void autoLayout();
    
    // 新增布局方法
    void hierarchicalLayout(const QMap<int, QStringList> &layers);
    void groupedLayout(const QMap<QString, QStringList> &groups);
    
signals:
    void nodeSelected(const HostInfo &host);
    
private:
    QGraphicsScene *m_scene;
    QMap<QString, DeviceNode*> m_nodes;
    QList<ConnectionLine*> m_connections;
    
    TopologyAnalyzer m_analyzer;
    
    DeviceType determineDeviceType(const HostInfo &host);
    void createConnection(DeviceNode *source, DeviceNode *target, 
                          ConnectionType type = CONNECTION_DIRECT);
};

// 网络拓扑组件（包含拓扑图和控制面板）
class NetworkTopology : public QWidget
{
    Q_OBJECT
    
public:
    NetworkTopology(QWidget *parent = nullptr);
    
    void updateTopology(const QList<HostInfo> &hosts);
    void clear();
    
    // 缩放和视图控制方法
    void scale(qreal factor);
    void resetView();
    
    // 新增：切换布局方式
    void setLayoutMode(int mode);
    
signals:
    void deviceSelected(const HostInfo &host);
    
private:
    NetworkTopologyView *m_topologyView;
    QWidget *m_controlPanel;
    
    enum LayoutMode {
        LAYOUT_AUTO,
        LAYOUT_HIERARCHICAL,
        LAYOUT_GROUPED
    };
    
    LayoutMode m_layoutMode;
    QList<HostInfo> m_currentHosts;  // 缓存当前主机列表，用于布局切换
};

#endif // NETWORKTOPOLOGY_H 