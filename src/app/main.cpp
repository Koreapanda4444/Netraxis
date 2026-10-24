#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Netraxis");
    QCoreApplication::setApplicationName("Netraxis");
    QCoreApplication::setApplicationVersion("0.1.0");
    QMainWindow window;
    window.setWindowTitle("Netraxis");
    window.resize(1100, 720);
    auto *label = new QLabel("Netraxis — Network & Internet Management", &window);
    label->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(label);
    window.statusBar()->showMessage("Ready");
    window.show();
    return app.exec();
}
