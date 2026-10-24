#pragma once
#include <QMainWindow>
#include "core/Settings.h"
#include "core/TaskManager.h"

class QLabel;
class QStackedWidget;
class QSpinBox;
class QComboBox;
namespace netraxis {
class StatusSurface;
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(TaskManager &tasks, SettingsStore &store, RuntimeSettings &settings);
    void showError(const Error &error);
signals:
    void settingsChanged();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    QWidget *dashboard();
    QWidget *diagnostics();
    QWidget *settingsPage();
    TaskManager &tasks_;
    SettingsStore &store_;
    RuntimeSettings &settings_;
    QLabel *pending_ = nullptr;
    StatusSurface *diagnosticStatus_ = nullptr;
    QLabel *diagnosticResult_ = nullptr;
    quint64 diagnosticTask_ = 0;
};
}
