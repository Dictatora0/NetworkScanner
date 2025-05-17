#ifndef NETWORKTOPOLOGY_H
#define NETWORKTOPOLOGY_H

#include <QWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QMap>
#include <QPair>
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
};

// 连接线
class ConnectionLine : public QGraphicsItem
{
public:
    ConnectionLine(DeviceNode *source, DeviceNode *target);
    
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    
    void updatePosition();
    
private:
    DeviceNode *m_source;
    DeviceNode *m_target;
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
    
signals:
    void nodeSelected(const HostInfo &host);
    
private:
    QGraphicsScene *m_scene;
    QMap<QString, DeviceNode*> m_nodes;
    QList<ConnectionLine*> m_connections;
    
    DeviceType determineDeviceType(const HostInfo &host);
    void createConnection(DeviceNode *source, DeviceNode *target);
};

// 网络拓扑组件（包含拓扑图和控制面板）
class NetworkTopology : public QWidget
{
    Q_OBJECT
    
public:
    NetworkTopology(QWidget *parent = nullptr);
    
    void updateTopology(const QList<HostInfo> &hosts);
    void clear();
    
signals:
    void deviceSelected(const HostInfo &host);
    
private:
    NetworkTopologyView *m_topologyView;
    QWidget *m_controlPanel;
};

#endif // NETWORKTOPOLOGY_H 