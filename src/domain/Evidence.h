#pragma once
#include "core/Availability.h"
#include <QDateTime>
#include <QJsonObject>
#include <QUuid>
#include <optional>

namespace netraxis {
enum class EvidenceKind { Observed, DeviceReported, Inferred, UserDefined };
enum class Freshness { Unknown, Fresh, Stale, ClockSkew };
struct Evidence {
    QUuid id;
    QUuid subjectId;
    EvidenceKind kind = EvidenceKind::Inferred;
    QString source;
    QString authority;
    QDateTime observedAt;
    std::optional<double> confidence;
    std::optional<qint64> validForMs;
    Availability capability = Availability::Unknown;
    std::optional<ErrorCode> errorCode;
    Status validate() const;
    Freshness freshness(const QDateTime &now) const;
    QJsonObject toJson() const;
    static Result<Evidence> fromJson(const QJsonObject &object);
};
template<class T> struct Observation {
    T value;
    Evidence evidence;
};
}
