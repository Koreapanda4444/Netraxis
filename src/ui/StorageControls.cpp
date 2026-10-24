#include "StorageControls.h"
#include "StatusSurface.h"
#include "storage/Maintenance.h"
#include "storage/Schema.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace netraxis {
StorageControls::StorageControls(TaskManager &tasks,RuntimeSettings &settings,QWidget *parent):QWidget(parent),tasks_(tasks),settings_(settings){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,12,0,0);layout->addWidget(new QLabel(tr("<h3>Local data</h3>")));
    auto *description=new QLabel(tr("Observation cleanup runs every 15 minutes using the saved retention period and storage budget. Saved profiles are retained."));description->setWordWrap(true);layout->addWidget(description);
    auto *buttons=new QHBoxLayout;clean_=new QPushButton(tr("Clean old observations"));clear_=new QPushButton(tr("Delete saved networks"));cancel_=new QPushButton(tr("Cancel cleanup"));
    buttons->addWidget(clean_);buttons->addWidget(clear_);buttons->addWidget(cancel_);layout->addLayout(buttons);
    state_=new StatusSurface({Availability::Unknown,Verification::Unverified,tr("Local storage is unavailable.")},this);layout->addWidget(state_);
    connect(clean_,&QPushButton::clicked,this,[this]{start(false);});
    connect(clear_,&QPushButton::clicked,this,[this]{
        QMessageBox prompt(QMessageBox::Warning,tr("Delete saved networks"),tr("Delete all saved workspaces, hosts, devices, observations and migration backups? This cannot be undone."),QMessageBox::Yes|QMessageBox::No,this);prompt.setDefaultButton(QMessageBox::No);
        if(prompt.exec()==QMessageBox::Yes)start(true);
    });
    connect(cancel_,&QPushButton::clicked,this,[this]{if(task_)tasks_.cancel(task_);});
    connect(state_,&StatusSurface::retryRequested,this,[this]{start(false);});
    timer_.setInterval(15*60*1000);connect(&timer_,&QTimer::timeout,this,[this]{start(false);});
    connect(&tasks_,&TaskManager::finished,this,[this](quint64 id,const TaskResult &result){
        if(id!=task_)return;
        task_=0;setBusy(false);emit dataChanged();
        if(!result){auto error=result.error();if(error.code==ErrorCode::Cancelled||error.code==ErrorCode::Timeout)error.message+=tr(" Completed changes may already have been saved.");state_->setError(error);return;}
        const auto report=result.value().toMap();
        if(clearing_){state_->setState({Availability::Supported,Verification::Healthy,tr("Saved network records and migration backups were deleted. The local database was compacted. Settings and application event logs are retained.")});emit dataDeleted();}
        else{
            const bool within=report.value("within_budget").toBool();
            state_->setState({Availability::Supported,within?Verification::Healthy:Verification::Degraded,
                tr("Removed %1 old observation(s). Local storage: %2 MiB.").arg(report.value("removed").toLongLong()).arg(report.value("bytes").toLongLong()/1048576.0,0,'f',2)
                    +(within?QString():tr(" Saved profiles exceed the budget and require manual review."))});
        }
    });
    setBusy(false);
}
void StorageControls::setDatabasePath(const QString &path){if(path_==path)return;path_=path;setBusy(false);if(path.isEmpty()){timer_.stop();return;}timer_.start();QTimer::singleShot(0,this,[this]{start(false);});}
void StorageControls::setBusy(bool busy){clean_->setEnabled(!busy&&!path_.isEmpty());clear_->setEnabled(!busy&&!path_.isEmpty());cancel_->setEnabled(busy&&!clearing_);emit busyChanged(busy);}
void StorageControls::cleanNow(){start(false);}
void StorageControls::start(bool clear){
    if(task_||path_.isEmpty())return;
    clearing_=clear;
    const auto path=path_;const RetentionPolicy policy{settings_.retentionDays,qint64(settings_.storageLimitMiB)*1024*1024};
    auto task=tasks_.submit([path,policy,clear](const TaskContext &context)->TaskResult{
        auto ready=context.check();if(!ready)return TaskResult::failure(ready.error());
        SqliteStore store;ready=store.open(path);if(!ready)return TaskResult::failure(ready.error());ready=Schema::migrate(store);if(!ready)return TaskResult::failure(ready.error());
        if(clear){ready=Maintenance::clearSavedData(store);return ready?TaskResult::success(QVariantMap{}):TaskResult::failure(ready.error());}
        auto report=Maintenance::retain(store,policy,QDateTime::currentDateTimeUtc(),[&context]{return context.shouldStop();});
        if(!report)return TaskResult::failure(report.error());
        return TaskResult::success(QVariantMap{{"removed",report.value().removedObservations},{"bytes",report.value().fileBytes},{"within_budget",report.value().withinBudget}});
    },120000);
    if(!task){state_->setError(task.error());return;}
    task_=task.value();setBusy(true);state_->setState({Availability::Supported,Verification::Unverified,clear?tr("Deleting saved local records…"):tr("Applying observation retention policy…")});
}
}
