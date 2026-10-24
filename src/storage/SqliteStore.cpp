#include "SqliteStore.h"
#include "core/Paths.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QUuid>
#include <optional>

namespace netraxis {
namespace {
Error sqlError(const QSqlError &error) {
    const auto code=error.nativeErrorCode().toInt();
    return {code==11||code==26?ErrorCode::CorruptData:ErrorCode::Storage,
        code==5||code==6?"Local storage is busy. Retry after other operations finish.":"The local database operation failed.",
        code==5||code==6,"database"};
}
}
SqliteStore::SqliteStore() : owner_(QThread::currentThread()) {}
SqliteStore::~SqliteStore() { close(); }
bool SqliteStore::isOpen() const { return QThread::currentThread()==owner_ && db_.isOpen(); }
Result<QString> SqliteStore::defaultPath() {
    auto directory=applicationDataDirectory();
    if(!directory)return Result<QString>::failure(directory.error());
    return Result<QString>::success(directory.value()+"/netraxis.sqlite");
}
Status SqliteStore::open(const QString &absolutePath) {
    if(QThread::currentThread()!=owner_||db_.isOpen())return Status::failure({ErrorCode::Conflict,"Database connections must remain on their owning thread.",false,"database"});
    const QFileInfo file(absolutePath);
    if(!file.isAbsolute()||file.isSymLink()||(file.exists()&&!file.isFile())
        ||QFileInfo(absolutePath+"-wal").isSymLink()||QFileInfo(absolutePath+"-shm").isSymLink())
        return Status::failure({ErrorCode::InvalidInput,"An absolute local database path is required.",false,"database"});
    const bool newDirectory=!QFileInfo::exists(file.absolutePath());
    if(!QDir().mkpath(file.absolutePath())||(newDirectory&&!QFile::setPermissions(file.absolutePath(),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)))
        return Status::failure({ErrorCode::PermissionDenied,"Cannot secure the database directory.",false,"database"});
#ifndef Q_OS_WIN
    if(QFile::permissions(file.absolutePath())&(QFile::ReadGroup|QFile::WriteGroup|QFile::ExeGroup|QFile::ReadOther|QFile::WriteOther|QFile::ExeOther))
        return Status::failure({ErrorCode::PermissionDenied,"Choose a private directory for local storage.",false,"database"});
#endif
    name_="netraxis-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    db_=QSqlDatabase::addDatabase("QSQLITE",name_);db_.setDatabaseName(absolutePath);db_.setConnectOptions("QSQLITE_BUSY_TIMEOUT=3000");
    if(!db_.open()){const auto error=sqlError(db_.lastError());close();return Status::failure(error);}
    path_=absolutePath;
    if(!QFile::setPermissions(absolutePath,QFile::ReadOwner|QFile::WriteOwner)){close();return Status::failure({ErrorCode::PermissionDenied,"Cannot secure the database file.",false,"database"});}
    std::optional<Error> failure;
    {
        auto wal=execute("PRAGMA journal_mode=WAL");
        if(!wal)failure=wal.error();
        else if(!wal.value().next()||wal.value().value(0).toString().toLower()!="wal")failure=Error{ErrorCode::Storage,"WAL mode is unavailable for this database.",false,"database"};
    }
    if(!failure)for(auto sql:{"PRAGMA foreign_keys=ON","PRAGMA synchronous=NORMAL","PRAGMA secure_delete=ON","PRAGMA trusted_schema=OFF","PRAGMA temp_store=MEMORY"}) {
        auto result=execute(QString::fromLatin1(sql));if(!result){failure=result.error();break;}
    }
    if(failure){close();return Status::failure(*failure);}
    return success();
}
void SqliteStore::close() {
    if(name_.isEmpty())return;
    db_.close();db_=QSqlDatabase();QSqlDatabase::removeDatabase(name_);name_.clear();path_.clear();inTransaction_=false;
}
Result<QSqlQuery> SqliteStore::execute(const QString &sql, const QVariantList &values) {
    if(!isOpen())return Result<QSqlQuery>::failure({ErrorCode::Storage,"Database is not open on this thread.",false,"database"});
    QSqlQuery query(db_);
    if(!query.prepare(sql))return Result<QSqlQuery>::failure(sqlError(query.lastError()));
    for(const auto &value:values)query.addBindValue(value);
    if(!query.exec())return Result<QSqlQuery>::failure(sqlError(query.lastError()));
    return Result<QSqlQuery>::success(std::move(query));
}
Status SqliteStore::transaction(const std::function<Status()> &body) {
    if(!isOpen()||inTransaction_||!body)return Status::failure({ErrorCode::Conflict,"Cannot start this database transaction.",false,"database"});
    if(!db_.transaction())return Status::failure(sqlError(db_.lastError()));
    inTransaction_=true;
    const auto finish=[this](Status result){
        if(result&&!db_.commit())result=Status::failure(sqlError(db_.lastError()));
        if(!result)db_.rollback();
        inTransaction_=false;return result;
    };
    try{return finish(body());}
    catch(...){return finish(Status::failure({ErrorCode::Internal,"Database transaction aborted.",false,"database"}));}
}
Status SqliteStore::checkpoint() {
    auto result=execute("PRAGMA wal_checkpoint(TRUNCATE)");if(!result)return Status::failure(result.error());
    if(!result.value().next()||result.value().value(0).toInt()!=0)return Status::failure({ErrorCode::Storage,"Database checkpoint is busy.",true,"database"});
    return success();
}
}
