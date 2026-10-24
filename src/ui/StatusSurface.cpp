#include "StatusSurface.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QFontMetrics>

namespace netraxis {
StatusSurface::StatusSurface(const FeatureState &state, QWidget *parent) : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Minimum);
    auto *layout = new QVBoxLayout(this);
    title_ = new QLabel(this);description_ = new QLabel(this);retry_ = new QPushButton(tr("Retry"),this);
    title_->setTextFormat(Qt::PlainText);description_->setTextFormat(Qt::PlainText);description_->setWordWrap(true);description_->setAlignment(Qt::AlignLeft|Qt::AlignTop);
    layout->addWidget(title_);layout->addWidget(description_);layout->addWidget(retry_);
    connect(retry_,&QPushButton::clicked,this,&StatusSurface::retryRequested);setState(state);
}
void StatusSurface::setState(const FeatureState &state) {
    title_->setText(availabilityLabel(state.availability));
    auto text=state.reason;
    if(state.verification==Verification::Unverified)text+=tr("\nVerification: unverified.");
    else if(state.verification==Verification::Skipped)text+=tr("\nVerification: skipped.");
    else if(state.verification==Verification::Failed)text+=tr("\nVerification: failed.");
    else if(state.verification==Verification::Degraded)text+=tr("\nVerification: degraded.");
    else text+=tr("\nVerification: healthy.");
    description_->setText(text);retry_->hide();
    setAccessibleName(title_->text());setAccessibleDescription(text);updateDescriptionHeight();
}
void StatusSurface::setError(const Error &error) {
    title_->setText(tr("Operation failed"));description_->setText(error.message);
    retry_->setVisible(error.retryable);setAccessibleName(title_->text());setAccessibleDescription(error.message);updateDescriptionHeight();
}
void StatusSurface::updateDescriptionHeight() {
    const auto margins=layout()->contentsMargins();
    const int width=qMax(100,contentsRect().width()-margins.left()-margins.right());
    const auto bounds=description_->fontMetrics().boundingRect(QRect(0,0,width,100000),Qt::TextWordWrap,description_->text());
    description_->setMinimumHeight(bounds.height()+2);
}
void StatusSurface::resizeEvent(QResizeEvent *event) {
    QFrame::resizeEvent(event);updateDescriptionHeight();
}

}
