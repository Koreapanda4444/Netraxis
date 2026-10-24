#pragma once
#include "core/Result.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>
#include <functional>

namespace netraxis {
class SqliteStore final {
public:
    SqliteStore();
    ~SqliteStore();
    SqliteStore(const SqliteStore &) = delete;
    SqliteStore &operator=(const SqliteStore &) = delete;
    Status open(const QString &absolutePath);
    void close();
    bool isOpen() const;
    bool inTransaction() const { return inTransaction_; }
    QString path() const { return path_; }
    Result<QSqlQuery> execute(const QString &sql, const QVariantList &values = {});
    Status transaction(const std::function<Status()> &body);
    Status checkpoint();
    static Result<QString> defaultPath();
private:
    QThread *owner_;
    QSqlDatabase db_;
    QString name_;
    QString path_;
    bool inTransaction_ = false;
};
}
