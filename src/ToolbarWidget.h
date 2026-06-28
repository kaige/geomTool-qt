#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QToolButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>

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

    // Language selector
    QComboBox* languageCombo;

    void setupCreateTab();
    void setupManageTab();
    void setupLanguageSelector();
    void retranslateUi();

    QToolButton* createToolButton(const QString& text, const QString& iconColor);
    void updateButtonStates();
};
