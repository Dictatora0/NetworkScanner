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
#include <QProcess>
#include <QRegularExpression>

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
ConnectionLine::ConnectionLine(DeviceNode *source, DeviceNode *target, ConnectionType type)
    : m_source(source), m_target(target), m_connectionType(type)
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
    
    // 根据连接类型设置不同的样式
    switch (m_connectionType) {
        case CONNECTION_DIRECT:
            // 实线表示直接连接
            painter->setPen(QPen(Qt::darkGreen, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            break;
            
        case CONNECTION_WIRELESS:
            // 虚线表示无线连接
            painter->setPen(QPen(Qt::blue, 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            break;
            
        case CONNECTION_VPN:
            // 点划线表示VPN连接
            painter->setPen(QPen(Qt::darkCyan, 1.5, Qt::DashDotLine, Qt::RoundCap, Qt::RoundJoin));
            break;
            
        case CONNECTION_ROUTED:
            // 波浪线表示通过路由器的连接
            painter->setPen(QPen(Qt::darkGray, 1.5, Qt::DashDotDotLine, Qt::RoundCap, Qt::RoundJoin));
            break;
            
        default:
            painter->setPen(QPen(Qt::gray, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            break;
    }
    
    // 绘制连接线
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

void ConnectionLine::setConnectionType(ConnectionType type)
{
    m_connectionType = type;
    update();
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
    
    // 使用TopologyAnalyzer分析设备连接关系
    QMap<QString, QStringList> connections = m_analyzer.inferDeviceConnections(hosts);
    
    // 分析网络层次结构
    QMap<int, QStringList> layers = m_analyzer.analyzeTTLLayers(hosts);
    
    // 创建所有设备节点
    for (const HostInfo &host : hosts) {
        DeviceType type = determineDeviceType(host);
        DeviceNode *node = new DeviceNode(host, type);
        
        // 设置节点的网络层级
        for (auto it = layers.begin(); it != layers.end(); ++it) {
            if (it.value().contains(host.ipAddress)) {
                node->setNetworkLayer(it.key());
                break;
            }
        }
        
        // 设置节点的子网组
        node->setSubnetGroup(m_analyzer.calculateSubnet(host.ipAddress));
        
        m_nodes[host.ipAddress] = node;
        m_scene->addItem(node);
    }
    
    // 创建连接关系
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        if (m_nodes.contains(it.key())) {
            DeviceNode *sourceNode = m_nodes[it.key()];
            
            for (const QString &targetIP : it.value()) {
                if (m_nodes.contains(targetIP)) {
                    // 根据连接节点的网络层级决定连接类型
                    ConnectionType connectionType = CONNECTION_DIRECT;
                    if (sourceNode->networkLayer() + 1 < m_nodes[targetIP]->networkLayer()) {
                        connectionType = CONNECTION_ROUTED;
                    } else if (sourceNode->deviceType() == DEVICE_ROUTER && 
                               m_nodes[targetIP]->deviceType() == DEVICE_MOBILE) {
                        connectionType = CONNECTION_WIRELESS;
                    }
                    
                    createConnection(sourceNode, m_nodes[targetIP], connectionType);
                }
            }
        }
    }
    
    // 使用层次化布局
    hierarchicalLayout(layers);
    
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

void NetworkTopologyView::hierarchicalLayout(const QMap<int, QStringList> &layers)
{
    // 首先确定各个层级的节点数量，以便计算布局
    int maxNodesInLayer = 0;
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        maxNodesInLayer = qMax(maxNodesInLayer, it.value().size());
    }
    
    // 计算水平和垂直间距
    qreal verticalSpacing = 150;  // 层级之间的垂直间距
    qreal horizontalSpacing = 120;  // 同层节点之间的水平间距
    
    // 确保最宽的层也能容纳所有节点
    qreal sceneWidth = qMax(600.0, maxNodesInLayer * horizontalSpacing);
    m_scene->setSceneRect(-sceneWidth/2, -200, sceneWidth, (layers.size() + 1) * verticalSpacing);
    
    // 按层放置节点
    for (auto it = layers.begin(); it != layers.end(); ++it) {
        int layer = it.key();
        const QStringList &ips = it.value();
        
        // 该层有多少个节点
        int nodeCount = ips.size();
        
        // 计算该层第一个节点的水平位置
        qreal startX = -((nodeCount - 1) * horizontalSpacing) / 2;
        
        // 放置该层的所有节点
        for (int i = 0; i < nodeCount; ++i) {
            QString ip = ips[i];
            if (m_nodes.contains(ip)) {
                QPointF pos(startX + i * horizontalSpacing, layer * verticalSpacing);
                
                // 创建动画效果
                QTimeLine *timer = new QTimeLine(500, this);
                timer->setFrameRange(0, 100);
                
                QGraphicsItemAnimation *animation = new QGraphicsItemAnimation;
                animation->setItem(m_nodes[ip]);
                animation->setTimeLine(timer);
                
                // 设置起始位置和目标位置
                QPointF startPos = m_nodes[ip]->pos();
                QPointF endPos = pos;
                
                for (int j = 0; j <= 100; ++j) {
                    qreal stepX = startPos.x() + (endPos.x() - startPos.x()) * j / 100.0;
                    qreal stepY = startPos.y() + (endPos.y() - startPos.y()) * j / 100.0;
                    animation->setPosAt(j / 100.0, QPointF(stepX, stepY));
                }
                
                timer->start();
                connect(timer, &QTimeLine::finished, timer, &QObject::deleteLater);
                connect(timer, &QTimeLine::finished, animation, &QObject::deleteLater);
            }
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

void NetworkTopologyView::createConnection(DeviceNode *source, DeviceNode *target, ConnectionType type)
{
    if (!source || !target) return;
    
    ConnectionLine *line = new ConnectionLine(source, target, type);
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

void NetworkTopology::setLayoutMode(int mode)
{
    m_layoutMode = static_cast<LayoutMode>(mode);
    
    // 重新应用当前主机列表以更新布局
    if (!m_currentHosts.isEmpty()) {
        updateTopology(m_currentHosts);
    }
}

void NetworkTopology::updateTopology(const QList<HostInfo> &hosts)
{
    // 缓存当前主机列表
    m_currentHosts = hosts;
    
    // 更新拓扑图
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

// TopologyAnalyzer 实现
TopologyAnalyzer::TopologyAnalyzer()
{
}

QMap<QString, QStringList> TopologyAnalyzer::inferDeviceConnections(const QList<HostInfo> &hosts)
{
    QMap<QString, QStringList> connections;
    
    // 寻找网关/路由器设备
    QString gatewayIP;
    for (const HostInfo &host : hosts) {
        if (host.ipAddress.endsWith(".1") || host.ipAddress.endsWith(".254") ||
            host.macVendor.contains("路由", Qt::CaseInsensitive) ||
            host.hostName.contains("router", Qt::CaseInsensitive) ||
            host.hostName.contains("gateway", Qt::CaseInsensitive)) {
            gatewayIP = host.ipAddress;
            break;
        }
    }
    
    // 分析TTL层次
    QMap<int, QStringList> layers = analyzeTTLLayers(hosts);
    
    // 分析子网
    QMap<QString, QStringList> subnets = analyzeSubnets(hosts);
    
    // 基于以上信息构建连接关系
    if (!gatewayIP.isEmpty()) {
        // 路由器是所有连接的中心
        QStringList routerConnections;
        
        // 找到所有第二层设备(与路由器直连)
        if (layers.contains(1)) {
            for (const QString &ip : layers[1]) {
                routerConnections.append(ip);
                
                // 第二层设备可能是子网的网关
                for (auto it = subnets.begin(); it != subnets.end(); ++it) {
                    if (it.key() != calculateSubnet(gatewayIP) && 
                        it.value().contains(ip)) {
                        // 这个设备可能是子网的网关，连接到该子网中的其他设备
                        QStringList subnetConnections;
                        for (const QString &subnetIP : it.value()) {
                            if (subnetIP != ip) {
                                subnetConnections.append(subnetIP);
                            }
                        }
                        connections[ip] = subnetConnections;
                    }
                }
            }
        }
        
        connections[gatewayIP] = routerConnections;
    } else {
        // 没有明确的路由器，使用基于子网的连接关系
        for (auto it = subnets.begin(); it != subnets.end(); ++it) {
            // 在每个子网中，找到可能的网关
            QString subnetGateway;
            for (const QString &ip : it.value()) {
                if (ip.endsWith(".1") || ip.endsWith(".254")) {
                    subnetGateway = ip;
                    break;
                }
            }
            
            if (!subnetGateway.isEmpty()) {
                // 将该子网中的其他设备连接到网关
                QStringList gatewayConnections;
                for (const QString &ip : it.value()) {
                    if (ip != subnetGateway) {
                        gatewayConnections.append(ip);
                    }
                }
                connections[subnetGateway] = gatewayConnections;
            }
        }
    }
    
    return connections;
}

QMap<int, QStringList> TopologyAnalyzer::analyzeTTLLayers(const QList<HostInfo> &hosts)
{
    QMap<int, QStringList> layers;
    
    // 查找网关设备
    QString gatewayIP;
    for (const HostInfo &host : hosts) {
        if (host.ipAddress.endsWith(".1") || host.ipAddress.endsWith(".254")) {
            gatewayIP = host.ipAddress;
            break;
        }
    }
    
    if (gatewayIP.isEmpty() && !hosts.isEmpty()) {
        gatewayIP = hosts.first().ipAddress;
    }
    
    // 网关设备在第0层
    layers[0].append(gatewayIP);
    
    // 尝试执行traceroute获取其他设备的层级
    for (const HostInfo &host : hosts) {
        if (host.ipAddress != gatewayIP) {
            // 获取TTL值
            int ttl = getTTLValue(host.ipAddress);
            
            // TTL值1通常表示直接连接(第1层)
            // TTL值2表示通过一个路由器(第2层)，以此类推
            if (ttl > 0) {
                layers[ttl].append(host.ipAddress);
            } else {
                // 如果无法获取TTL，默认放在第1层
                layers[1].append(host.ipAddress);
            }
        }
    }
    
    return layers;
}

int TopologyAnalyzer::getTTLValue(const QString &ipAddress)
{
#ifdef Q_OS_MACOS
    QProcess process;
    process.start("ping", QStringList() << "-c" << "1" << "-t" << "100" << ipAddress);
    process.waitForFinished(2000);
    
    QString output = process.readAllStandardOutput();
    
    // 解析ttl值
    QRegularExpression ttlRegex("ttl=(\\d+)");
    QRegularExpressionMatch match = ttlRegex.match(output);
    if (match.hasMatch()) {
        bool ok;
        int ttl = match.captured(1).toInt(&ok);
        if (ok) {
            // 标准TTL值通常是64/128/255
            // 64通常是Linux/Unix, 128是Windows, 255通常是路由器
            // 可以根据TTL值估算跳数
            if (ttl >= 250) return 0;      // 路由器
            else if (ttl >= 120) return 1; // Windows主机(直连)
            else if (ttl >= 60) return 1;  // Linux/Unix主机(直连)
            else return 2;                 // 远程主机
        }
    }
#endif

#ifdef Q_OS_WINDOWS
    QProcess process;
    process.start("ping", QStringList() << "-n" << "1" << ipAddress);
    process.waitForFinished(2000);
    
    QString output = process.readAllStandardOutput();
    
    // 解析TTL值
    QRegularExpression ttlRegex("TTL=(\\d+)");
    QRegularExpressionMatch match = ttlRegex.match(output);
    if (match.hasMatch()) {
        bool ok;
        int ttl = match.captured(1).toInt(&ok);
        if (ok) {
            if (ttl >= 250) return 0;
            else if (ttl >= 120) return 1;
            else if (ttl >= 60) return 1;
            else return 2;
        }
    }
#endif
    
    return -1;  // 无法确定
}

QStringList TopologyAnalyzer::performTraceRoute(const QString &targetIP)
{
    QStringList hops;
    
#ifdef Q_OS_MACOS
    QProcess process;
    process.start("traceroute", QStringList() << "-n" << "-w" << "1" << targetIP);
    process.waitForFinished(5000);
    
    QString output = process.readAllStandardOutput();
    QStringList lines = output.split("\n");
    
    // 跳过第一行(标题)
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (!line.isEmpty()) {
            QRegularExpression ipRegex("(\\d+\\.\\d+\\.\\d+\\.\\d+)");
            QRegularExpressionMatch match = ipRegex.match(line);
            if (match.hasMatch()) {
                hops.append(match.captured(1));
            }
        }
    }
#endif

#ifdef Q_OS_WINDOWS
    QProcess process;
    process.start("tracert", QStringList() << "-d" << "-w" << "1000" << targetIP);
    process.waitForFinished(5000);
    
    QString output = process.readAllStandardOutput();
    QStringList lines = output.split("\n");
    
    // 跳过前两行(标题)
    for (int i = 2; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (!line.isEmpty()) {
            QRegularExpression ipRegex("(\\d+\\.\\d+\\.\\d+\\.\\d+)");
            QRegularExpressionMatch match = ipRegex.match(line);
            if (match.hasMatch()) {
                hops.append(match.captured(1));
            }
        }
    }
#endif
    
    return hops;
}

QMap<QString, QStringList> TopologyAnalyzer::analyzeSubnets(const QList<HostInfo> &hosts)
{
    QMap<QString, QStringList> subnets;
    
    // 按子网分组IP地址
    for (const HostInfo &host : hosts) {
        QString subnet = calculateSubnet(host.ipAddress);
        subnets[subnet].append(host.ipAddress);
    }
    
    return subnets;
}

QString TopologyAnalyzer::calculateSubnet(const QString &ip, int prefixLength)
{
    QStringList parts = ip.split(".");
    if (parts.size() != 4) return "";
    
    // 将IP转换为32位整数
    quint32 ipValue = 0;
    for (int i = 0; i < 4; ++i) {
        ipValue = (ipValue << 8) | parts[i].toUInt();
    }
    
    // 计算子网掩码
    quint32 mask = 0xFFFFFFFF << (32 - prefixLength);
    
    // 计算网络地址
    quint32 network = ipValue & mask;
    
    // 转换回点分十进制表示法
    QString subnet = QString("%1.%2.%3.%4")
                    .arg((network >> 24) & 0xFF)
                    .arg((network >> 16) & 0xFF)
                    .arg((network >> 8) & 0xFF)
                    .arg(network & 0xFF);
    
    return subnet;
}

bool TopologyAnalyzer::inSameSubnet(const QString &ip1, const QString &ip2, int prefixLength)
{
    return calculateSubnet(ip1, prefixLength) == calculateSubnet(ip2, prefixLength);
}

QList<QStringList> TopologyAnalyzer::clusterDevicesByResponseTime(const QList<HostInfo> &hosts)
{
    QList<QStringList> clusters;
    QVector<QPair<QString, int>> deviceTimes;
    
    // 执行ping获取响应时间
    for (const HostInfo &host : hosts) {
        QProcess process;
        process.start("ping", QStringList() << "-c" << "3" << host.ipAddress);
        process.waitForFinished(3000);
        
        QString output = process.readAllStandardOutput();
        
        // 解析平均响应时间
        QRegularExpression timeRegex("time=([0-9.]+)");
        QRegularExpressionMatchIterator i = timeRegex.globalMatch(output);
        
        double totalTime = 0;
        int count = 0;
        
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            totalTime += match.captured(1).toDouble();
            count++;
        }
        
        if (count > 0) {
            double avgTime = totalTime / count;
            deviceTimes.append(qMakePair(host.ipAddress, qRound(avgTime)));
        }
    }
    
    // 简单聚类：按响应时间分组
    // 本地设备(0-10ms), 近端设备(10-50ms), 远端设备(>50ms)
    QStringList localDevices, nearDevices, farDevices;
    
    for (const auto &pair : deviceTimes) {
        if (pair.second < 10) {
            localDevices.append(pair.first);
        } else if (pair.second < 50) {
            nearDevices.append(pair.first);
        } else {
            farDevices.append(pair.first);
        }
    }
    
    if (!localDevices.isEmpty()) clusters.append(localDevices);
    if (!nearDevices.isEmpty()) clusters.append(nearDevices);
    if (!farDevices.isEmpty()) clusters.append(farDevices);
    
    return clusters;
}

void NetworkTopologyView::groupedLayout(const QMap<QString, QStringList> &groups)
{
    // 首先计算总共有多少个分组
    int groupCount = groups.size();
    int maxNodesInGroup = 0;
    
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        maxNodesInGroup = qMax(maxNodesInGroup, it.value().size());
    }
    
    // 计算分组的排列布局（尝试接近正方形布局）
    int rows = qSqrt(groupCount);
    int cols = (groupCount + rows - 1) / rows;
    
    // 计算每个组的大小
    qreal groupWidth = 300;
    qreal groupHeight = 300;
    qreal horizontalGap = 150;
    qreal verticalGap = 150;
    
    //
    qreal totalWidth = cols * groupWidth + (cols - 1) * horizontalGap;
    qreal totalHeight = rows * groupHeight + (rows - 1) * verticalGap;
    
    m_scene->setSceneRect(-totalWidth/2, -totalHeight/2, totalWidth, totalHeight);
    
    // 放置各个分组中的节点
    int groupIndex = 0;
    for (auto it = groups.begin(); it != groups.end(); ++it, ++groupIndex) {
        int row = groupIndex / cols;
        int col = groupIndex % cols;
        
        // 计算该分组的中心位置
        qreal groupCenterX = -totalWidth/2 + col * (groupWidth + horizontalGap) + groupWidth/2;
        qreal groupCenterY = -totalHeight/2 + row * (groupHeight + verticalGap) + groupHeight/2;
        
        // 放置该分组中的节点
        const QStringList &ips = it.value();
        int nodeCount = ips.size();
        
        // 使用环形布局，围绕分组中心放置节点
        qreal radius = qMin(groupWidth, groupHeight) / 2 * 0.8;
        
        for (int i = 0; i < nodeCount; ++i) {
            QString ip = ips[i];
            if (m_nodes.contains(ip)) {
                // 计算环形上的位置
                qreal angle = 2 * M_PI * i / nodeCount;
                qreal x = groupCenterX + radius * qCos(angle);
                qreal y = groupCenterY + radius * qSin(angle);
                
                // 创建动画
                QTimeLine *timer = new QTimeLine(500, this);
                timer->setFrameRange(0, 100);
                
                QGraphicsItemAnimation *animation = new QGraphicsItemAnimation;
                animation->setItem(m_nodes[ip]);
                animation->setTimeLine(timer);
                
                QPointF startPos = m_nodes[ip]->pos();
                QPointF endPos(x, y);
                
                for (int j = 0; j <= 100; ++j) {
                    qreal stepX = startPos.x() + (endPos.x() - startPos.x()) * j / 100.0;
                    qreal stepY = startPos.y() + (endPos.y() - startPos.y()) * j / 100.0;
                    animation->setPosAt(j / 100.0, QPointF(stepX, stepY));
                }
                
                timer->start();
                connect(timer, &QTimeLine::finished, timer, &QObject::deleteLater);
                connect(timer, &QTimeLine::finished, animation, &QObject::deleteLater);
            }
        }
    }
    
    // 更新所有连接线的位置
    for (ConnectionLine *line : m_connections) {
        line->updatePosition();
    }
    
    // 更新视图
    m_scene->update();
}

// 网络拓扑视图的自动布局方法
void NetworkTopologyView::autoLayout()
{
    // 如果没有网络拓扑图分析器，使用层次化布局
    QMap<int, QStringList> layers;
    QStringList ipList;
    
    // 收集所有节点的IP地址
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        ipList.append(it.key());
    }
    
    // 如果节点少于2个，不需要布局
    if (ipList.size() < 2) return;
    
    // 查找网关设备作为第0层
    QString gatewayIP;
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        if (it.value()->deviceType() == DEVICE_ROUTER) {
            gatewayIP = it.key();
            layers[0].append(gatewayIP);
            break;
        }
    }
    
    // 如果没找到路由器，使用第一个节点作为中心
    if (gatewayIP.isEmpty()) {
        gatewayIP = ipList.first();
        layers[0].append(gatewayIP);
        ipList.removeOne(gatewayIP);
    } else {
        ipList.removeOne(gatewayIP);
    }
    
    // 将其他节点放在第1层
    layers[1] = ipList;
    
    // 使用层次化布局
    hierarchicalLayout(layers);
} 