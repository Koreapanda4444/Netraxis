#include "WorkspacePage.h"
#include "StatusSurface.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace netraxis {
WorkspacePage::WorkspacePage(QWidget *parent):QWidget(parent){
    auto *layout=new QVBoxLayout(this);layout->addWidget(new QLabel(tr("<h2>Saved workspaces</h2>")));
    auto *explanation=new QLabel(tr("Profiles are stored locally. Automatic network matching is unavailable in this build."));explanation->setWordWrap(true);layout->addWidget(explanation);
    table_=new QTableWidget(0,2,this);table_->setHorizontalHeaderLabels({tr("Name"),tr("Created")});table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);table_->setSelectionBehavior(QAbstractItemView::SelectRows);table_->setSelectionMode(QAbstractItemView::SingleSelection);table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_);auto *buttons=new QHBoxLayout;
    add_=new QPushButton(tr("Add workspace"));rename_=new QPushButton(tr("Rename"));remove_=new QPushButton(tr("Delete workspace"));
    buttons->addWidget(add_);buttons->addWidget(rename_);buttons->addWidget(remove_);layout->addLayout(buttons);
    state_=new StatusSurface({Availability::Unknown,Verification::Unverified,tr("Local storage has not been connected.")},this);layout->addWidget(state_);
    connect(add_,&QPushButton::clicked,this,[this]{edit(true);});connect(rename_,&QPushButton::clicked,this,[this]{edit(false);});connect(remove_,&QPushButton::clicked,this,&WorkspacePage::removeSelected);
    connect(state_,&StatusSurface::retryRequested,this,&WorkspacePage::reload);
    connect(table_,&QTableWidget::itemSelectionChanged,this,[this]{const bool selected=repository_&&table_->currentRow()>=0;rename_->setEnabled(selected);remove_->setEnabled(selected);});
    setRepository(nullptr);
}
void WorkspacePage::setRepository(CoreRepository *repository){repository_=repository;add_->setEnabled(repository);reload();}
void WorkspacePage::reload(){
    values_.clear();table_->setRowCount(0);rename_->setEnabled(false);remove_->setEnabled(false);
    if(!repository_){state_->setState({Availability::Unknown,Verification::Unverified,tr("Local storage is unavailable.")});return;}
    auto result=repository_->workspaces();if(!result){state_->setError(result.error());return;}
    values_=result.value();table_->setRowCount(int(values_.size()));
    for(int i=0;i<int(values_.size());++i){table_->setItem(i,0,new QTableWidgetItem(values_[i].name));table_->setItem(i,1,new QTableWidgetItem(values_[i].createdAt.toLocalTime().toString(Qt::ISODate)));}
    state_->setState({Availability::Supported,Verification::Healthy,tr("%1 saved workspace(s). This describes local storage, not network connectivity.").arg(values_.size())});
}
void WorkspacePage::edit(bool create){
    if(!repository_)return;
    const auto row=table_->currentRow();if(!create&&(row<0||row>=values_.size()))return;
    bool accepted=false;const auto name=QInputDialog::getText(this,create?tr("Add workspace"):tr("Rename workspace"),tr("Name"),QLineEdit::Normal,create?QString():values_[row].name,&accepted).trimmed();
    if(!accepted)return;
    auto value=create?NetworkWorkspace{QUuid::createUuid(),name,QDateTime::currentDateTimeUtc()}:values_[row];value.name=name;
    const auto result=repository_->save(value);if(!result){state_->setError(result.error());return;}reload();
}
void WorkspacePage::removeSelected(){
    if(!repository_)return;
    const auto row=table_->currentRow();if(row<0||row>=values_.size())return;
    QMessageBox confirm(QMessageBox::Question,tr("Delete workspace"),tr("Delete this workspace, its registered devices and their observation history?"),QMessageBox::Yes|QMessageBox::No,this);confirm.setDefaultButton(QMessageBox::No);
    if(confirm.exec()!=QMessageBox::Yes)return;
    const auto result=repository_->removeWorkspace(values_[row].id);if(!result){state_->setError(result.error());return;}reload();
}
}
