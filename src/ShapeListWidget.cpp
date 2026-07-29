#include "ShapeListWidget.h"
#include "MainWindow.h"
#include "GeometryStore.h"
#include "I18n.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QSignalBlocker>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

ShapeListWidget::ShapeListWidget(MainWindow* mw, QWidget* parent)
    : QWidget(parent), mainWindow(mw)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    headerLabel = new QLabel;
    headerLabel->setStyleSheet(
        "padding: 12px 16px; font-size: 14px; font-weight: 600; "
        "background-color: #faf9f8; border-bottom: 1px solid #e1dfdd;"
    );
    layout->addWidget(headerLabel);

    // Table
    table = new QTableWidget;
    table->setColumnCount(3);   // Type, ID, Actions (eye + trash)
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    // Row height must accommodate the 30px action buttons + padding
    table->verticalHeader()->setDefaultSectionSize(38);
    table->setStyleSheet(
        "QTableWidget { border: none; background: white; }"
        "QTableWidget::item { padding: 6px 10px; }"
        "QTableWidget::item:selected { background-color: #d4e9f7; color: #333; font-weight: 600; }"
        "QHeaderView::section { padding: 8px 10px; background: #f5f5f5; border: none; "
        "border-bottom: 1px solid #e1dfdd; font-weight: 600; font-size: 13px; }"
    );

    // Column widths: Type stretches, ID fixed, Actions fixed
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);  // Type
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);    // ID
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);    // Actions
    table->setColumnWidth(1, 40);    // ID
    table->setColumnWidth(2, 96);    // Actions: room for two 30px buttons + margins

    layout->addWidget(table);

    // Empty state placeholder
    emptyLabel = new QLabel;
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet(
        "color: #a19f9d; font-size: 14px; padding: 60px;"
    );
    layout->addWidget(emptyLabel);
    emptyLabel->setVisible(false);

    connect(table, &QTableWidget::cellClicked, [this](int row, int col) {
        if (row < 0 || row >= (int)g_store.shapes.size()) return;
        auto& shape = g_store.shapes[row];

        if (col == 0 || col == 1) {
            // Select (clicking type or ID)
            if (g_store.selectedShapeId == shape->id)
                g_store.selectShapeNull();
            else
                g_store.selectShape(shape->id);
            refresh();
        }
        // col == 2 is handled by the embedded QToolButtons
    });

    retranslateUi();
}

void ShapeListWidget::retranslateUi() {
    auto& i18n = I18n::instance();
    QStringList headers;
    headers << QString::fromStdString(i18n.t("type"))
            << QString::fromStdString(i18n.t("id"))
            << QString::fromStdString(i18n.t("actions"));
    table->setHorizontalHeaderLabels(headers);

    emptyLabel->setText(QString::fromStdString(i18n.t("noShapes")));
}

void ShapeListWidget::refresh() {
    auto& i18n = I18n::instance();

    QSignalBlocker blocker(table);

    bool hasShapes = !g_store.shapes.empty();
    table->setVisible(hasShapes);
    emptyLabel->setVisible(!hasShapes);

    table->setRowCount((int)g_store.shapes.size());

    int row = 0;
    for (auto& shape : g_store.shapes) {
        table->setRowHeight(row, 38);  // ensure room for 30px buttons
        // Type
        std::string typeKey = shapeTypeKey(shape->type);
        std::string typeLabel = i18n.t(typeKey);
        if (typeLabel == typeKey) typeLabel = typeKey; // fallback
        auto* typeItem = new QTableWidgetItem(QString::fromStdString(typeLabel));
        table->setItem(row, 0, typeItem);

        // ID
        auto* idItem = new QTableWidgetItem(QString::fromStdString(shape->id));
        idItem->setTextAlignment(Qt::AlignCenter);
        idItem->setToolTip(QString::fromStdString(shape->id));
        table->setItem(row, 1, idItem);

        // Actions cell (col 2): eye toggle button + trash delete button
        {
            auto* actionsWidget = new QWidget;
            auto* hLayout = new QHBoxLayout(actionsWidget);
            hLayout->setContentsMargins(4, 0, 4, 0);
            hLayout->setSpacing(6);

            // --- Draw eye icon as pixmap (reliable, no emoji clipping) ---
            auto makeEyeIcon = [](bool visible) -> QIcon {
                const int sz = 24;
                QPixmap pix(sz, sz);
                pix.fill(Qt::transparent);
                QPainter p(&pix);
                p.setRenderHint(QPainter::Antialiasing);
                QColor c = visible ? QColor("#555555") : QColor("#bbb");
                QPen pen(c, 1.6);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                // Eye outline (almond shape)
                p.drawEllipse(QRectF(2, 7, 20, 10));
                // Pupil
                p.setBrush(c);
                p.drawEllipse(QPointF(12, 12), 2.5, 2.5);
                p.end();
                return QIcon(pix);
            };

            // --- Draw trash icon as pixmap ---
            auto makeTrashIcon = []() -> QIcon {
                const int sz = 24;
                QPixmap pix(sz, sz);
                pix.fill(Qt::transparent);
                QPainter p(&pix);
                p.setRenderHint(QPainter::Antialiasing);
                QColor c("#666666");
                QPen pen(c, 1.6);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                // Lid
                p.drawLine(5, 7, 19, 7);
                p.drawLine(9, 7, 10, 5);
                p.drawLine(10, 5, 14, 5);
                p.drawLine(14, 5, 15, 7);
                // Body
                p.drawLine(7, 7, 8, 19);
                p.drawLine(17, 7, 16, 19);
                p.drawLine(8, 19, 16, 19);
                // Inner lines
                p.drawLine(10, 9, 10, 17);
                p.drawLine(12, 9, 12, 17);
                p.drawLine(14, 9, 14, 17);
                p.end();
                return QIcon(pix);
            };

            // Eye toggle button
            auto* eyeBtn = new QToolButton;
            eyeBtn->setAutoRaise(true);
            eyeBtn->setFixedSize(30, 30);
            eyeBtn->setIcon(makeEyeIcon(shape->visible));
            eyeBtn->setIconSize(QSize(24, 24));
            eyeBtn->setStyleSheet(
                "QToolButton { border: none; border-radius: 4px; }"
                "QToolButton:hover { background-color: #f3f2f1; }"
            );
            eyeBtn->setToolTip(QString::fromStdString(i18n.t(shape->visible ? "hide" : "show")));

            QString shapeId = QString::fromStdString(shape->id);
            connect(eyeBtn, &QToolButton::clicked, [this, shapeId]() {
                for (auto& s : g_store.shapes) {
                    if (s->id == shapeId.toStdString()) {
                        g_store.updateShape(s->id, VisibilityUpdate{!s->visible});
                        break;
                    }
                }
                refresh();
            });

            // Trash delete button
            auto* trashBtn = new QToolButton;
            trashBtn->setAutoRaise(true);
            trashBtn->setFixedSize(30, 30);
            trashBtn->setIcon(makeTrashIcon());
            trashBtn->setIconSize(QSize(24, 24));
            trashBtn->setStyleSheet(
                "QToolButton { border: none; border-radius: 4px; }"
                "QToolButton:hover { background-color: #e8e8e8; }"
            );
            trashBtn->setToolTip(QString::fromStdString(i18n.t("deleteSelected")));

            connect(trashBtn, &QToolButton::clicked, [this, shapeId]() {
                g_store.removeShape(shapeId.toStdString());
                refresh();
            });

            hLayout->addWidget(eyeBtn);
            hLayout->addWidget(trashBtn);
            hLayout->addStretch();

            table->setCellWidget(row, 2, actionsWidget);
        }

        // Highlight selected
        if (shape->id == g_store.selectedShapeId) {
            table->selectRow(row);
        }

        row++;
    }

    // Header label
    headerLabel->setText(QString("%1 (%2)")
        .arg(QString::fromStdString(i18n.t("shapeList")))
        .arg(g_store.shapeCount()));
}
