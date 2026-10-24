#include "SystemNetworkEvents.h"
#include <QCoreApplication>
#include <QNetworkInformation>
#include <QThread>

namespace netraxis {
SystemNetworkEvents::SystemNetworkEvents(QObject *parent):QObject(parent){qRegisterMetaType<SystemNetworkEvent>();}
Status SystemNetworkEvents::start(){
    if(!QCoreApplication::instance()||QThread::currentThread()!=QCoreApplication::instance()->thread()||QThread::currentThread()!=thread())
        return Status::failure({ErrorCode::InvalidInput,"System events must be initialized on the application thread.",false,"events"});
    if(active_)return success();
    if(!QNetworkInformation::instance()&&!QNetworkInformation::loadDefaultBackend()){
        state_={Availability::Unsupported,Verification::Skipped,"No native Qt network information backend is available."};emit availabilityChanged();
        return Status::failure({ErrorCode::Unsupported,state_.reason,false,"events"});
    }
    auto *information=QNetworkInformation::instance();
    if(!information||information->supportedFeatures()==QNetworkInformation::Features{}){
        state_={Availability::Unsupported,Verification::Skipped,"The loaded network information provider has no observable event features."};emit availabilityChanged();
        return Status::failure({ErrorCode::Unsupported,state_.reason,false,"events"});
    }
    const auto source="Qt.QNetworkInformation/"+information->backendName();
    if(information->supports(QNetworkInformation::Feature::Reachability))connections_.push_back(connect(information,&QNetworkInformation::reachabilityChanged,this,[this,source](auto value){publish(NetworkEventKind::ReachabilityReported,int(value),source);}));
    if(information->supports(QNetworkInformation::Feature::TransportMedium))connections_.push_back(connect(information,&QNetworkInformation::transportMediumChanged,this,[this,source](auto value){publish(NetworkEventKind::TransportReported,int(value),source);}));
    if(information->supports(QNetworkInformation::Feature::CaptivePortal))connections_.push_back(connect(information,&QNetworkInformation::isBehindCaptivePortalChanged,this,[this,source](bool value){publish(NetworkEventKind::CaptivePortalReported,value,source);}));
    if(information->supports(QNetworkInformation::Feature::Metered))connections_.push_back(connect(information,&QNetworkInformation::isMeteredChanged,this,[this,source](bool value){publish(NetworkEventKind::MeteringReported,value,source);}));
    active_=true;state_={Availability::ReadOnly,Verification::Unverified,"Subscribed to "+source+". Provider reports request a refresh and do not prove service reachability."};emit availabilityChanged();
    return success();
}
void SystemNetworkEvents::publish(NetworkEventKind kind,const QVariant &value,const QString &source){
    if(!active_)return;
    emit changed({++sequence_,kind,QDateTime::currentDateTimeUtc(),source,value,true});
}
void SystemNetworkEvents::stop(){
    for(const auto &connection:std::as_const(connections_))disconnect(connection);
    connections_.clear();active_=false;state_={Availability::Unknown,Verification::Skipped,"System event subscription is stopped."};emit availabilityChanged();
}
}
