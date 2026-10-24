#pragma once
#include <QWidget>
#include "storage/CoreRepository.h"
class QTableWidget;
class QPushButton;
namespace netraxis {
class StatusSurface;
class WorkspacePage final : public QWidget {
    Q_OBJECT
public:
    explicit WorkspacePage(QWidget *parent = nullptr);
    void setRepository(CoreRepository *repository);
public slots:
    void reload();
private:
    void edit(bool create);
    void removeSelected();
    CoreRepository *repository_ = nullptr;
    QVector<NetworkWorkspace> values_;
    QTableWidget *table_;
    QPushButton *add_;
    QPushButton *rename_;
    QPushButton *remove_;
    StatusSurface *state_;
};
}
