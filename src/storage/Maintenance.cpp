#include "Maintenance.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace netraxis {
namespace {
Result<qint64> scalar(SqliteStore &store,const QString &sql){
    auto query=store.execute(sql);if(!query)return Result<qint64>::failure(query.error());
    if(!query.value().next())return Result<qint64>::failure({ErrorCode::Storage,"Cannot measure local storage.",false,"maintenance"});
    return Result<qint64>::success(query.value().value(0).toLongLong());
}
}
Result<qint64> Maintenance::liveBytes(SqliteStore &store){
    auto size=scalar(store,"PRAGMA page_size");auto count=scalar(store,"PRAGMA page_count");auto free=scalar(store,"PRAGMA freelist_count");
    if(!size)return Result<qint64>::failure(size.error());
    if(!count)return Result<qint64>::failure(count.error());
    if(!free)return Result<qint64>::failure(free.error());
    if(size.value()<512||size.value()>65536||count.value()<free.value()||free.value()<0||count.value()>4294967295LL)
        return Result<qint64>::failure({ErrorCode::CorruptData,"Invalid database size metadata.",false,"maintenance"});
    return Result<qint64>::success((count.value()-free.value())*size.value());
}
Status Maintenance::compact(SqliteStore &store){
    auto checkpoint=store.checkpoint();if(!checkpoint)return checkpoint;
    {
        auto vacuum=store.execute("VACUUM");if(!vacuum)return Status::failure(vacuum.error());
    }
    return store.checkpoint();
}
Result<MaintenanceReport> Maintenance::retain(SqliteStore &store,const RetentionPolicy &policy,const QDateTime &now,const std::function<bool()> &cancelled){
    if(policy.days<1||policy.days>3650||policy.maxBytes<16LL*1024*1024||policy.maxBytes>4096LL*1024*1024||!now.isValid())
        return Result<MaintenanceReport>::failure({ErrorCode::InvalidInput,"Invalid observation retention policy.",false,"maintenance"});
    MaintenanceReport report;
    const auto cutoff=now.toUTC().addDays(-policy.days).toMSecsSinceEpoch();
    const auto prune=[&](bool expired)->Result<qint64>{
        if(cancelled&&cancelled())return Result<qint64>::failure({ErrorCode::Cancelled,"Observation cleanup cancelled. Completed batches remain deleted.",false,"maintenance"});
        const auto sql=expired?QString("DELETE FROM observations WHERE id IN (SELECT id FROM observations WHERE observed_at<? ORDER BY observed_at,id LIMIT 256)"):
            QString("DELETE FROM observations WHERE id IN (SELECT id FROM observations ORDER BY observed_at,id LIMIT 256)");
        auto query=store.execute(sql,expired?QVariantList{cutoff}:QVariantList{});if(!query)return Result<qint64>::failure(query.error());
        return Result<qint64>::success(query.value().numRowsAffected());
    };
    while(true){auto removed=prune(true);if(!removed)return Result<MaintenanceReport>::failure(removed.error());report.removedObservations+=removed.value();if(removed.value()==0)break;}
    while(true){
        auto bytes=liveBytes(store);if(!bytes)return Result<MaintenanceReport>::failure(bytes.error());report.liveBytes=bytes.value();
        if(report.liveBytes<=policy.maxBytes)break;
        auto removed=prune(false);if(!removed)return Result<MaintenanceReport>::failure(removed.error());report.removedObservations+=removed.value();
        if(removed.value()==0){report.withinBudget=false;break;}
    }
    const auto checkpoint=report.removedObservations?compact(store):store.checkpoint();
    if(!checkpoint)return Result<MaintenanceReport>::failure({checkpoint.error().code,"Cleanup changes were saved, but database compaction is pending. Close other storage operations and retry.",true,"maintenance"});
    const QFileInfo database(store.path());QDir directory(database.absolutePath());
    const auto backups=directory.entryInfoList({database.fileName()+".migration-*.sqlite"},QDir::Files|QDir::NoSymLinks,QDir::Time|QDir::Reversed);
    qint64 backupBytes=0;for(const auto &backup:backups)backupBytes+=backup.size();
    qint64 databaseBytes=QFileInfo(store.path()).size()+QFileInfo(store.path()+"-wal").size()+QFileInfo(store.path()+"-shm").size();
    for(const auto &backup:backups){
        if(report.removedObservations>0||backup.lastModified().toUTC()<now.toUTC().addDays(-policy.days)||databaseBytes+backupBytes>policy.maxBytes){
            if(!QFile::remove(backup.absoluteFilePath()))return Result<MaintenanceReport>::failure({ErrorCode::Storage,"Observation cleanup completed, but a migration backup could not be removed.",true,"maintenance"});
            backupBytes-=backup.size();
        }
    }
    report.fileBytes=databaseBytes+backupBytes;
    report.withinBudget=report.withinBudget&&report.fileBytes<=policy.maxBytes;
    const auto fresh=liveBytes(store);if(!fresh)return Result<MaintenanceReport>::failure(fresh.error());report.liveBytes=fresh.value();
    report.withinBudget=report.liveBytes<=policy.maxBytes&&report.fileBytes<=policy.maxBytes;
    return Result<MaintenanceReport>::success(report);
}
Status Maintenance::clearSavedData(SqliteStore &store){
    const auto cleared=store.transaction([&]{
        for(auto table:{"observations","devices","routers","interfaces","hosts","workspaces"}){
            auto query=store.execute("DELETE FROM "+QString::fromLatin1(table));if(!query)return Status::failure(query.error());
        }
        return success();
    });
    if(!cleared)return cleared;
    const auto compacted=compact(store);
    if(!compacted)return Status::failure({compacted.error().code,"Saved records were deleted. Local database compaction is still pending.",true,"privacy"});
    const QFileInfo database(store.path());QDir directory(database.absolutePath());
    const auto backups=directory.entryInfoList({database.fileName()+".migration-*.sqlite"},QDir::Files|QDir::NoSymLinks);
    for(const auto &backup:backups)if(!QFile::remove(backup.absoluteFilePath()))return Status::failure({ErrorCode::Storage,"Saved records were deleted, but a migration backup could not be removed.",true,"privacy"});
    return success();
}
}
