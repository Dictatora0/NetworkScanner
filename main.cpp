#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    MainWindow mainWindow;
    mainWindow.setWindowTitle("网络扫描器");
    mainWindow.resize(800, 600);
    mainWindow.show();
    
    return app.exec();
} 