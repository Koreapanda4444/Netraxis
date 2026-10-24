#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QStatusBar>
#include <QDebug>
#include "ApplicationLifecycle.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Netraxis");
    QCoreApplication::setApplicationName("Netraxis");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Unified network and internet management");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);
    netraxis::ApplicationLifecycle lifecycle;
    const auto state = lifecycle.start();
    if (state == netraxis::ApplicationLifecycle::Start::ActivatedExisting) return 0;
    if (state == netraxis::ApplicationLifecycle::Start::AlreadyRunning) {
        qWarning() << lifecycle.errorString();
        return 2;
    }
    if (state == netraxis::ApplicationLifecycle::Start::Failed) {
        QMessageBox::critical(nullptr, "Netraxis", lifecycle.errorString());
        return 1;
    }
    netraxis::MainWindow window;
    QObject::connect(&lifecycle, &netraxis::ApplicationLifecycle::activationRequested, &window, [&window] {
        window.showNormal();
        window.raise();
        window.activateWindow();
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &lifecycle, &netraxis::ApplicationLifecycle::shutdown);
    if (!lifecycle.errorString().isEmpty()) window.statusBar()->showMessage(lifecycle.errorString());
    window.show();
    return app.exec();
}
