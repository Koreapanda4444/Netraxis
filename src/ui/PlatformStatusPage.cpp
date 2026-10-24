#include "PlatformStatusPage.h"
#include "StatusSurface.h"
#include "platform/base/PlatformFactory.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace netraxis {
namespace {
QString verification(Verification state){
    switch(state){
    case Verification::Healthy:return QObject::tr("Observed");
    case Verification::Degraded:return QObject::tr("Partial");
    case Verification::Failed:return QObject::tr("Failed");
    case Verification::Skipped:return QObject::tr("Skipped");
    case Verification::Unverified:return QObject::tr("Unverified");
    }
    return QObject::tr("Unverified");
}
QLabel *plainLabel(QWidget *parent){
    auto *label=new QLabel(parent);label->setTextFormat(Qt::PlainText);label->setWordWrap(true);return label;
}
QTableWidget *table(const QStringList &columns,const QString &name,QWidget *parent){
    auto *result=new QTableWidget(0,int(columns.size()),parent);result->setObjectName(name);
    result->setHorizontalHeaderLabels(columns);result->setEditTriggers(QAbstractItemView::NoEditTriggers);
    result->setSelectionBehavior(QAbstractItemView::SelectRows);result->verticalHeader()->hide();
    result->horizontalHeader()->setStretchLastSection(true);result->setMinimumHeight(180);
    return result;
}
void cell(QTableWidget *table,int row,int column,const QString &text){
    auto *item=new QTableWidgetItem(text);item->setToolTip(text);table->setItem(row,column,item);
}
}
PlatformStatusPage::PlatformStatusPage(TaskManager &tasks,int timeoutMs,QWidget *parent,std::shared_ptr<INetworkPlatform> platform)
    :QWidget(parent),platform_(std::move(platform)),timeoutMs_(qBound(500,timeoutMs,120000)){
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    layout->addWidget(new QLabel(tr("<h3>Local network platform</h3>"),this));
    provider_=plainLabel(this);provider_->setObjectName("platformProvider");layout->addWidget(provider_);
    permissions_=plainLabel(this);permissions_->setObjectName("platformPermissions");layout->addWidget(permissions_);
    freshness_=plainLabel(this);freshness_->setObjectName("platformFreshness");layout->addWidget(freshness_);
    status_=new StatusSurface({Availability::Unknown,Verification::Unverified,tr("Native collection has not run. Internet and service reachability are unverified.")},this);layout->addWidget(status_);
    auto *actions=new QHBoxLayout;refresh_=new QPushButton(tr("Refresh platform"),this);cancel_=new QPushButton(tr("Cancel refresh"),this);cancel_->setEnabled(false);
    actions->addWidget(refresh_);actions->addWidget(cancel_);actions->addStretch();layout->addLayout(actions);
    capabilities_=table({tr("Feature"),tr("Support / permission"),tr("Collection"),tr("Reason")},"platformCapabilities",this);
    capabilities_->setMinimumHeight(260);layout->addWidget(capabilities_);
    layout->addWidget(new QLabel(tr("<h3>Observed interfaces</h3>"),this));
    interfaces_=table({tr("Interface"),tr("Native ID"),tr("Link state"),tr("Addresses"),tr("Source")},"platformInterfaces",this);layout->addWidget(interfaces_);
    auto created=platform_?Result<std::shared_ptr<INetworkPlatform>>::success(platform_):createNetworkPlatform();
    if(!created){provider_->setText(tr("No local platform provider is available."));status_->setError(created.error());refresh_->setEnabled(false);freshness();return;}
    platform_=created.value();
    coordinator_=new PlatformCoordinator(tasks,platform_,this);
    events_.start();capabilities(inspectCapabilities(*platform_));
    debounce_.setSingleShot(true);debounce_.setInterval(250);
    connect(&debounce_,&QTimer::timeout,this,[this]{refresh();});
    connect(&events_,&SystemNetworkEvents::changed,this,[this](const SystemNetworkEvent &event){
        if(!event.refreshRequired)return;
        if(refreshing_)refreshPending_=true;else debounce_.start();
    });
    connect(&events_,&SystemNetworkEvents::availabilityChanged,this,[this]{capabilities(snapshot_?snapshot_->capabilities:inspectCapabilities(*platform_));});
    connect(refresh_,&QPushButton::clicked,this,[this]{refresh();});
    connect(cancel_,&QPushButton::clicked,coordinator_,&PlatformCoordinator::cancelRefresh);
    connect(status_,&StatusSurface::retryRequested,this,[this]{refresh();});
    connect(coordinator_,&PlatformCoordinator::refreshingChanged,this,[this](bool active){
        refreshing_=active;refresh_->setEnabled(!active);cancel_->setEnabled(active);
        if(!active&&refreshPending_){refreshPending_=false;debounce_.start();}
    });
    connect(coordinator_,&PlatformCoordinator::snapshotReady,this,&PlatformStatusPage::display);
    connect(coordinator_,&PlatformCoordinator::failed,this,[this](const Error &error){
        refreshFailed_=true;status_->setError(error);
        for(int row=0;row<capabilities_->rowCount();++row){
            auto *state=capabilities_->item(row,2);
            if(state&&(state->text()==verification(Verification::Healthy)||state->text()==verification(Verification::Degraded)))state->setText(tr("Unverified after refresh failure"));
        }
        freshness();
    });
    freshnessTimer_.setInterval(1000);connect(&freshnessTimer_,&QTimer::timeout,this,&PlatformStatusPage::freshness);freshnessTimer_.start();
    freshness();QTimer::singleShot(0,this,[this]{refresh();});
}
PlatformStatusPage::~PlatformStatusPage(){
    debounce_.stop();freshnessTimer_.stop();events_.disconnect(this);events_.stop();if(coordinator_)coordinator_->stop();
}
void PlatformStatusPage::setTimeout(int timeoutMs){timeoutMs_=qBound(500,timeoutMs,120000);}
void PlatformStatusPage::refresh(){
    if(!coordinator_||refreshing_)return;
    debounce_.stop();auto started=coordinator_->refresh(timeoutMs_);
    if(!started){refreshFailed_=true;status_->setError(started.error());return;}
    status_->setState({Availability::ReadOnly,Verification::Unverified,tr("Collecting local state. Previous observations remain historical until this refresh completes.")});
}
void PlatformStatusPage::capabilities(CapabilityReport report){
    provider_->setText(tr("Platform: %1 — Provider: %2").arg(report.platform.name,report.platform.implementation));
    const auto level=report.permissions.level;
    permissions_->setText(tr("Process permission: %1. Configuration changes require separate explicit approval.").arg(level==PrivilegeLevel::Elevated?tr("administrator"):level==PrivilegeLevel::Standard?tr("standard user"):tr("unknown")));
    if(report.permissions.error)permissions_->setText(permissions_->text()+"\n"+report.permissions.error->message);
    capabilities_->setRowCount(int(report.features.size()));
    int row=0;
    for(auto feature:report.features){
        if(feature.feature==PlatformFeature::Events)feature.state=events_.state();
        cell(capabilities_,row,0,platformFeatureName(feature.feature));cell(capabilities_,row,1,availabilityLabel(feature.state.availability));
        cell(capabilities_,row,2,verification(feature.state.verification));cell(capabilities_,row,3,feature.state.reason);++row;
    }
    capabilities_->resizeColumnsToContents();capabilities_->setColumnWidth(3,400);capabilities_->resizeRowsToContents();
}
void PlatformStatusPage::display(const PlatformSnapshot &snapshot){
    snapshot_=snapshot;refreshFailed_=false;
    auto report=snapshot.capabilities;
    for(auto &feature:report.features){
        if(feature.feature==PlatformFeature::Interfaces||(feature.feature==PlatformFeature::Addresses&&feature.canRead))feature.state=snapshot.interfaces.state;
        else if(feature.feature==PlatformFeature::Routes)feature.state=snapshot.routes.state;
        else if(feature.feature==PlatformFeature::Resolvers)feature.state=snapshot.resolvers.state;
    }
    capabilities(std::move(report));interfaces_->setRowCount(0);
    if(snapshot.interfaces.observations){
        int row=0;
        for(const auto &observation:*snapshot.interfaces.observations){
            interfaces_->insertRow(row);const auto &value=observation.value;
            QStringList addresses;for(const auto &address:value.addresses)addresses.push_back(address.address->toString()+"/"+QString::number(address.prefix));
            cell(interfaces_,row,0,value.name);cell(interfaces_,row,1,value.nativeId);
            cell(interfaces_,row,2,value.operational?(*value.operational?tr("Running"):tr("Not running")):tr("Unknown"));
            cell(interfaces_,row,3,addresses.isEmpty()?tr("No addresses observed"):addresses.join('\n'));
            cell(interfaces_,row,4,observation.evidence.source+(observation.evidence.errorCode?tr(" (partial fields)"):QString()));++row;
        }
        status_->setState({Availability::ReadOnly,snapshot.interfaces.state.verification,tr("Collected %1 interfaces. Link state and assigned addresses do not establish internet or service reachability.").arg(interfaces_->rowCount())});
    }else if(snapshot.interfaces.error)status_->setError(*snapshot.interfaces.error);
    else status_->setState(snapshot.interfaces.state);
    interfaces_->resizeColumnsToContents();interfaces_->resizeRowsToContents();freshness();
}
void PlatformStatusPage::freshness(){
    if(!snapshot_){freshness_->setText(tr("No collected snapshot. Internet and service reachability are unverified."));return;}
    if(!snapshot_->interfaces.observations){freshness_->setText(tr("Last collection attempt: %1 — interface state is unavailable. Internet and service reachability remain unverified.").arg(snapshot_->completedAt.toLocalTime().toString(Qt::ISODate)));return;}
    const auto now=QDateTime::currentDateTimeUtc();
    bool stale=now<snapshot_->completedAt||snapshot_->completedAt.msecsTo(now)>=10000;
    if(snapshot_->interfaces.observations)for(const auto &value:*snapshot_->interfaces.observations)stale|=value.evidence.freshness(now)!=Freshness::Fresh;
    freshness_->setText(tr("Last collection: %1 — %2. Internet and service reachability remain unverified.")
        .arg(snapshot_->completedAt.toLocalTime().toString(Qt::ISODate),refreshFailed_?tr("refresh failed; previous snapshot"):stale?tr("stale; refresh required"):tr("recent collection")));
}
}
