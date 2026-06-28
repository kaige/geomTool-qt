#pragma once
#include <QWidget>
#include <QLabel>

class MainWindow;

class StatusBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatusBarWidget(MainWindow* mw, QWidget* parent = nullptr);
    void refresh();

private:
    MainWindow* mainWindow;
    QLabel* leftLabel;
    QLabel* centerLabel;
    QLabel* rightLabel;
};
