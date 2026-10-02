#pragma once

#include <QLabel>
#include <QPushButton>

class NotInstalledPage : public QWidget
{
    Q_OBJECT

public:
    explicit NotInstalledPage(QWidget* parent = nullptr);

signals:
    // The user installed the CLI and wants the check re-run without having to
    // restart the app.
    void recheckRequested();
};
