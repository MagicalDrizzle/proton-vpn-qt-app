#pragma once

#include <QWidget>

// Shown when the protonvpn CLI is installed but does not answer the startup
// login check in time - most often because it is waiting on a keyring/wallet
// unlock prompt.
class CliNotRespondingPage : public QWidget
{
    Q_OBJECT

public:
    explicit CliNotRespondingPage(QWidget* parent = nullptr);

signals:
    // The user dealt with whatever was blocking the CLI and wants the login
    // check re-run without having to restart the app.
    void retryRequested();
};
