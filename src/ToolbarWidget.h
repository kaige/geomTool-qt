#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>

class MainWindow;
class QEvent;
class QPaintEvent;

// QToolButton whose hover/active look works on WebAssembly. Two Qt 6.8.x WASM
// (single-thread) problems defeat the default :hover stylesheet styling, fixed
// in ToolbarWidget.cpp:
//   1. The QSS :hover/:checked pseudo-states never repaint on WASM — paintEvent
//      draws the look manually from underMouse()/isChecked().
//   2. Raster widget repaints aren't flushed to the canvas on hover events —
//      event() requests a full top-level window update to force the present.
// Harmless on desktop, which keeps using the stylesheet.
class HoverToolButton : public QToolButton {
public:
    explicit HoverToolButton(QWidget* parent = nullptr) : QToolButton(parent) {
        setAttribute(Qt::WA_Hover);
        setMouseTracking(true);
    }
protected:
    bool event(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
};

class ToolbarWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolbarWidget(MainWindow* mw, QWidget* parent = nullptr);
    void refresh();

private:
    MainWindow* mainWindow;
    QTabWidget* tabs;
    QWidget* createTab;
    QWidget* manageTab;

    // Create tab buttons
    QToolButton* btnSphere;
    QToolButton* btnCube;
    QToolButton* btnCylinder;
    QToolButton* btnCone;
    QToolButton* btnTorus;
    QToolButton* btnLineSegment;
    QToolButton* btnCircularArc;

    // Manage tab buttons
    QToolButton* btnDeleteSelected;
    QToolButton* btnClearAll;

    // Language selector (globe + compact text, popup menu)
    QToolButton* langButton;

    void setupCreateTab();
    void setupManageTab();
    void setupLanguageSelector();
    void retranslateUi();
    void updateLangButtonText();

    QToolButton* createToolButton(const QString& text, const QString& iconName, const QString& iconColor);
    QIcon makeSvgIcon(const QString& iconName, const QString& color);
    void updateButtonStates();
};
