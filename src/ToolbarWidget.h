#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>

class MainWindow;

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
