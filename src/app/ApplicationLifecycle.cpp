#include "ApplicationLifecycle.h"
#include <QCryptographicHash>
#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>

namespace netraxis {
ApplicationLifecycle::ApplicationLifecycle(QObject *parent) : QObject(parent) {
    connect(&server_, &QLocalServer::newConnection, this, [this] {
        while (auto *socket = server_.nextPendingConnection()) {
            socket->close();
            socket->deleteLater();
            emit activationRequested();
        }
    });
}
ApplicationLifecycle::Start ApplicationLifecycle::start() {
    if (started_) return Start::Primary;
    const auto path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (path.isEmpty() || !QDir().mkpath(path)) {
        error_ = tr("Cannot create the application data directory.");
        return Start::Failed;
    }
    const auto name = "netraxis-" + QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex().left(24);
    lock_ = std::make_unique<QLockFile>(path + "/instance.lock");
    lock_->setStaleLockTime(0);
    if (!lock_->tryLock(0)) {
        QLocalSocket socket;
        socket.connectToServer(QString::fromLatin1(name));
        if (socket.waitForConnected(1500)) return Start::ActivatedExisting;
        if (lock_->error() == QLockFile::LockFailedError) {
            error_ = tr("Netraxis is already running; its activation channel is unavailable.");
            return Start::AlreadyRunning;
        }
        error_ = tr("Cannot lock the application data directory.");
        return Start::Failed;
    }
    QLocalServer::removeServer(QString::fromLatin1(name));
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_.listen(QString::fromLatin1(name))) {
        error_ = tr("Window activation is unavailable. Additional instances will be blocked.");
    }
    started_ = true;
    return Start::Primary;
}
void ApplicationLifecycle::shutdown() {
    if (!started_) return;
    emit stopping();
    server_.close();
    lock_->unlock();
    started_ = false;
}
}
