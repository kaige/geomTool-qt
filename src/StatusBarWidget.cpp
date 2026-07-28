#include "StatusBarWidget.h"
#include "MainWindow.h"
#include "GeometryStore.h"
#include "I18n.h"
#include <QHBoxLayout>

StatusBarWidget::StatusBarWidget(MainWindow* mw, QWidget* parent)
    : QWidget(parent), mainWindow(mw)
{
    setFixedHeight(36);
    setStyleSheet(
        "StatusBarWidget { background-color: #f3f2f1; border-top: 1px solid #e1dfdd; }"
        "QLabel { color: #666; font-size: 12px; }"
    );

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(24);

    leftLabel = new QLabel;
    centerLabel = new QLabel;
    rightLabel = new QLabel;

    layout->addWidget(leftLabel);
    layout->addStretch();
    layout->addWidget(centerLabel);
    layout->addStretch();
    layout->addWidget(rightLabel);

    refresh();
}

void StatusBarWidget::refresh() {
    auto& i18n = I18n::instance();
    leftLabel->setText(QString("%1 %2")
        .arg(QString::fromStdString(i18n.t("appName")))
        .arg(QString::fromStdString(i18n.t("version"))));

    if (!g_store.selectedShapeId.empty()) {
        BaseShape* sel = g_store.getSelectedShape();
        if (sel) {
            std::string typeKey = shapeTypeKey(sel->type);
            centerLabel->setText(QString("%1: %2")
                .arg(QString::fromStdString(i18n.t("selectedShape")))
                .arg(QString::fromStdString(i18n.t(typeKey))));
            centerLabel->setStyleSheet("color: #0078d4; font-weight: 600; font-size: 12px;");
        }
    } else {
        centerLabel->setText("");
    }

    rightLabel->setText(QString("%1: %2")
        .arg(QString::fromStdString(i18n.t("totalShapes")))
        .arg(g_store.shapeCount()));
}
