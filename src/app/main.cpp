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
#include "storage/Schema.h"
#include "storage/CoreRepository.h"
#include <memory>
#include "ui/MainWindow.h"
#include "platform/base/PlatformFactory.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Netraxis");
    QCoreApplication::setApplicationName("Netraxis");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Unified network and internet management");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"platform-report","Report the native network provider and interface count as JSON."});
    parser.process(app);
    if(parser.isSet("platform-report")){
        const auto backend=netraxis::createNetworkPlatform();
        if(!backend){QTextStream(stdout)<<QJsonDocument(QJsonObject{{"status","unavailable"},{"error",backend.error().message}}).toJson(QJsonDocument::Compact)<<Qt::endl;return 2;}
        const netraxis::TaskContext context(std::make_shared<std::atomic_bool>(false),QDeadlineTimer(10000));
        const auto inventory=backend.value()->interfaces(context);
        QJsonObject report{{"backend",backend.value()->descriptor().name},{"provider",backend.value()->descriptor().implementation},{"status",inventory?"observed":"unavailable"}};
        if(inventory)report.insert("interfaces",int(inventory.value().size()));else report.insert("error",inventory.error().message);
        QTextStream(stdout)<<QJsonDocument(report).toJson(QJsonDocument::Compact)<<Qt::endl;return inventory?0:2;
    }
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
    QObject::connect(&lifecycle, &netraxis::ApplicationLifecycle::stopping, &app, [&logger] {
        logger.write(netraxis::LogLevel::Info, netraxis::LogEvent::Stopping);
    });
    netraxis::SettingsStore settingsStore;
    const auto loadedSettings = settingsStore.load();
    auto settings = loadedSettings ? loadedSettings.value() : netraxis::RuntimeSettings{};
    logger.setLevel(settings.logLevel);
    if(logging)logger.write(netraxis::LogLevel::Info,netraxis::LogEvent::Started);
    netraxis::TaskManager tasks;
    tasks.setConcurrency(settings.workerLimit);
    QObject::connect(&lifecycle, &netraxis::ApplicationLifecycle::stopping, &tasks, &netraxis::TaskManager::shutdown);
    netraxis::SqliteStore database;
    const auto databasePath=netraxis::SqliteStore::defaultPath();
    const auto storage=databasePath?database.open(databasePath.value()):netraxis::Status::failure(databasePath.error());
    const auto schema=storage?netraxis::Schema::migrate(database):storage;
    std::unique_ptr<netraxis::CoreRepository> repository;
    if(schema)repository=std::make_unique<netraxis::CoreRepository>(database);
    netraxis::MainWindow window(tasks, settingsStore, settings);
    window.setRepository(repository.get());
    if(schema&&loadedSettings)window.setDatabasePath(database.path());
    QObject::connect(&window,&netraxis::MainWindow::dataDeleted,&app,[&logger]{logger.write(netraxis::LogLevel::Info,netraxis::LogEvent::DataDeleted);});
    QObject::connect(&window, &netraxis::MainWindow::settingsChanged, &app, [&] {
        logger.setLevel(settings.logLevel);
        logger.write(netraxis::LogLevel::Info, netraxis::LogEvent::SettingsChanged);
        if(schema)window.setDatabasePath(database.path());
    });
    QObject::connect(&lifecycle, &netraxis::ApplicationLifecycle::activationRequested, &window, [&window] {
        window.showNormal();
        window.raise();
        window.activateWindow();
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &lifecycle, &netraxis::ApplicationLifecycle::shutdown);
    if (!lifecycle.errorString().isEmpty()) window.statusBar()->showMessage(lifecycle.errorString());
    if (!schema) window.showError(schema.error());
    if (!logging) window.showError(logging.error());
    if (!loadedSettings) window.showError(loadedSettings.error());
    window.show();
    return app.exec();
}
