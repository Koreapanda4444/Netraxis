#pragma once
#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <memory>

namespace netraxis {
class ApplicationLifecycle final : public QObject {
    Q_OBJECT
public:
    enum class Start { Primary, ActivatedExisting, AlreadyRunning, Failed };
    explicit ApplicationLifecycle(QObject *parent = nullptr);
    Start start();
    QString errorString() const { return error_; }
public slots:
    void shutdown();
signals:
    void activationRequested();
    void stopping();
private:
    QLocalServer server_;
    std::unique_ptr<QLockFile> lock_;
    QString error_;
    bool started_ = false;
};
}
