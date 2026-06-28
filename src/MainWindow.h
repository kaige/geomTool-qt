#pragma once
#include <QMainWindow>
#include <QTabWidget>

class CanvasWidget;
class ShapeListWidget;
class StatusBarWidget;
class ToolbarWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    CanvasWidget* canvas;
    ShapeListWidget* shapeList;
    StatusBarWidget* statusBarWidget;
    ToolbarWidget* toolbar;

    void setupUI();
    void retranslateUi();
};
