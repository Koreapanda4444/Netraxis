#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Netraxis");
    QCoreApplication::setApplicationName("Netraxis");
    QCoreApplication::setApplicationVersion("0.1.0");
    netraxis::MainWindow window;
    window.show();
    return app.exec();
}
