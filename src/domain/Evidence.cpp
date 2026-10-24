#include "Evidence.h"
#include <cmath>
#include <limits>

namespace netraxis {
namespace {
Error invalid(){return {ErrorCode::CorruptData,"Invalid evidence metadata.",false,"evidence"};}
}
Status Evidence::validate() const {
    if(id.isNull()||subjectId.isNull()||source.trimmed().isEmpty()||source.size()>256||authority.size()>256||!observedAt.isValid()
        ||int(kind)<0||int(kind)>int(EvidenceKind::UserDefined)||int(capability)<0||int(capability)>int(Availability::Unknown)
        ||(confidence&&(!std::isfinite(*confidence)||*confidence<0||*confidence>1))
        ||(validForMs&&(*validForMs<0||*validForMs>3650LL*86400000))
        ||(errorCode&&(int(*errorCode)<0||int(*errorCode)>int(ErrorCode::Internal))))return Status::failure(invalid());
    return success();
}
Freshness Evidence::freshness(const QDateTime &now) const {
    if(!now.isValid()||!observedAt.isValid())return Freshness::Unknown;
    if(now<observedAt)return Freshness::ClockSkew;
    if(!validForMs)return Freshness::Unknown;
    return observedAt.msecsTo(now)>=*validForMs?Freshness::Stale:Freshness::Fresh;
}
QJsonObject Evidence::toJson() const {
    QJsonObject obj{{"version",1},{"id",id.toString(QUuid::WithoutBraces)},{"subject",subjectId.toString(QUuid::WithoutBraces)},
        {"kind",int(kind)},{"source",source},{"authority",authority},{"observed_at",observedAt.toUTC().toString(Qt::ISODateWithMs)},
        {"capability",int(capability)}};
    if(confidence)obj.insert("confidence",*confidence);
    if(validForMs)obj.insert("valid_for_ms",double(*validForMs));
    if(errorCode)obj.insert("error_code",int(*errorCode));
    return obj;
}
Result<Evidence> Evidence::fromJson(const QJsonObject &obj) {
    for(auto key:{"id","subject","source","authority","observed_at"})if(!obj.value(QLatin1String(key)).isString())return Result<Evidence>::failure(invalid());
    for(auto key:{"version","kind","capability"}){
        const auto value=obj.value(QLatin1String(key));const auto n=value.toDouble(std::numeric_limits<double>::quiet_NaN());
        if(!value.isDouble()||!std::isfinite(n)||std::floor(n)!=n)return Result<Evidence>::failure(invalid());
    }
    if(obj.value("version").toInt(-1)!=1)return Result<Evidence>::failure(invalid());
    Evidence e;e.id=QUuid(obj.value("id").toString());e.subjectId=QUuid(obj.value("subject").toString());
    e.kind=static_cast<EvidenceKind>(obj.value("kind").toInt(-1));e.source=obj.value("source").toString();e.authority=obj.value("authority").toString();
    const auto timestamp=obj.value("observed_at").toString();
    if(!timestamp.endsWith('Z'))return Result<Evidence>::failure(invalid());
    e.observedAt=QDateTime::fromString(timestamp,Qt::ISODateWithMs);e.capability=static_cast<Availability>(obj.value("capability").toInt(-1));
    if(obj.contains("confidence")){
        if(!obj.value("confidence").isDouble())return Result<Evidence>::failure(invalid());
        e.confidence=obj.value("confidence").toDouble();
    }
    if(obj.contains("valid_for_ms")){
        const auto value=obj.value("valid_for_ms");const double n=value.toDouble(-1);
        if(!value.isDouble()||!std::isfinite(n)||n<0||n>3650LL*86400000||std::floor(n)!=n)return Result<Evidence>::failure(invalid());
        e.validForMs=static_cast<qint64>(n);
    }
    if(obj.contains("error_code")){
        const auto value=obj.value("error_code");const auto n=value.toDouble(-1);
        if(!value.isDouble()||!std::isfinite(n)||std::floor(n)!=n||n<0||n>int(ErrorCode::Internal))return Result<Evidence>::failure(invalid());
        e.errorCode=static_cast<ErrorCode>(int(n));
    }
    const auto valid=e.validate();if(!valid)return Result<Evidence>::failure(valid.error());
    return Result<Evidence>::success(e);
}
}
