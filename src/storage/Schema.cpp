#include "Schema.h"
#include <QDateTime>
#include <QFile>
#include <QStringList>
#include <QMap>
#include <QUuid>

namespace netraxis {
namespace {
Status corrupt(){return Status::failure({ErrorCode::CorruptData,"Local database integrity validation failed. Preserve the file for recovery.",false,"schema"});}
}
Status Schema::checkIntegrity(SqliteStore &store) {
    {
        auto query=store.execute("PRAGMA quick_check");if(!query)return Status::failure(query.error());
        if(!query.value().next()||query.value().value(0).toString()!="ok"||query.value().next())return corrupt();
    }
    auto query=store.execute("PRAGMA foreign_key_check");if(!query)return Status::failure(query.error());
    if(query.value().next())return corrupt();
    int version=0;
    {
        auto versionQuery=store.execute("PRAGMA user_version");if(!versionQuery)return Status::failure(versionQuery.error());
        if(!versionQuery.value().next())return corrupt();
        version=versionQuery.value().value(0).toInt();
    }
    if(version>=1&&version<=CurrentVersion){
        QMap<QString,QStringList> tables{
            {"workspaces",{"id","name","created_at"}}, {"hosts",{"id","name","platform","native_id"}},
            {"interfaces",{"id","host_id","native_id","name","payload"}}, {"routers",{"id","workspace_id","name","url","adapter_id","secret_ref"}},
            {"devices",{"id","workspace_id","alias","payload"}}};
        if(version>=2)tables.insert("observations",{"id","subject_id","observed_at","evidence"});
        for(auto it=tables.cbegin();it!=tables.cend();++it){
            auto columns=store.execute("PRAGMA table_info("+it.key()+")");if(!columns)return Status::failure(columns.error());
            QStringList actual;while(columns.value().next())actual.push_back(columns.value().value(1).toString());
            if(actual!=it.value())return corrupt();
        }
    }
    return success();
}
Status Schema::migrate(SqliteStore &store) {
    auto integrity=checkIntegrity(store);if(!integrity)return integrity;
    int version;
    {
        auto query=store.execute("PRAGMA user_version");if(!query)return Status::failure(query.error());
        if(!query.value().next())return corrupt();
        version=query.value().value(0).toInt();
    }
    if(version<0||version>CurrentVersion)return Status::failure({ErrorCode::Unsupported,"This database was created by an unsupported Netraxis version. It has not been migrated.",false,"schema"});
    if(version==CurrentVersion)return success();
    if(version==0) {
        auto query=store.execute("SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'");
        if(!query)return Status::failure(query.error());
        if(!query.value().next()||query.value().value(0).toInt()!=0)return Status::failure({ErrorCode::CorruptData,"Unversioned database tables cannot be migrated automatically.",false,"schema"});
    } else {
        const auto backup=store.path()+".migration-"+QString::number(version)+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".sqlite";
        {
            auto query=store.execute("VACUUM INTO ?",{backup});if(!query)return Status::failure(query.error());
        }
        if(!QFile::setPermissions(backup,QFile::ReadOwner|QFile::WriteOwner))return Status::failure({ErrorCode::PermissionDenied,"Cannot secure the pre-migration database backup.",false,"schema"});
    }
    return store.transaction([&]{
        if(version<1) {
            const QStringList statements{
                "CREATE TABLE workspaces(id TEXT PRIMARY KEY NOT NULL,name TEXT NOT NULL,created_at INTEGER NOT NULL)",
                "CREATE TABLE hosts(id TEXT PRIMARY KEY NOT NULL,name TEXT NOT NULL,platform TEXT NOT NULL,native_id TEXT NOT NULL,UNIQUE(platform,native_id))",
                "CREATE TABLE interfaces(id TEXT PRIMARY KEY NOT NULL,host_id TEXT NOT NULL REFERENCES hosts(id) ON DELETE CASCADE,native_id TEXT NOT NULL,name TEXT NOT NULL,payload TEXT NOT NULL,UNIQUE(host_id,native_id))",
                "CREATE TABLE routers(id TEXT PRIMARY KEY NOT NULL,workspace_id TEXT NOT NULL REFERENCES workspaces(id) ON DELETE CASCADE,name TEXT NOT NULL,url TEXT NOT NULL,adapter_id TEXT NOT NULL,secret_ref TEXT)",
                "CREATE TABLE devices(id TEXT PRIMARY KEY NOT NULL,workspace_id TEXT NOT NULL REFERENCES workspaces(id) ON DELETE CASCADE,alias TEXT NOT NULL,payload TEXT NOT NULL)",
                "CREATE INDEX routers_workspace ON routers(workspace_id)",
                "CREATE INDEX devices_workspace ON devices(workspace_id)",
                "PRAGMA user_version=1"};
            for(const auto &sql:statements){auto query=store.execute(sql);if(!query)return Status::failure(query.error());}
        }
        if(version<2) {
            const QStringList statements{
                "CREATE TABLE observations(id TEXT PRIMARY KEY NOT NULL,subject_id TEXT NOT NULL,observed_at INTEGER NOT NULL,evidence TEXT NOT NULL)",
                "CREATE INDEX observations_time ON observations(observed_at,id)",
                "CREATE INDEX observations_subject ON observations(subject_id,observed_at)",
                "PRAGMA user_version=2"};
            for(const auto &sql:statements){auto query=store.execute(sql);if(!query)return Status::failure(query.error());}
        }
        return checkIntegrity(store);
    });
}
}
