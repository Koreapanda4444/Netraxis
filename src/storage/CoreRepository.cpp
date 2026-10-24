#include "CoreRepository.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <cmath>

namespace netraxis {
namespace {
QString key(const QUuid &id){return id.toString(QUuid::WithoutBraces);}
Error corrupt(){return {ErrorCode::CorruptData,"A saved network record is invalid.",false,"repository"};}
Status done(Result<QSqlQuery> result){return result?success():Status::failure(result.error());}
QByteArray json(const QJsonObject &obj){return QJsonDocument(obj).toJson(QJsonDocument::Compact);}
Result<QJsonObject> object(const QByteArray &bytes){
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);
    if(bytes.size()>1024*1024||error.error!=QJsonParseError::NoError||!doc.isObject())return Result<QJsonObject>::failure(corrupt());
    return Result<QJsonObject>::success(doc.object());
}
bool integer(const QJsonValue &value,double min,double max){const auto n=value.toDouble(min-1);return value.isDouble()&&std::isfinite(n)&&std::floor(n)==n&&n>=min&&n<=max;}
Result<QByteArray> hardware(const QJsonValue &value){
    if(!value.isString())return Result<QByteArray>::failure(corrupt());
    const auto decoded=QByteArray::fromBase64Encoding(value.toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded||decoded.decoded.size()>32)return Result<QByteArray>::failure(corrupt());
    return Result<QByteArray>::success(decoded.decoded);
}
template<class T,class Decode> Result<QVector<T>> rows(Result<QSqlQuery> result,Decode decode){
    if(!result)return Result<QVector<T>>::failure(result.error());
    QVector<T> values;
    while(result.value().next()){
        auto value=decode(result.value());if(!value||!value.value().validate())return Result<QVector<T>>::failure(corrupt());
        values.push_back(value.value());
    }
    if(result.value().lastError().isValid())return Result<QVector<T>>::failure(corrupt());
    return Result<QVector<T>>::success(values);
}
}
Result<QVector<NetworkWorkspace>> CoreRepository::workspaces(){
    return rows<NetworkWorkspace>(store_.execute("SELECT id,name,created_at FROM workspaces ORDER BY name,id"),[](const QSqlQuery &q){
        return Result<NetworkWorkspace>::success({QUuid(q.value(0).toString()),q.value(1).toString(),QDateTime::fromMSecsSinceEpoch(q.value(2).toLongLong(),Qt::UTC)});
    });
}
Result<QVector<Host>> CoreRepository::hosts(){
    return rows<Host>(store_.execute("SELECT id,name,platform,native_id FROM hosts ORDER BY name,id"),[](const QSqlQuery &q){
        return Result<Host>::success({QUuid(q.value(0).toString()),q.value(1).toString(),q.value(2).toString(),q.value(3).toString()});
    });
}
Status CoreRepository::save(const NetworkWorkspace &value){
    auto valid=value.validate();if(!valid)return valid;
    return done(store_.execute("INSERT INTO workspaces(id,name,created_at) VALUES(?,?,?) ON CONFLICT(id) DO UPDATE SET name=excluded.name,created_at=excluded.created_at",{key(value.id),value.name,value.createdAt.toMSecsSinceEpoch()}));
}
Status CoreRepository::save(const Host &value){
    auto valid=value.validate();if(!valid)return valid;
    return done(store_.execute("INSERT INTO hosts(id,name,platform,native_id) VALUES(?,?,?,?) ON CONFLICT(id) DO UPDATE SET name=excluded.name,platform=excluded.platform,native_id=excluded.native_id",{key(value.id),value.name,value.platform,value.nativeId}));
}
Status CoreRepository::save(const NetworkInterface &value){
    auto valid=value.validate();if(!valid)return valid;
    QJsonObject payload{{"kind",int(value.kind)},{"hardware",QString::fromLatin1(value.hardwareAddress.toBase64())}};
    if(value.operational)payload.insert("operational",*value.operational);
    if(value.mtu)payload.insert("mtu",double(*value.mtu));
    QJsonArray addresses;
    for(const auto &a:value.addresses){
        QJsonObject item{{"address",a.address->toString()},{"prefix",a.prefix},{"origin",int(a.origin)}};
        if(a.preferredUntil.isValid())item.insert("preferred",a.preferredUntil.toUTC().toString(Qt::ISODateWithMs));
        if(a.validUntil.isValid())item.insert("valid",a.validUntil.toUTC().toString(Qt::ISODateWithMs));
        addresses.append(item);
    }
    payload.insert("addresses",addresses);const auto data=json(payload);
    if(data.size()>1024*1024)return Status::failure({ErrorCode::InvalidInput,"Interface record exceeds the local size limit.",false,"repository"});
    return done(store_.execute("INSERT INTO interfaces(id,host_id,native_id,name,payload) VALUES(?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET host_id=excluded.host_id,native_id=excluded.native_id,name=excluded.name,payload=excluded.payload",{key(value.id),key(value.hostId),value.nativeId,value.name,QString::fromUtf8(data)}));
}
Result<QVector<NetworkInterface>> CoreRepository::interfaces(const QUuid &hostId){
    return rows<NetworkInterface>(store_.execute("SELECT id,host_id,native_id,name,payload FROM interfaces WHERE host_id=? ORDER BY name,id",{key(hostId)}),[](const QSqlQuery &q)->Result<NetworkInterface>{
        auto parsed=object(q.value(4).toByteArray());if(!parsed)return Result<NetworkInterface>::failure(corrupt());
        const auto p=parsed.value();auto mac=hardware(p.value("hardware"));
        if(!integer(p.value("kind"),0,int(InterfaceKind::Virtual))||!mac||!p.value("addresses").isArray())return Result<NetworkInterface>::failure(corrupt());
        NetworkInterface value;value.id=QUuid(q.value(0).toString());value.hostId=QUuid(q.value(1).toString());value.nativeId=q.value(2).toString();value.name=q.value(3).toString();value.kind=static_cast<InterfaceKind>(p.value("kind").toInt());value.hardwareAddress=mac.value();
        if(p.contains("operational")){if(!p.value("operational").isBool())return Result<NetworkInterface>::failure(corrupt());value.operational=p.value("operational").toBool();}
        if(p.contains("mtu")){if(!integer(p.value("mtu"),1,4294967295.0))return Result<NetworkInterface>::failure(corrupt());value.mtu=static_cast<quint32>(p.value("mtu").toDouble());}
        for(const auto &entry:p.value("addresses").toArray()){
            if(!entry.isObject())return Result<NetworkInterface>::failure(corrupt());
            const auto a=entry.toObject();
            auto address=IpAddress::parse(a.value("address").toString());
            if(!address||!integer(a.value("prefix"),0,address.value().bitCount())||!integer(a.value("origin"),0,int(AddressOrigin::Slaac)))return Result<NetworkInterface>::failure(corrupt());
            InterfaceAddress assigned;assigned.address=address.value();assigned.prefix=a.value("prefix").toInt();assigned.origin=static_cast<AddressOrigin>(a.value("origin").toInt());
            for(auto field:{"preferred","valid"})if(a.contains(QLatin1String(field))){
                const auto text=a.value(QLatin1String(field));const auto date=QDateTime::fromString(text.toString(),Qt::ISODateWithMs);
                if(!text.isString()||!text.toString().endsWith('Z')||!date.isValid())return Result<NetworkInterface>::failure(corrupt());
                if(QLatin1String(field)==QLatin1String("preferred"))assigned.preferredUntil=date;else assigned.validUntil=date;
            }
            value.addresses.push_back(assigned);
        }
        return Result<NetworkInterface>::success(value);
    });
}
Status CoreRepository::save(const RouterProfile &value){
    auto valid=value.validate();if(!valid)return valid;
    const QVariant secret=value.secretReference?QVariant(key(*value.secretReference)):QVariant();
    return done(store_.execute("INSERT INTO routers(id,workspace_id,name,url,adapter_id,secret_ref) VALUES(?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET workspace_id=excluded.workspace_id,name=excluded.name,url=excluded.url,adapter_id=excluded.adapter_id,secret_ref=excluded.secret_ref",{key(value.id),key(value.workspaceId),value.name,value.managementUrl.toString(QUrl::FullyEncoded),value.adapterId,secret}));
}
Result<QVector<RouterProfile>> CoreRepository::routers(const QUuid &workspaceId){
    return rows<RouterProfile>(store_.execute("SELECT id,workspace_id,name,url,adapter_id,secret_ref FROM routers WHERE workspace_id=? ORDER BY name,id",{key(workspaceId)}),[](const QSqlQuery &q){
        RouterProfile value{QUuid(q.value(0).toString()),QUuid(q.value(1).toString()),q.value(2).toString(),QUrl(q.value(3).toString(),QUrl::StrictMode),q.value(4).toString(),{}};
        if(!q.value(5).isNull())value.secretReference=QUuid(q.value(5).toString());
        return Result<RouterProfile>::success(value);
    });
}
Status CoreRepository::save(const LanDevice &value){
    auto valid=value.validate();if(!valid)return valid;
    QJsonArray addresses;for(const auto &address:value.addresses)addresses.append(address.toString());
    QJsonObject payload{{"hardware",QString::fromLatin1(value.hardwareAddress.toBase64())},{"addresses",addresses}};
    if(value.lastSeen.isValid())payload.insert("last_seen",value.lastSeen.toUTC().toString(Qt::ISODateWithMs));
    return done(store_.execute("INSERT INTO devices(id,workspace_id,alias,payload) VALUES(?,?,?,?) ON CONFLICT(id) DO UPDATE SET workspace_id=excluded.workspace_id,alias=excluded.alias,payload=excluded.payload",{key(value.id),key(value.workspaceId),value.alias.isNull()?QStringLiteral(""):value.alias,QString::fromUtf8(json(payload))}));
}
Result<QVector<LanDevice>> CoreRepository::devices(const QUuid &workspaceId){
    return rows<LanDevice>(store_.execute("SELECT id,workspace_id,alias,payload FROM devices WHERE workspace_id=? ORDER BY alias,id",{key(workspaceId)}),[](const QSqlQuery &q)->Result<LanDevice>{
        auto parsed=object(q.value(3).toByteArray());if(!parsed)return Result<LanDevice>::failure(corrupt());const auto p=parsed.value();auto mac=hardware(p.value("hardware"));
        if(!mac||!p.value("addresses").isArray())return Result<LanDevice>::failure(corrupt());
        LanDevice value;value.id=QUuid(q.value(0).toString());value.workspaceId=QUuid(q.value(1).toString());value.alias=q.value(2).toString();value.hardwareAddress=mac.value();
        for(const auto &entry:p.value("addresses").toArray()){
            if(!entry.isString())return Result<LanDevice>::failure(corrupt());
            auto address=IpAddress::parse(entry.toString());if(!address)return Result<LanDevice>::failure(corrupt());
            value.addresses.push_back(address.value());
        }
        if(p.contains("last_seen")){const auto text=p.value("last_seen");value.lastSeen=QDateTime::fromString(text.toString(),Qt::ISODateWithMs);if(!text.isString()||!text.toString().endsWith('Z')||!value.lastSeen.isValid())return Result<LanDevice>::failure(corrupt());}
        return Result<LanDevice>::success(value);
    });
}
Status CoreRepository::atomic(const std::function<Status()> &body){
    return store_.inTransaction()?body():store_.transaction(body);
}
Status CoreRepository::remove(const QString &table,const QUuid &id){
    if(id.isNull()||!QStringList{"interfaces","routers","devices"}.contains(table))return Status::failure({ErrorCode::InvalidInput,"Invalid removal target.",false,"repository"});
    return atomic([&]{
        auto status=done(store_.execute("DELETE FROM observations WHERE subject_id=?",{key(id)}));if(!status)return status;
        return done(store_.execute("DELETE FROM "+table+" WHERE id=?",{key(id)}));
    });
}
Status CoreRepository::removeInterface(const QUuid &id){return remove("interfaces",id);}
Status CoreRepository::removeRouter(const QUuid &id){return remove("routers",id);}
Status CoreRepository::removeDevice(const QUuid &id){return remove("devices",id);}
Status CoreRepository::removeWorkspace(const QUuid &id){
    if(id.isNull())return Status::failure({ErrorCode::InvalidInput,"Invalid workspace.",false,"repository"});
    return atomic([&]{
        auto status=done(store_.execute("DELETE FROM observations WHERE subject_id=? OR subject_id IN (SELECT id FROM routers WHERE workspace_id=? UNION SELECT id FROM devices WHERE workspace_id=?)",{key(id),key(id),key(id)}));if(!status)return status;
        return done(store_.execute("DELETE FROM workspaces WHERE id=?",{key(id)}));
    });
}
Status CoreRepository::removeHost(const QUuid &id){
    if(id.isNull())return Status::failure({ErrorCode::InvalidInput,"Invalid host.",false,"repository"});
    return atomic([&]{
        auto status=done(store_.execute("DELETE FROM observations WHERE subject_id=? OR subject_id IN (SELECT id FROM interfaces WHERE host_id=?)",{key(id),key(id)}));if(!status)return status;
        return done(store_.execute("DELETE FROM hosts WHERE id=?",{key(id)}));
    });
}
Status CoreRepository::addEvidence(const Evidence &value){
    auto valid=value.validate();if(!valid)return valid;
    return done(store_.execute("INSERT INTO observations(id,subject_id,observed_at,evidence) VALUES(?,?,?,?)",{key(value.id),key(value.subjectId),value.observedAt.toMSecsSinceEpoch(),QString::fromUtf8(json(value.toJson()))}));
}
Result<QVector<Evidence>> CoreRepository::evidence(const QUuid &subject,int limit){
    if(subject.isNull()||limit<1||limit>1000)return Result<QVector<Evidence>>::failure({ErrorCode::InvalidInput,"Invalid evidence query.",false,"repository"});
    return rows<Evidence>(store_.execute("SELECT id,subject_id,observed_at,evidence FROM observations WHERE subject_id=? ORDER BY observed_at DESC,id DESC LIMIT ?",{key(subject),limit}),[](const QSqlQuery &q)->Result<Evidence>{auto parsed=object(q.value(3).toByteArray());
        if(!parsed)return Result<Evidence>::failure(parsed.error());
        auto value=Evidence::fromJson(parsed.value());
        if(!value||value.value().id!=QUuid(q.value(0).toString())||value.value().subjectId!=QUuid(q.value(1).toString())||value.value().observedAt.toMSecsSinceEpoch()!=q.value(2).toLongLong())return Result<Evidence>::failure(corrupt());
        return value;});
}
Status CoreRepository::transaction(const std::function<Status(CoreRepository &)> &body){
    if(!body)return Status::failure({ErrorCode::InvalidInput,"A repository transaction body is required.",false,"repository"});
    return atomic([&]{return body(*this);});
}
}
