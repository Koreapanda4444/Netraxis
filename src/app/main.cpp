#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QStatusBar>
#include <QDebug>
#include "ApplicationLifecycle.h"
#include "core/Logger.h"
#include "core/TaskManager.h"
#include "core/Settings.h"
#include "storage/SqliteStore.h"
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
    netraxis::Logger logger;
    const auto logging = logger.open();
    if (logging) logger.write(netraxis::LogLevel::Info, netraxis::LogEvent::Started);
    QObject::connect(&lifecycle, &netraxis::ApplicationLifecycle::stopping, &app, [&logger] {
        logger.write(netraxis::LogLevel::Info, netraxis::LogEvent::Stopping);
    });
    netraxis::SettingsStore settingsStore;
    const auto loadedSettings = settingsStore.load();
    auto settings = loadedSettings ? loadedSettings.value() : netraxis::RuntimeSettings{};
    logger.setLevel(settings.logLevel);
    netraxis::TaskManager tasks;
    tasks.setConcurrency(settings.workerLimit);
    QObject::connect(&lifecycle, &netraxis::ApplicationLifecycle::stopping, &tasks, &netraxis::TaskManager::shutdown);
    netraxis::SqliteStore database;
    const auto databasePath=netraxis::SqliteStore::defaultPath();
    const auto storage=databasePath?database.open(databasePath.value()):netraxis::Status::failure(databasePath.error());
    netraxis::MainWindow window(tasks, settingsStore, settings);
    QObject::connect(&window, &netraxis::MainWindow::settingsChanged, &app, [&] {
        logger.setLevel(settings.logLevel);
        logger.write(netraxis::LogLevel::Info, netraxis::LogEvent::SettingsChanged);
    });
    QObject::connect(&lifecycle, &netraxis::ApplicationLifecycle::activationRequested, &window, [&window] {
        window.showNormal();
        window.raise();
        window.activateWindow();
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &lifecycle, &netraxis::ApplicationLifecycle::shutdown);
    if (!lifecycle.errorString().isEmpty()) window.statusBar()->showMessage(lifecycle.errorString());
    if (!storage) window.showError(storage.error());
    if (!logging) window.showError(logging.error());
    if (!loadedSettings) window.showError(loadedSettings.error());
    window.show();
    return app.exec();
}
