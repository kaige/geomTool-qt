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
#include <QComboBox>
#include <QEvent>
#include <QWindow>       // [hover/WASM] windowHandle()->requestUpdate()
#include <QPaintEvent>

#if __has_include(<QSvgRenderer>)
#include <QSvgRenderer>
#define HAVE_SVG 1
#else
#define HAVE_SVG 0
#endif

bool HoverToolButton::event(QEvent* e) {
    const bool res = QToolButton::event(e);
    switch (e->type()) {
    case QEvent::Enter:
    case QEvent::Leave:
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
        // Repaint on hover/click transitions; paintEvent draws the hover/active
        // look manually (the QSS :hover/:checked states don't repaint on WASM).
        repaint();
#ifdef Q_OS_WASM
        // On WASM, plain toolbar repaints aren't flushed to the canvas on hover
        // events (only mouse-button/key events trigger that flush), so request a
        // full top-level window update: Qt then composites the dirty toolbar into
        // the backing store and flushes the window to the canvas. Nudge the canvas
        // too so its GL content stays composited in the same frame.
        if (QWidget* tl = window())
            if (QWindow* wh = tl->windowHandle()) wh->requestUpdate();
        if (g_canvas) g_canvas->update();
#endif
        break;
    default:
        break;
    }
    return res;
}

void HoverToolButton::paintEvent(QPaintEvent* e) {
#ifdef Q_OS_WASM
    // The QSS :hover/:checked pseudo-states don't repaint on WASM, so draw the
    // state background+border here from underMouse()/isChecked(), then let the
    // base class paint icon+text on top. Desktop keeps using the stylesheet.
    QColor bg, bd;
    if (isChecked()) {
        bg = QColor(QStringLiteral("#d4e9f7"));
        bd = QColor(QStringLiteral("#b0d8f0"));
    } else if (underMouse() && isEnabled()) {
        bg = QColor(QStringLiteral("#e8e8e8"));
        bd = QColor(QStringLiteral("#d0d0d0"));
    }
    if (bg.isValid()) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF rr = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(bd, 1));
        p.setBrush(bg);
        p.drawRoundedRect(rr, 4, 4);
    }
#endif
    QToolButton::paintEvent(e);
}

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
    cornerLayout->addWidget(langCombo);
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
    auto* btn = new HoverToolButton;
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
    // Language dropdown built from QComboBox (Qt's built-in combo box).
    //
    // Why not QToolButton + QMenu: QToolButton::InstantPopup shows the menu via
    // QMenu::exec(), a blocking nested event loop. On Qt 6.8.3 WASM built without
    // asyncify (aqt prebuilt), that nested loop deadlocks the page — clicking the
    // selector froze the whole app (see memory: qt-wasm-modal-exec-deadlock).
    // QComboBox's popup is non-modal (container show()/hide(), event-driven
    // selection, no exec), so it works on WASM. Desktop behaviour is unchanged.
    // Matches geomTool (React) LanguageSelector: globe icon + label, no arrow.
    langCombo = new QComboBox;
    langCombo->setCursor(Qt::PointingHandCursor);
    langCombo->addItem(QStringLiteral("中文"),   static_cast<int>(Language::ZH));
    langCombo->addItem(QStringLiteral("English"), static_cast<int>(Language::EN));

    // Globe icon (drawn, so it works cross-platform without an icon theme).
    QPixmap globePix(20, 20);
    globePix.fill(Qt::transparent);
    {
        QPainter gp(&globePix);
        gp.setRenderHint(QPainter::Antialiasing);
        gp.setPen(QPen(QColor("#888"), 1.5));
        gp.setBrush(Qt::NoBrush);
        gp.drawEllipse(4, 3, 12, 14);        // outer circle
        gp.drawLine(4, 10, 16, 10);          // equator
        gp.drawEllipse(7, 3, 6, 14);         // meridian ellipse
        gp.end();
    }
    const QIcon globeIcon(globePix);
    langCombo->setItemIcon(0, globeIcon);
    langCombo->setItemIcon(1, globeIcon);
    langCombo->setIconSize(QSize(16, 16));

    langCombo->setCurrentIndex(0);  // Chinese by default

    langCombo->setStyleSheet(
        "QComboBox {"
        "  border: 1px solid transparent;"
        "  border-radius: 4px;"
        "  padding: 2px 4px;"
        "  font-size: 13px;"
        "  background: transparent;"
        "}"
        "QComboBox:hover { background-color: #f3f2f1; }"
        "QComboBox:on    { background-color: #edebe9; }"
        // Hide the drop-down arrow to keep the compact globe + label look
        // (mirrors geomTool's menu-indicator: image:none). Click still opens it.
        "QComboBox::drop-down { border: none; width: 0; }"
        "QComboBox QAbstractItemView {"
        "  font-size: 12px;"
        "  min-width: 90px;"
        "  border: 1px solid #d0d0d0;"
        "  background: #ffffff;"
        "  selection-background-color: #e8e8e8;"
        "  selection-color: #333;"
        "  outline: none;"
        "}"
    );

    // activated() fires only on user interaction — never on setCurrentIndex —
    // so retranslateUi() (which calls setCurrentIndex) cannot recurse into here.
    QObject::connect(langCombo, QOverload<int>::of(&QComboBox::activated),
        [this](int index) {
            Language lang = static_cast<Language>(langCombo->itemData(index).toInt());
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

    // Keep the language combo's selected index in sync with the current language.
    // setCurrentIndex does NOT emit activated(), so this cannot recurse into the
    // combo's handler. Item texts ("中文"/"English") are fixed in each language's
    // own script, so they never need retranslating.
    langCombo->setCurrentIndex(i18n.getLanguage() == Language::ZH ? 0 : 1);
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
