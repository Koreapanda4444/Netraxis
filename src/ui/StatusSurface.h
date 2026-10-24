#pragma once
#include "core/Availability.h"
#include <QFrame>
class QLabel;
class QPushButton;
namespace netraxis {
class StatusSurface final : public QFrame {
    Q_OBJECT
public:
    explicit StatusSurface(const FeatureState &state, QWidget *parent = nullptr);
    void setState(const FeatureState &state);
    void setError(const Error &error);
signals:
    void retryRequested();
private:
    QLabel *title_;
    QLabel *description_;
    QPushButton *retry_;
};
}
