#include "MainWindow.h"
#include "CanvasWidget.h"
#include "ToolbarWidget.h"
#include "ShapeListWidget.h"
#include "StatusBarWidget.h"
#include "GeometryStore.h"
#include "I18n.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    auto& i18n = I18n::instance();
    setWindowTitle(QString("%1 %2")
        .arg(QString::fromStdString(i18n.t("appName")))
        .arg(QString::fromStdString(i18n.t("version"))));
    resize(1200, 800);
}

void MainWindow::setupUI() {
    auto* centralWidget = new QWidget;
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar at top
    toolbar = new ToolbarWidget(this);
    mainLayout->addWidget(toolbar);

    // Middle: sidebar + canvas
    auto* splitter = new QSplitter(Qt::Horizontal);

    // Shape list sidebar
    shapeList = new ShapeListWidget(this);
    shapeList->setMinimumWidth(250);
    shapeList->setMaximumWidth(400);
    splitter->addWidget(shapeList);

    // Canvas
    canvas = new CanvasWidget;
    canvas->setMinimumSize(400, 300);
    splitter->addWidget(canvas);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({300, 900});

    mainLayout->addWidget(splitter, 1);

    // Status bar at bottom
    statusBarWidget = new StatusBarWidget(this);
    mainLayout->addWidget(statusBarWidget);

    setCentralWidget(centralWidget);

    // Connect store change to UI updates
    g_store.onChange = [this]() {
        canvas->update();
        shapeList->refresh();
        statusBarWidget->refresh();
        toolbar->refresh();
    };
}
