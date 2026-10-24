#pragma once
#include <QWidget>
#include <QTimer>
#include "core/Settings.h"
#include "core/TaskManager.h"
class QPushButton;
namespace netraxis {
class StatusSurface;
class StorageControls final : public QWidget {
    Q_OBJECT
public:
    StorageControls(TaskManager &tasks, RuntimeSettings &settings, QWidget *parent = nullptr);
    void setDatabasePath(const QString &path);
public slots:
    void cleanNow();
signals:
    void busyChanged(bool busy);
    void dataChanged();
    void dataDeleted();
private:
    void start(bool clear);
    void setBusy(bool busy);
    TaskManager &tasks_;
    RuntimeSettings &settings_;
    QString path_;
    quint64 task_ = 0;
    bool clearing_ = false;
    QPushButton *clean_;
    QPushButton *clear_;
    QPushButton *cancel_;
    StatusSurface *state_;
    QTimer timer_;
};
}
