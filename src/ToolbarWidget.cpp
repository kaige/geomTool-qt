#include "ToolbarWidget.h"
#include "MainWindow.h"
#include "CanvasWidget.h"
#include "GeometryStore.h"
#include "I18n.h"
#include "tools/ToolManager.h"
#include <QVBoxLayout>
#include <QStyle>
#include <QPainter>
#include <QFile>
#include <QMenu>

#if __has_include(<QSvgRenderer>)
#include <QSvgRenderer>
#define HAVE_SVG 1
#else
#define HAVE_SVG 0
#endif

ToolbarWidget::ToolbarWidget(MainWindow* mw, QWidget* parent)
    : QWidget(parent), mainWindow(mw)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Tab bar — Chili3D style: clean, minimal, top tabs
    tabs = new QTabWidget;
    tabs->setDocumentMode(true);
    tabs->setStyleSheet(
        "QTabBar::tab { padding: 6px 16px; font-size: 13px; color: #666; border: none; }"
        "QTabBar::tab:selected { color: #333; font-weight: 600; border-bottom: 3px solid #0E62D7; }"
        "QTabBar::tab:hover { color: #333; }"
        "QTabWidget::pane { border: none; border-top: 1px solid #dcdcdc; "
        "  background: #ffffff; }"
    );

    createTab = new QWidget;
    createTab->setStyleSheet("background-color: #ffffff;");
    manageTab = new QWidget;
    manageTab->setStyleSheet("background-color: #ffffff;");

    setupCreateTab();
    setupManageTab();

    tabs->addTab(createTab, "");
    tabs->addTab(manageTab, "");

    mainLayout->addWidget(tabs);

    // Language selector in corner
    setupLanguageSelector();
    auto* cornerWidget = new QWidget;
    auto* cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(4, 0, 12, 0);
    cornerLayout->setSpacing(0);
    cornerLayout->addWidget(langButton);
    tabs->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    retranslateUi();
}

QIcon ToolbarWidget::makeSvgIcon(const QString& iconName, const QString& iconColor) {
#if HAVE_SVG
    // Load the SVG from Qt resources, substitute the requested stroke color
    // (the source icons use stroke="currentColor"), and rasterize via QSvgRenderer.
    QFile file(":/icons/" + iconName + ".svg");
    if (!file.open(QIODevice::ReadOnly))
        return QIcon();

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("currentColor"), iconColor);

    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid())
        return QIcon();

    // Render at 3x for crisp HiDPI output
    const qreal dpr = 3.0;
    const int logical = 48;         // larger target → less clipping risk
    const int px = int(logical * dpr);

    QPixmap pixmap(px, px);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // CRITICAL: explicitly render into the full target rectangle.
    // Without bounds, QSvgRenderer uses the SVG's default size (24×24)
    // positioned at (0,0), causing partial rendering / clipping.
    renderer.render(&painter, QRectF(0, 0, logical, logical));
    painter.end();

    return QIcon(pixmap);
#else
    // Fallback: draw a colored circle when SVG support is unavailable (e.g. WASM without Qt::Svg)
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(iconColor));
    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawEllipse(3, 3, 26, 26);
    p.end();
    return QIcon(pixmap);
#endif
}

QToolButton* ToolbarWidget::createToolButton(const QString& text, const QString& iconName, const QString& iconColor) {
    auto* btn = new QToolButton;
    btn->setText(text);
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    // Wider to fit English labels like "Circular Arc", "Delete Selected"
    btn->setFixedSize(80, 72);
    btn->setCheckable(true);

    btn->setIcon(makeSvgIcon(iconName, iconColor));
    btn->setIconSize(QSize(40, 40));

    // Chili3D-style ribbon button: monochrome icon, clean hover/active states
    btn->setStyleSheet(
        "QToolButton {"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 4px;"
        "  font-size: 12px;"
        "  color: #333;"
        "  background: transparent;"
        "} "
        "QToolButton:hover {"
        "  border: 1px solid #d0d0d0;"
        "  background-color: #e8e8e8;"
        "} "
        "QToolButton:checked {"
        "  border: 1px solid #b0d8f0;"
        "  background-color: #d4e9f7;"
        "} "
        "QToolButton:disabled {"
        "  color: #bbb;"
        "} "
    );

    return btn;
}

void ToolbarWidget::setupCreateTab() {
    auto* layout = new QHBoxLayout(createTab);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(4);
    layout->addStretch();
    const QString iconColor = "#555555";
    btnSphere = createToolButton("", "sphere", iconColor);
    btnCube = createToolButton("", "cube", iconColor);
    btnCylinder = createToolButton("", "cylinder", iconColor);
    btnCone = createToolButton("", "cone", iconColor);
    btnTorus = createToolButton("", "torus", iconColor);
    btnLineSegment = createToolButton("", "line", iconColor);
    btnCircularArc = createToolButton("", "arc", iconColor);

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
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(4);
    layout->addStretch();

    btnDeleteSelected = createToolButton("", "delete", "#555555");
    btnClearAll = createToolButton("", "clear", "#555555");

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
    // Compact button: globe icon + "中"/"En", click opens popup menu.
    // Matches geomTool (React) LanguageSelector: Icon(Globe) + short label, no arrow.
    langButton = new QToolButton;
    langButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    langButton->setPopupMode(QToolButton::InstantPopup);

    // Globe icon: use emoji as icon text prefix (works cross-platform without icon theme)
    QPixmap globePix(20, 20);
    globePix.fill(Qt::transparent);
    {
        QPainter gp(&globePix);
        gp.setRenderHint(QPainter::Antialiasing);
        gp.setPen(QPen(QColor("#888"), 1.5));
        gp.setBrush(Qt::NoBrush);
        gp.drawEllipse(4, 3, 12, 14);        // outer circle
        gp.drawLine(4, 10, 16, 10);           // equator
        gp.drawEllipse(7, 3, 6, 14);          // meridian ellipse
        gp.end();
    }
    langButton->setIcon(QIcon(globePix));
    langButton->setIconSize(QSize(17, 17));
    langButton->setStyleSheet(
        "QToolButton {"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 2px 6px;"
        "  font-size: 14px;"
        "  background: transparent;"
        "}"
        "QToolButton::menu-indicator { image: none; }"  // hide arrow, like geomTool
        "QToolButton:hover { background-color: #f3f2f1; }"
        "QToolButton:pressed { background-color: #edebe9; }"
        "QToolButton::menu {"
        "  font-size: 12px;"
        "  min-width: 80px;"
        "}"
    );

    auto* menu = new QMenu(langButton);
    menu->setStyleSheet(
        "QMenu { font-size: 12px; min-width: 80px; padding: 4px; }"
        "QMenu::item { padding: 6px 16px; }"
        "QMenu::item:selected { background-color: #e8e8e8; color: #333; }"
    );
    auto* actZh = menu->addAction(QString::fromStdString(I18n::instance().t("chinese")));
    auto* actEn = menu->addAction(QString::fromStdString(I18n::instance().t("english")));
    actZh->setData((int)Language::ZH);
    actEn->setData((int)Language::EN);
    actZh->setCheckable(true);
    actEn->setCheckable(true);

    langButton->setMenu(menu);

    QObject::connect(menu, &QMenu::triggered, [this](QAction* action) {
        Language lang = (Language)action->data().toInt();
        I18n::instance().setLanguage(lang);
        retranslateUi();   // also updates menu check marks + button text
    });

    // Set initial check
    actZh->setChecked(true);

    updateLangButtonText();
}

void ToolbarWidget::updateLangButtonText() {
    // Show "中" for Chinese, "En" for English — exactly like geomTool
    auto& i18n = I18n::instance();
    langButton->setText(i18n.getLanguage() == Language::ZH ? QStringLiteral("中") : QStringLiteral("En"));
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

    // Update language button label and menu check marks
    updateLangButtonText();
    auto* menu = langButton->menu();
    if (menu) {
        for (auto* act : menu->actions()) {
            Language lang = (Language)act->data().toInt();
            act->setChecked(i18n.getLanguage() == lang);
        }
    }
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
