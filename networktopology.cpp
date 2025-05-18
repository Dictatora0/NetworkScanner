#include "networktopology.h"
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QApplication>
#include <QPalette>
#include <QDebug>
#include <QtMath>
#include <QGraphicsItemAnimation>
#include <QTimeLine>
#include <QToolTip>
#include <QScrollBar>

// DeviceNode 实现
DeviceNode::DeviceNode(const HostInfo &host, DeviceType type)
    : m_host(host), m_type(type), m_highlight(false)
{
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
    
    // 初始大小
    setScale(1.0);
}

QRectF DeviceNode::boundingRect() const
{
    return QRectF(-30, -30, 60, 60);
}

void DeviceNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    
    // 绘制设备节点
    painter->setRenderHint(QPainter::Antialiasing);
    
    // 设置颜色
    QColor baseColor;
    switch (m_type) {
        case DEVICE_ROUTER: baseColor = Qt::blue; break;
        case DEVICE_SERVER: baseColor = Qt::darkGreen; break;
        case DEVICE_PC: baseColor = Qt::darkCyan; break;
        case DEVICE_MOBILE: baseColor = Qt::magenta; break;
        case DEVICE_PRINTER: baseColor = Qt::darkYellow; break;
        case DEVICE_IOT: baseColor = Qt::darkMagenta; break;
        default: baseColor = Qt::gray; break;
    }
    
    // 如果选中或高亮，使用更亮的颜色
    if (isSelected() || m_highlight) {
        baseColor = baseColor.lighter(150);
    }
    
    // 绘制设备图标背景
    painter->setBrush(baseColor);
    painter->setPen(QPen(baseColor.darker(), 2));
    painter->drawEllipse(QPointF(0, 0), 25, 25);
    
    // 绘制设备图标
    painter->setPen(Qt::white);
    painter->setFont(QFont("Arial", 12, QFont::Bold));
    QString iconText;
    switch (m_type) {
        case DEVICE_ROUTER: iconText = "R"; break;
        case DEVICE_SERVER: iconText = "S"; break;
        case DEVICE_PC: iconText = "PC"; break;
        case DEVICE_MOBILE: iconText = "M"; break;
        case DEVICE_PRINTER: iconText = "PR"; break;
        case DEVICE_IOT: iconText = "IoT"; break;
        default: iconText = "?"; break;
    }
    
    // 居中绘制文本
    QFontMetrics fm(painter->font());
    QRect textRect = fm.boundingRect(iconText);
    painter->drawText(QPointF(-textRect.width()/2, textRect.height()/3), iconText);
    
    // 显示IP地址
    painter->setPen(Qt::black);
    painter->setFont(QFont("Arial", 8));
    QString ipText = m_host.ipAddress;
    QFontMetrics fmIp(painter->font());
    QRect ipRect = fmIp.boundingRect(ipText);
    painter->drawText(QPointF(-ipRect.width()/2, 40), ipText);
}

void DeviceNode::setDeviceType(DeviceType type)
{
    m_type = type;
    update();
}

void DeviceNode::setPosition(const QPointF &pos)
{
    setPos(pos);
}

void DeviceNode::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_dragStartPosition = pos();
    QGraphicsItem::mousePressEvent(event);
}

void DeviceNode::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseMoveEvent(event);
    
    // 通知连接线更新位置
    scene()->update();
}

void DeviceNode::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
}

void DeviceNode::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_highlight = true;
    update();
    QGraphicsItem::hoverEnterEvent(event);
    
    // 显示设备详情工具提示
    QString tooltipText = QString("IP: %1\n主机名: %2\nMAC: %3\n厂商: %4")
                            .arg(m_host.ipAddress)
                            .arg(m_host.hostName)
                            .arg(m_host.macAddress)
                            .arg(m_host.macVendor);
                            
    // 如果有开放端口，显示开放端口
    QStringList openPorts;
    QMapIterator<int, bool> i(m_host.openPorts);
    while (i.hasNext()) {
        i.next();
        if (i.value()) {
            openPorts << QString::number(i.key());
        }
    }
    if (!openPorts.isEmpty()) {
        tooltipText += "\n开放端口: " + openPorts.join(", ");
    }
    
    QToolTip::showText(event->screenPos(), tooltipText);
}

void DeviceNode::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_highlight = false;
    update();
    QGraphicsItem::hoverLeaveEvent(event);
}

// ConnectionLine 实现
ConnectionLine::ConnectionLine(DeviceNode *source, DeviceNode *target)
    : m_source(source), m_target(target)
{
    setZValue(-1);  // 确保线在节点下方
    updatePosition();
}

QRectF ConnectionLine::boundingRect() const
{
    if (!m_source || !m_target) return QRectF();
    
    QPointF sourcePoint = m_source->pos();
    QPointF targetPoint = m_target->pos();
    
    return QRectF(sourcePoint, QSizeF(targetPoint.x() - sourcePoint.x(), 
                                      targetPoint.y() - sourcePoint.y()))
           .normalized()
           .adjusted(-10, -10, 10, 10);
}

void ConnectionLine::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    
    if (!m_source || !m_target) return;
    
    QPointF sourcePoint = m_source->pos();
    QPointF targetPoint = m_target->pos();
    
    // 计算源节点和目标节点之间的方向向量
    QLineF line(sourcePoint, targetPoint);
    
    // 确保不绘制穿过节点的线，而是到节点边缘
    qreal angle = std::atan2(targetPoint.y() - sourcePoint.y(), targetPoint.x() - sourcePoint.x());
    qreal sourceRadius = 25;  // 节点半径
    qreal targetRadius = 25;  // 节点半径
    
    QPointF sourceEdge(
        sourcePoint.x() + sourceRadius * std::cos(angle),
        sourcePoint.y() + sourceRadius * std::sin(angle)
    );
    
    QPointF targetEdge(
        targetPoint.x() - targetRadius * std::cos(angle),
        targetPoint.y() - targetRadius * std::sin(angle)
    );
    
    // 绘制连接线
    painter->setPen(QPen(Qt::gray, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawLine(sourceEdge, targetEdge);
    
    // 绘制箭头
    qreal arrowSize = 10;
    double angle_arrow = std::atan2(targetEdge.y() - sourceEdge.y(), targetEdge.x() - sourceEdge.x());
    QPointF arrowP1 = targetEdge - QPointF(sin(angle_arrow + M_PI / 3) * arrowSize,
                                          cos(angle_arrow + M_PI / 3) * arrowSize);
    QPointF arrowP2 = targetEdge - QPointF(sin(angle_arrow + M_PI - M_PI / 3) * arrowSize,
                                          cos(angle_arrow + M_PI - M_PI / 3) * arrowSize);
    
    QPolygonF arrowHead;
    arrowHead << targetEdge << arrowP1 << arrowP2;
    painter->setBrush(Qt::gray);
    painter->drawPolygon(arrowHead);
}

void ConnectionLine::updatePosition()
{
    if (m_source && m_target) {
        // 更新边界矩形
        prepareGeometryChange();
    }
}

// NetworkTopologyView 实现
NetworkTopologyView::NetworkTopologyView(QWidget *parent)
    : QGraphicsView(parent)
{
    // 创建场景
    m_scene = new QGraphicsScene(this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    setScene(m_scene);
    
    // 设置视图属性
    setCacheMode(CacheBackground);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorViewCenter);
    setDragMode(ScrollHandDrag);
    
    // 设置场景大小
    m_scene->setSceneRect(-300, -300, 600, 600);
    
    // 设置背景色
    setBackgroundBrush(QBrush(QColor(240, 240, 250)));
}

void NetworkTopologyView::setHosts(const QList<HostInfo> &hosts)
{
    // 清除现有内容
    clear();
    
    // 如果主机列表为空，直接返回
    if (hosts.isEmpty()) return;
    
    // 找到作为网关的设备
    QString gatewayIP;
    for (const HostInfo &host : hosts) {
        if (host.macVendor.contains("路由器", Qt::CaseInsensitive) ||
            host.hostName.contains("router", Qt::CaseInsensitive) ||
            host.ipAddress.endsWith(".1") || 
            host.ipAddress.endsWith(".254")) {
            gatewayIP = host.ipAddress;
            break;
        }
    }
    
    // 没有找到网关，使用第一个设备作为中心
    if (gatewayIP.isEmpty() && !hosts.isEmpty()) {
        gatewayIP = hosts.first().ipAddress;
    }
    
    // 创建所有设备节点
    for (const HostInfo &host : hosts) {
        DeviceType type = determineDeviceType(host);
        DeviceNode *node = new DeviceNode(host, type);
        m_nodes[host.ipAddress] = node;
        m_scene->addItem(node);
    }
    
    // 创建连接（将所有设备连接到网关）
    if (!gatewayIP.isEmpty() && m_nodes.contains(gatewayIP)) {
        DeviceNode *gatewayNode = m_nodes[gatewayIP];
        
        for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
            if (it.key() != gatewayIP) {
                createConnection(gatewayNode, it.value());
            }
        }
    }
    
    // 自动布局
    autoLayout();
    
    // 调整视图以适应所有项目
    fitInView(m_scene->itemsBoundingRect(), Qt::KeepAspectRatio);
}

void NetworkTopologyView::clear()
{
    // 清除所有连接线
    qDeleteAll(m_connections);
    m_connections.clear();
    
    // 清除所有节点
    qDeleteAll(m_nodes);
    m_nodes.clear();
    
    // 清除场景中的所有项目
    m_scene->clear();
}

void NetworkTopologyView::autoLayout()
{
    // 如果节点少于2个，不需要布局
    if (m_nodes.size() < 2) return;
    
    // 查找网关设备作为中心节点
    DeviceNode *centerNode = nullptr;
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (it.value()->deviceType() == DEVICE_ROUTER) {
            centerNode = it.value();
            break;
        }
    }
    
    // 如果没找到路由器，使用第一个节点
    if (!centerNode && !m_nodes.isEmpty()) {
        centerNode = m_nodes.values().first();
    }
    
    // 将中心节点放在场景中央
    if (centerNode) {
        centerNode->setPos(0, 0);
    }
    
    // 围绕中心节点放置其他节点
    int nodeCount = m_nodes.size() - (centerNode ? 1 : 0);
    int idx = 0;
    qreal radius = 150;  // 放置半径
    
    // 如果有很多设备，增加半径
    if (nodeCount > 10) {
        radius = 200 + (nodeCount - 10) * 10;
    }
    
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (it.value() != centerNode) {
            // 计算围绕中心的位置角度
            qreal angle = 2 * M_PI * idx / nodeCount;
            qreal x = radius * std::cos(angle);
            qreal y = radius * std::sin(angle);
            
            // 创建动画效果
            QTimeLine *timer = new QTimeLine(500, this);
            timer->setFrameRange(0, 100);
            
            QGraphicsItemAnimation *animation = new QGraphicsItemAnimation;
            animation->setItem(it.value());
            animation->setTimeLine(timer);
            
            // 设置起始位置和目标位置
            QPointF startPos = it.value()->pos();
            QPointF endPos(x, y);
            
            for (int i = 0; i <= 100; ++i) {
                qreal stepX = startPos.x() + (endPos.x() - startPos.x()) * i / 100.0;
                qreal stepY = startPos.y() + (endPos.y() - startPos.y()) * i / 100.0;
                animation->setPosAt(i / 100.0, QPointF(stepX, stepY));
            }
            
            timer->start();
            connect(timer, &QTimeLine::finished, timer, &QObject::deleteLater);
            connect(timer, &QTimeLine::finished, animation, &QObject::deleteLater);
            
            idx++;
        }
    }
    
    // 更新所有连接线的位置
    for (ConnectionLine *line : m_connections) {
        line->updatePosition();
    }
    
    // 更新视图
    m_scene->update();
}

DeviceType NetworkTopologyView::determineDeviceType(const HostInfo &host)
{
    // 根据IP地址、主机名、MAC厂商等信息推断设备类型
    QString ipLower = host.ipAddress.toLower();
    QString hostLower = host.hostName.toLower();
    QString vendorLower = host.macVendor.toLower();
    
    // 检测路由器
    if (vendorLower.contains("路由") || 
        vendorLower.contains("router") ||
        hostLower.contains("router") || 
        hostLower.contains("gateway") ||
        ipLower.endsWith(".1") || 
        ipLower.endsWith(".254")) {
        return DEVICE_ROUTER;
    }
    
    // 检测服务器
    if (hostLower.contains("server") || 
        vendorLower.contains("server") ||
        vendorLower.contains("dell") || 
        vendorLower.contains("ibm") ||
        vendorLower.contains("hp") && !vendorLower.contains("printer")) {
        return DEVICE_SERVER;
    }
    
    // 检测打印机
    if (hostLower.contains("print") || 
        vendorLower.contains("print") ||
        vendorLower.contains("canon") || 
        vendorLower.contains("hp") && vendorLower.contains("printer") ||
        vendorLower.contains("epson")) {
        return DEVICE_PRINTER;
    }
    
    // 检测移动设备
    if (vendorLower.contains("apple") || 
        vendorLower.contains("iphone") ||
        vendorLower.contains("ipad") || 
        vendorLower.contains("android") ||
        vendorLower.contains("xiaomi") || 
        vendorLower.contains("huawei") ||
        vendorLower.contains("oppo") || 
        vendorLower.contains("vivo")) {
        return DEVICE_MOBILE;
    }
    
    // 检测IoT设备
    if (hostLower.contains("esp") || 
        hostLower.contains("arduino") ||
        hostLower.contains("raspberry") || 
        hostLower.contains("iot") ||
        vendorLower.contains("nest") || 
        vendorLower.contains("smart")) {
        return DEVICE_IOT;
    }
    
    // 默认为PC
    return DEVICE_PC;
}

void NetworkTopologyView::createConnection(DeviceNode *source, DeviceNode *target)
{
    if (!source || !target) return;
    
    ConnectionLine *line = new ConnectionLine(source, target);
    m_connections.append(line);
    m_scene->addItem(line);
}

// NetworkTopology 实现
NetworkTopology::NetworkTopology(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 创建拓扑图视图
    m_topologyView = new NetworkTopologyView(this);
    
    // 创建控制面板
    m_controlPanel = new QWidget(this);
    QHBoxLayout *controlLayout = new QHBoxLayout(m_controlPanel);
    
    QLabel *layoutLabel = new QLabel("布局:", m_controlPanel);
    QPushButton *autoLayoutButton = new QPushButton("自动布局", m_controlPanel);
    QPushButton *resetViewButton = new QPushButton("重置视图", m_controlPanel);
    QPushButton *zoomInButton = new QPushButton("+", m_controlPanel);
    QPushButton *zoomOutButton = new QPushButton("-", m_controlPanel);
    
    controlLayout->addWidget(layoutLabel);
    controlLayout->addWidget(autoLayoutButton);
    controlLayout->addWidget(resetViewButton);
    controlLayout->addWidget(zoomInButton);
    controlLayout->addWidget(zoomOutButton);
    controlLayout->addStretch();
    
    // 添加到主布局
    mainLayout->addWidget(m_topologyView);
    mainLayout->addWidget(m_controlPanel);
    
    // 连接信号和槽
    connect(autoLayoutButton, &QPushButton::clicked, m_topologyView, &NetworkTopologyView::autoLayout);
    connect(resetViewButton, &QPushButton::clicked, [this]() {
        m_topologyView->fitInView(m_topologyView->scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
    });
    
    connect(zoomInButton, &QPushButton::clicked, [this]() {
        m_topologyView->scale(1.2, 1.2);
    });
    
    connect(zoomOutButton, &QPushButton::clicked, [this]() {
        m_topologyView->scale(1/1.2, 1/1.2);
    });
    
    connect(m_topologyView, &NetworkTopologyView::nodeSelected, this, &NetworkTopology::deviceSelected);
}

void NetworkTopology::updateTopology(const QList<HostInfo> &hosts)
{
    m_topologyView->setHosts(hosts);
}

void NetworkTopology::clear()
{
    m_topologyView->clear();
}

void NetworkTopology::scale(qreal factor)
{
    if (m_topologyView) {
        m_topologyView->scale(factor, factor);
    }
}

void NetworkTopology::resetView()
{
    if (m_topologyView && m_topologyView->scene()) {
        m_topologyView->fitInView(m_topologyView->scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
} 