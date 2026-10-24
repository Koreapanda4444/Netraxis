#pragma once
#include "Result.h"
#include <QDeadlineTimer>
#include <QHash>
#include <QObject>
#include <QThreadPool>
#include <QVariant>
#include <atomic>
#include <functional>
#include <memory>

namespace netraxis {
class TaskContext final {
public:
    TaskContext(std::shared_ptr<std::atomic_bool> cancelled, QDeadlineTimer deadline)
        : cancelled_(std::move(cancelled)), deadline_(deadline) {}
    bool cancelled() const { return cancelled_->load(); }
    bool expired() const { return deadline_.hasExpired(); }
    qint64 remainingTimeMs() const { return deadline_.remainingTime(); }
    bool shouldStop() const { return cancelled() || expired(); }
    Status check() const;
private:
    std::shared_ptr<std::atomic_bool> cancelled_;
    QDeadlineTimer deadline_;
};
using TaskResult = Result<QVariant>;
class TaskManager final : public QObject {
    Q_OBJECT
public:
    using Work = std::function<TaskResult(const TaskContext &)>;
    explicit TaskManager(QObject *parent = nullptr);
    ~TaskManager() override;
    Result<quint64> submit(Work work, int timeoutMs = 10000);
    Status setConcurrency(int limit);
    int pendingCount() const { return int(tasks_.size()); }
public slots:
    void cancel(quint64 id);
    void shutdown();
signals:
    void pendingChanged(int count);
    void finished(quint64 id, netraxis::TaskResult result);
private:
    QThreadPool pool_;
    QHash<quint64, std::shared_ptr<std::atomic_bool>> tasks_;
    quint64 nextId_ = 1;
    bool accepting_ = true;
};
}
Q_DECLARE_METATYPE(netraxis::TaskResult)
