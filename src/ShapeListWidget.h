#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>

class MainWindow;

class ShapeListWidget : public QWidget {
    Q_OBJECT
public:
    explicit ShapeListWidget(MainWindow* mw, QWidget* parent = nullptr);
    void refresh();

private:
    MainWindow* mainWindow;
    QLabel* headerLabel;
    QTableWidget* table;
    QLabel* emptyLabel;
    void retranslateUi();
};
