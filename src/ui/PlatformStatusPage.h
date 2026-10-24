#pragma once
#include "platform/base/PlatformCoordinator.h"
#include "platform/base/SystemNetworkEvents.h"
#include <QWidget>
#include <QTimer>

class QLabel;
class QPushButton;
class QTableWidget;
namespace netraxis {
class StatusSurface;
class PlatformStatusPage final : public QWidget {
    Q_OBJECT
public:
    PlatformStatusPage(TaskManager &tasks,int timeoutMs,QWidget *parent=nullptr,std::shared_ptr<INetworkPlatform> platform={});
    ~PlatformStatusPage() override;
    void setTimeout(int timeoutMs);
private:
    void refresh();
    void display(const PlatformSnapshot &snapshot);
    void capabilities(CapabilityReport report);
    void freshness();
    std::shared_ptr<INetworkPlatform> platform_;
    PlatformCoordinator *coordinator_ = nullptr;
    SystemNetworkEvents events_;
    QTimer debounce_;
    QTimer freshnessTimer_;
    std::optional<PlatformSnapshot> snapshot_;
    QLabel *provider_;
    QLabel *permissions_;
    QLabel *freshness_;
    StatusSurface *status_;
    QPushButton *refresh_;
    QPushButton *cancel_;
    QTableWidget *capabilities_;
    QTableWidget *interfaces_;
    int timeoutMs_;
    bool refreshing_ = false;
    bool refreshPending_ = false;
    bool refreshFailed_ = false;
};
}
