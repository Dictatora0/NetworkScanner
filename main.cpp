#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>
#include <QFont>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序信息
    QCoreApplication::setApplicationName("NetScanner");
    QCoreApplication::setApplicationVersion("2.0");
    QCoreApplication::setOrganizationName("NetScannerTeam");
    
    // 加载样式 - Fusion样式提供现代外观
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    
    // 设置一个通用字体，避免特定字体缺失问题
    QFont defaultFont = QApplication::font();
    defaultFont.setFamily("Arial"); // 或者其他通用字体如 Helvetica, Verdana
    QApplication::setFont(defaultFont);
    
    // 创建启动屏幕
    QPixmap pixmap(":/images/splash.png"); // 确保你的资源文件中有此图片
    QSplashScreen splash(pixmap);
    if (!pixmap.isNull()) { // 仅当图片有效时显示启动屏幕
        splash.show();
        a.processEvents(); // 处理事件，确保启动屏幕显示
    } else {
        // 可以选择记录一个警告或者不显示启动屏幕
        // qDebug() << "Splash image not found or invalid.";
    }
    
    MainWindow w;
    
    // 在启动屏幕显示一段时间后显示主窗口
    if (!pixmap.isNull()) {
        QTimer::singleShot(2500, &splash, &QWidget::close);
        QTimer::singleShot(2500, &w, &QWidget::show);
    } else {
        w.show(); // 如果没有启动屏幕，直接显示主窗口
    }
    
    return a.exec();
} 