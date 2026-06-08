#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("imgmgr"));
    QApplication::setOrganizationName(QStringLiteral("imgmgr"));

    MainWindow w;
    w.show();
    return app.exec();
}
