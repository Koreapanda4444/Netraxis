#pragma once
#include "NetworkPlatform.h"
#include <QMetaObject>
#include <QObject>
#include <QVariant>

namespace netraxis {
enum class NetworkEventKind { ReachabilityReported, TransportReported, CaptivePortalReported, MeteringReported };
struct SystemNetworkEvent {
    quint64 sequence = 0;
    NetworkEventKind kind;
    QDateTime observedAt;
    QString source;
    QVariant providerValue;
    bool refreshRequired = true;
};
class SystemNetworkEvents final : public QObject {
    Q_OBJECT
public:
    explicit SystemNetworkEvents(QObject *parent = nullptr);
    Status start();
    FeatureState state() const {return state_;}
    void stop();
signals:
    void changed(netraxis::SystemNetworkEvent event);
    void availabilityChanged();
private:
    void publish(NetworkEventKind kind,const QVariant &value,const QString &source);
    QVector<QMetaObject::Connection> connections_;
    FeatureState state_{Availability::Unknown,Verification::Unverified,"System event subscription has not started."};
    quint64 sequence_ = 0;
    bool active_ = false;
};
}
Q_DECLARE_METATYPE(netraxis::SystemNetworkEvent)
