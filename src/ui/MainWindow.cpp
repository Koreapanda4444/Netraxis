#include "MainWindow.h"
#include "StatusSurface.h"
#include "WorkspacePage.h"
#include "core/Paths.h"
#include "platform/SystemInfo.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QVBoxLayout>

namespace netraxis {
namespace {
QWidget *messagePage(const QString &title, const QString &message) {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    auto *heading = new QLabel("<h2>" + title.toHtmlEscaped() + "</h2>");
    auto *text = new QLabel(message);
    text->setWordWrap(true);
    layout->addWidget(heading); layout->addWidget(text);
    layout->addWidget(new StatusSurface({Availability::Unsupported, Verification::Unverified, QObject::tr("Platform collection has not been implemented in this build.")},page));layout->addStretch();
    return page;
}
}
MainWindow::MainWindow(TaskManager &tasks, SettingsStore &store, RuntimeSettings &settings)
    : tasks_(tasks), store_(store), settings_(settings) {
    setWindowTitle("Netraxis"); resize(1100, 720); setMinimumSize(700, 480);
    auto *splitter = new QSplitter(this);
    auto *navigation = new QListWidget(splitter);
    navigation->addItems({tr("Overview"), tr("Workspaces"), tr("Devices"), tr("Diagnostics"), tr("Settings")});
    navigation->setMinimumWidth(145);
    auto *pages = new QStackedWidget(splitter);
    pages->addWidget(dashboard());
    workspaces_=new WorkspacePage;pages->addWidget(workspaces_);
    pages->addWidget(messagePage(tr("Devices"), tr("Device collection has not run. An empty inventory does not indicate that the network has no devices.")));
    pages->addWidget(diagnostics()); pages->addWidget(settingsPage());
    splitter->setStretchFactor(0, 0);splitter->setStretchFactor(1, 1);splitter->setSizes({190,910});
    setCentralWidget(splitter);
    auto *help = menuBar()->addMenu(tr("Help"));
    help->addAction(tr("About Qt"), this, []{QMessageBox::aboutQt(nullptr, "Netraxis — Qt");});
    connect(navigation, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    navigation->setCurrentRow(0);
    pending_ = new QLabel(this);statusBar()->addPermanentWidget(pending_);
    const auto pendingChanged = [this](int count){pending_->setText(tr("Background tasks: %1").arg(count));};
    connect(&tasks_, &TaskManager::pendingChanged, this, pendingChanged);pendingChanged(tasks_.pendingCount());
    connect(&tasks_, &TaskManager::finished, this, [this](quint64 id, const TaskResult &result){
        if (id != diagnosticTask_) return;
        diagnosticTask_ = 0;
        diagnosticResult_->clear();
        if(result) diagnosticStatus_->setState({Availability::Supported,Verification::Healthy,result.value().toString()});
        else diagnosticStatus_->setError(result.error());
    });
    if (!settings_.windowGeometry.isEmpty()) restoreGeometry(settings_.windowGeometry);
    statusBar()->showMessage(tr("Ready"));
}
QWidget *MainWindow::dashboard() {
    auto *page = new QWidget;auto *layout = new QFormLayout(page);
    const auto system = SystemInfo::current();
    layout->addRow(new QLabel(tr("<h2>Overview</h2>")));
    layout->addRow(tr("Operating system"), new QLabel(system.os));
    layout->addRow(tr("Kernel"), new QLabel(system.kernel));
    layout->addRow(tr("Architecture"), new QLabel(system.architecture));
    layout->addRow(new StatusSurface({Availability::Unsupported, Verification::Unverified, tr("Network collection is unavailable in this build. No network health conclusion can be drawn.")},page));
    return page;
}
QWidget *MainWindow::diagnostics() {
    auto *page = new QWidget;auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("<h2>Application diagnostics</h2>")));
    auto *start = new QPushButton(tr("Check application storage"));
    auto *cancel = new QPushButton(tr("Cancel check"));
    diagnosticResult_ = new QLabel(tr("No check has run."));diagnosticResult_->setWordWrap(true);
    diagnosticStatus_=new StatusSurface({Availability::Supported,Verification::Unverified,tr("Only local application storage is checked.")},page);
    layout->addWidget(start);layout->addWidget(cancel);layout->addWidget(diagnosticResult_);layout->addWidget(diagnosticStatus_);layout->addStretch();
    connect(diagnosticStatus_,&StatusSurface::retryRequested,start,&QPushButton::click);
    connect(start, &QPushButton::clicked, this, [this]{
        if (diagnosticTask_) return;
        auto job = tasks_.submit([](const TaskContext &context)->TaskResult{
            const auto ready = context.check();if (!ready) return TaskResult::failure(ready.error());
            const auto directory = applicationDataDirectory();if (!directory) return TaskResult::failure(directory.error());
            QTemporaryFile file(directory.value() + "/check-XXXXXX");
            if (!file.open() || file.write("Netraxis") != 8 || !file.flush() || !file.seek(0) || file.readAll() != "Netraxis")
                return TaskResult::failure({ErrorCode::Storage, "Application storage read/write check failed.", true, "diagnostics"});
            return TaskResult::success(QString("Application storage is readable and writable. Network connectivity remains unverified."));
        }, settings_.operationTimeoutMs);
        if (job) {diagnosticTask_ = job.value();diagnosticResult_->setText(tr("Checking…"));}
        else diagnosticStatus_->setError(job.error());
    });
    connect(cancel, &QPushButton::clicked, this, [this]{if(diagnosticTask_)tasks_.cancel(diagnosticTask_);});
    return page;
}
QWidget *MainWindow::settingsPage() {
    auto *page = new QWidget;auto *layout = new QFormLayout(page);
    layout->addRow(new QLabel(tr("<h2>Settings</h2>")));
    auto makeSpin = [page](int min, int max, int value){auto *spin=new QSpinBox(page);spin->setRange(min,max);spin->setValue(value);return spin;};
    auto *workers = makeSpin(1,16,settings_.workerLimit);
    auto *timeout = makeSpin(500,120000,settings_.operationTimeoutMs);timeout->setSuffix(tr(" ms"));timeout->setSingleStep(500);
    auto *retention = makeSpin(1,3650,settings_.retentionDays);retention->setSuffix(tr(" days"));
    auto *capacity = makeSpin(16,4096,settings_.storageLimitMiB);capacity->setSuffix(tr(" MiB"));
    auto *level = new QComboBox(page);level->addItems({tr("Debug"),tr("Info"),tr("Warning"),tr("Error")});level->setCurrentIndex(int(settings_.logLevel));
    layout->addRow(tr("Concurrent tasks"),workers);layout->addRow(tr("Operation timeout"),timeout);
    layout->addRow(tr("Observation retention"),retention);layout->addRow(tr("Observation storage budget"),capacity);layout->addRow(tr("Log level"),level);
    auto *policy = new QLabel(tr("Retention and capacity settings will apply when the observation store is available."));policy->setWordWrap(true);layout->addRow(policy);
    auto *save = new QPushButton(tr("Save settings"));layout->addRow(save);
    connect(save,&QPushButton::clicked,this,[=,this]{
        auto next=settings_;next.workerLimit=workers->value();next.operationTimeoutMs=timeout->value();next.retentionDays=retention->value();next.storageLimitMiB=capacity->value();next.logLevel=static_cast<LogLevel>(level->currentIndex());
        next.windowGeometry=saveGeometry();const auto result=store_.save(next);
        if(!result){statusBar()->showMessage(result.error().message);return;}
        settings_=next;tasks_.setConcurrency(next.workerLimit);emit settingsChanged();statusBar()->showMessage(tr("Settings saved."),5000);
    });
    return page;
}
void MainWindow::setRepository(CoreRepository *repository) {
    workspaces_->setRepository(repository);
}
void MainWindow::showError(const Error &error) {
    auto *message = new QMessageBox(QMessageBox::Warning,tr("Netraxis"),QString(),QMessageBox::Close,this);
    message->setAttribute(Qt::WA_DeleteOnClose);message->setTextFormat(Qt::PlainText);message->setText(error.message);message->open();
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (store_.load()) {
        auto saved=settings_;saved.windowGeometry=saveGeometry();
        const auto result=store_.save(saved);
        if(!result){statusBar()->showMessage(result.error().message);}
    }
    QMainWindow::closeEvent(event);
}
}
