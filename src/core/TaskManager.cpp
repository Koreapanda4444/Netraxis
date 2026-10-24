#include "TaskManager.h"
#include <QThread>
#include <QRunnable>

namespace netraxis {
Status TaskContext::check() const {
    if (cancelled()) return Status::failure({ErrorCode::Cancelled, "Operation cancelled.", false, "task"});
    if (expired()) return Status::failure({ErrorCode::Timeout, "Operation timed out.", true, "task"});
    return success();
}
TaskManager::TaskManager(QObject *parent) : QObject(parent) {
    pool_.setMaxThreadCount(4);
    qRegisterMetaType<TaskResult>();
}
TaskManager::~TaskManager() { shutdown(); }
Status TaskManager::setConcurrency(int limit) {
    if (QThread::currentThread() != thread() || limit < 1 || limit > 16)
        return Status::failure({ErrorCode::InvalidInput, "Invalid task concurrency.", false, "task"});
    pool_.setMaxThreadCount(limit);
    return success();
}
Result<quint64> TaskManager::submit(Work work, int timeoutMs) {
    if (QThread::currentThread() != thread() || !work || timeoutMs < 1 || timeoutMs > 120000)
        return Result<quint64>::failure({ErrorCode::InvalidInput, "Invalid task submission.", false, "task"});
    if (!accepting_ || tasks_.size() >= 64)
        return Result<quint64>::failure({ErrorCode::Conflict, "Task queue is full or shutting down.", true, "task"});
    const auto id = nextId_++;
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    tasks_.insert(id, cancelled);
    const TaskContext context(cancelled, QDeadlineTimer(timeoutMs));
    pool_.start(QRunnable::create([this, id, context, work = std::move(work)] {
        const auto run = [&]() -> TaskResult {
            auto state = context.check();
            if (!state) return TaskResult::failure(state.error());
            try {
                auto result = work(context);
                state = context.check();
                return state ? result : TaskResult::failure(state.error());
            } catch (...) {
                return TaskResult::failure({ErrorCode::Internal, "Background operation failed.", false, "task"});
            }
        };
        auto result = run();
        QMetaObject::invokeMethod(this, [this, id, result = std::move(result)] {
            if (!tasks_.remove(id)) return;
            emit pendingChanged(pendingCount());
            emit finished(id, result);
        }, Qt::QueuedConnection);
    }));
    emit pendingChanged(pendingCount());
    return Result<quint64>::success(id);
}
void TaskManager::cancel(quint64 id) {
    if (QThread::currentThread() != thread()) return;
    const auto it = tasks_.constFind(id);
    if (it != tasks_.cend()) it.value()->store(true);
}
void TaskManager::shutdown() {
    if (!accepting_) return;
    accepting_ = false;
    for (const auto &token : std::as_const(tasks_)) token->store(true);
    pool_.waitForDone();
    tasks_.clear();
    emit pendingChanged(0);
}
}
