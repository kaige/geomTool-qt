#include "ShapeListWidget.h"
#include "MainWindow.h"
#include "GeometryStore.h"
#include "I18n.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QSignalBlocker>

ShapeListWidget::ShapeListWidget(MainWindow* mw, QWidget* parent)
    : QWidget(parent), mainWindow(mw)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    headerLabel = new QLabel;
    headerLabel->setStyleSheet(
        "padding: 10px; font-size: 14px; font-weight: 600; "
        "background-color: #faf9f8; border-bottom: 1px solid #e1dfdd;"
    );
    layout->addWidget(headerLabel);

    // Table
    table = new QTableWidget;
    table->setColumnCount(4);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->setStyleSheet(
        "QTableWidget { border: none; background: white; }"
        "QTableWidget::item { padding: 4px 8px; }"
        "QTableWidget::item:selected { background-color: #e3f2fd; color: #1565c0; font-weight: 600; }"
        "QHeaderView::section { padding: 6px 8px; background: #f5f5f5; border: none; "
        "border-bottom: 1px solid #e1dfdd; font-weight: 600; }"
    );

    table->setColumnWidth(0, 80);  // Type
    table->setColumnWidth(1, 40);  // ID
    table->setColumnWidth(2, 30);  // Visibility
    table->setColumnWidth(3, 30);  // Delete

    layout->addWidget(table);

    connect(table, &QTableWidget::cellClicked, [this](int row, int col) {
        if (row < 0 || row >= (int)g_store.shapes.size()) return;
        auto& shape = g_store.shapes[row];

        if (col == 2) {
            // Toggle visibility
            g_store.updateShape(shape->id, VisibilityUpdate{!shape->visible});
        } else if (col == 3) {
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
            << QString::fromStdString(i18n.t("id"))
            << QString::fromStdString(i18n.t("actions"))
            << "";
    table->setHorizontalHeaderLabels(headers);
}

void ShapeListWidget::refresh() {
    auto& i18n = I18n::instance();

    QSignalBlocker blocker(table);
    table->setRowCount((int)g_store.shapes.size());

    int row = 0;
    for (auto& shape : g_store.shapes) {
        // Type
        std::string typeKey = shapeTypeKey(shape->type);
        std::string typeLabel = i18n.t(typeKey);
        if (typeLabel == typeKey) typeLabel = typeKey; // fallback
        auto* typeItem = new QTableWidgetItem(QString::fromStdString(typeLabel));
        table->setItem(row, 0, typeItem);

        // ID
        auto* idItem = new QTableWidgetItem(QString::fromStdString(shape->id));
        table->setItem(row, 1, idItem);

        // Visibility toggle
        auto* visItem = new QTableWidgetItem;
        visItem->setText(shape->visible ? "👁" : "—");
        visItem->setTextAlignment(Qt::AlignCenter);
        visItem->setToolTip(QString::fromStdString(i18n.t(shape->visible ? "hide" : "show")));
        table->setItem(row, 2, visItem);

        // Delete
        auto* delItem = new QTableWidgetItem("✕");
        delItem->setTextAlignment(Qt::AlignCenter);
        delItem->setToolTip(QString::fromStdString(i18n.t("delete")));
        delItem->setForeground(QColor("#EF5350"));
        table->setItem(row, 3, delItem);

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
