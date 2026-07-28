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
    table->setColumnCount(3);   // Type, Actions (vis+delete), ID
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->setStyleSheet(
        "QTableWidget { border: none; background: white; }"
        "QTableWidget::item { padding: 6px 10px; }"
        "QTableWidget::item:selected { background-color: #e3f2fd; color: #1565c0; font-weight: 600; }"
        "QHeaderView::section { padding: 8px 10px; background: #f5f5f5; border: none; "
        "border-bottom: 1px solid #e1dfdd; font-weight: 600; font-size: 13px; }"
    );

    // Column widths: Type stretches, Actions fixed, ID fixed
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);  // Type
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);    // Actions
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);    // ID
    table->setColumnWidth(1, 80);   // Actions: 👁 + ✕
    table->setColumnWidth(2, 50);   // ID

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

        if (col == 1) {
            // Toggle visibility
            g_store.updateShape(shape->id, VisibilityUpdate{!shape->visible});
        } else if (col == 2) {
            // Delete
            g_store.removeShape(shape->id);
        } else {
            // Select
            if (g_store.selectedShapeId == shape->id)
                g_store.selectShapeNull();
            else
                g_store.selectShape(shape->id);
        }
        refresh();
    });

    retranslateUi();
}

void ShapeListWidget::retranslateUi() {
    auto& i18n = I18n::instance();
    QStringList headers;
    headers << QString::fromStdString(i18n.t("type"))
            << QString::fromStdString(i18n.t("actions"))
            << QString::fromStdString(i18n.t("id"));
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
        // Type
        std::string typeKey = shapeTypeKey(shape->type);
        std::string typeLabel = i18n.t(typeKey);
        if (typeLabel == typeKey) typeLabel = typeKey; // fallback
        auto* typeItem = new QTableWidgetItem(QString::fromStdString(typeLabel));
        table->setItem(row, 0, typeItem);

        // Visibility toggle + delete (combined actions cell)
        auto* actionsItem = new QTableWidgetItem;
        QString visIcon = shape->visible ? QStringLiteral("\xF0\x9F\x91\x81") : QStringLiteral("\xE2\x80\x94");
        // Format: "👁  ✕" or "—  ✕"
        actionsItem->setText(QString("%1   \xE2\x9C\x95").arg(visIcon));
        actionsItem->setTextAlignment(Qt::AlignCenter);
        actionsItem->setToolTip(QString::fromStdString(i18n.t(shape->visible ? "hide" : "show")));
        table->setItem(row, 1, actionsItem);

        // ID
        auto* idItem = new QTableWidgetItem(QString::fromStdString(shape->id));
        idItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 2, idItem);

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
