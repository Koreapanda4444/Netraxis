#include "MainWindow.h"
#include <QLabel>
#include <QStatusBar>

namespace netraxis {
MainWindow::MainWindow() {
    setWindowTitle("Netraxis");
    resize(1100, 720);
    auto *label = new QLabel(tr("Netraxis — Network & Internet Management"), this);
    label->setAlignment(Qt::AlignCenter);
    setCentralWidget(label);
    statusBar()->showMessage(tr("Ready"));
}
}
