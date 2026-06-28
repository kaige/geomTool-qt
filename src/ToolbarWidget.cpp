#include "ToolbarWidget.h"
#include "MainWindow.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include "I18n.h"
#include "tools/ToolManager.h"
#include <QVBoxLayout>
#include <QStyle>
#include <QPainter>

ToolbarWidget::ToolbarWidget(MainWindow* mw, QWidget* parent)
    : QWidget(parent), mainWindow(mw)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Tab bar
    tabs = new QTabWidget;
    tabs->setDocumentMode(true);

    createTab = new QWidget;
    manageTab = new QWidget;

    setupCreateTab();
    setupManageTab();

    tabs->addTab(createTab, "");
    tabs->addTab(manageTab, "");

    mainLayout->addWidget(tabs);

    // Language selector in corner
    setupLanguageSelector();
    auto* cornerWidget = new QWidget;
    auto* cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(4, 0, 8, 0);
    cornerLayout->addWidget(languageCombo);
    tabs->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    retranslateUi();
}

QToolButton* ToolbarWidget::createToolButton(const QString& text, const QString& iconColor) {
    auto* btn = new QToolButton;
    btn->setText(text);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setFixedSize(70, 60);
    btn->setCheckable(true);

    // Create a colored circle as icon
    QPixmap pix(36, 36);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(iconColor));
    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawEllipse(4, 4, 28, 28);
    p.end();
    btn->setIcon(QIcon(pix));
    btn->setIconSize(QSize(36, 36));

    return btn;
}

void ToolbarWidget::setupCreateTab() {
    auto* layout = new QHBoxLayout(createTab);
    layout->setContentsMargins(20, 8, 20, 8);
    layout->setSpacing(4);
    layout->addStretch();

    btnSphere = createToolButton("", "#4FC3F7");
    btnCube = createToolButton("", "#81C784");
    btnCylinder = createToolButton("", "#FFB74D");
    btnCone = createToolButton("", "#A1887F");
    btnTorus = createToolButton("", "#BA68C8");
    btnLineSegment = createToolButton("", "#0078d4");
    btnCircularArc = createToolButton("", "#E8804D");

    layout->addWidget(btnSphere);
    layout->addWidget(btnCube);
    layout->addWidget(btnCylinder);
    layout->addWidget(btnCone);
    layout->addWidget(btnTorus);
    layout->addWidget(btnLineSegment);
    layout->addWidget(btnCircularArc);
    layout->addStretch();

    auto* group = new QButtonGroup(this);
    group->setExclusive(false);
    group->addButton(btnSphere);
    group->addButton(btnCube);
    group->addButton(btnCylinder);
    group->addButton(btnCone);
    group->addButton(btnTorus);
    group->addButton(btnLineSegment);
    group->addButton(btnCircularArc);

    connect(btnSphere, &QToolButton::clicked, []() {
        g_canvas->toolManager->activateTool(ToolType::CreateSphere);
    });
    connect(btnCube, &QToolButton::clicked, []() {
        g_canvas->toolManager->activateTool(ToolType::CreateCube);
    });
    connect(btnCylinder, &QToolButton::clicked, []() {
        g_canvas->toolManager->activateTool(ToolType::CreateCylinder);
    });
    connect(btnCone, &QToolButton::clicked, []() {
        g_canvas->toolManager->activateTool(ToolType::CreateCone);
    });
    connect(btnTorus, &QToolButton::clicked, []() {
        g_canvas->toolManager->activateTool(ToolType::CreateTorus);
    });
    connect(btnLineSegment, &QToolButton::clicked, []() {
        g_canvas->toolManager->activateTool(ToolType::CreateLineSegment);
    });
    connect(btnCircularArc, &QToolButton::clicked, []() {
        g_canvas->toolManager->activateTool(ToolType::CreateCircularArc);
    });
}

void ToolbarWidget::setupManageTab() {
    auto* layout = new QHBoxLayout(manageTab);
    layout->setContentsMargins(20, 8, 20, 8);
    layout->setSpacing(4);
    layout->addStretch();

    btnDeleteSelected = createToolButton("", "#EF5350");
    btnClearAll = createToolButton("", "#FF7043");

    layout->addWidget(btnDeleteSelected);
    layout->addWidget(btnClearAll);
    layout->addStretch();

    connect(btnDeleteSelected, &QToolButton::clicked, []() {
        if (!g_store.selectedShapeId.empty())
            g_store.removeShape(g_store.selectedShapeId);
    });
    connect(btnClearAll, &QToolButton::clicked, []() {
        g_store.clearAll();
    });
}

void ToolbarWidget::setupLanguageSelector() {
    languageCombo = new QComboBox;
    languageCombo->addItem("中文", (int)Language::ZH);
    languageCombo->addItem("English", (int)Language::EN);
    languageCombo->setFixedWidth(100);

    connect(languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
        Language lang = (Language)languageCombo->currentData().toInt();
        I18n::instance().setLanguage(lang);
        retranslateUi();
    });
}

void ToolbarWidget::retranslateUi() {
    auto& i18n = I18n::instance();
    tabs->setTabText(0, QString::fromStdString(i18n.t("create")));
    tabs->setTabText(1, QString::fromStdString(i18n.t("manage")));

    btnSphere->setText(QString::fromStdString(i18n.t("sphere")));
    btnCube->setText(QString::fromStdString(i18n.t("cube")));
    btnCylinder->setText(QString::fromStdString(i18n.t("cylinder")));
    btnCone->setText(QString::fromStdString(i18n.t("cone")));
    btnTorus->setText(QString::fromStdString(i18n.t("torus")));
    btnLineSegment->setText(QString::fromStdString(i18n.t("lineSegment")));
    btnCircularArc->setText(QString::fromStdString(i18n.t("circularArc")));

    btnDeleteSelected->setText(QString::fromStdString(i18n.t("deleteSelected")));
    btnClearAll->setText(QString::fromStdString(i18n.t("clearAll")));
}

void ToolbarWidget::refresh() {
    updateButtonStates();
}

void ToolbarWidget::updateButtonStates() {
    // Highlight active create tool
    btnSphere->setChecked(g_store.activeToolType == ToolType::CreateSphere);
    btnCube->setChecked(g_store.activeToolType == ToolType::CreateCube);
    btnCylinder->setChecked(g_store.activeToolType == ToolType::CreateCylinder);
    btnCone->setChecked(g_store.activeToolType == ToolType::CreateCone);
    btnTorus->setChecked(g_store.activeToolType == ToolType::CreateTorus);
    btnLineSegment->setChecked(g_store.activeToolType == ToolType::CreateLineSegment);
    btnCircularArc->setChecked(g_store.activeToolType == ToolType::CreateCircularArc);

    // Enable/disable manage buttons
    btnDeleteSelected->setEnabled(!g_store.selectedShapeId.empty());
    btnClearAll->setEnabled(g_store.shapeCount() > 0);
}
