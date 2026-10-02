#pragma once

#include <QCloseEvent>
#include <QEvent>
#include <QFrame>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QSystemTrayIcon>
#include <QMenu>
#include "vpnManager.h"

class QNetworkAccessManager;

class VpnPage;
class LoginPage;
class CountriesPage;
class AccountPage;
class NotInstalledPage;
class SettingsPage;
class CliNotRespondingPage;
#ifdef QT_DEBUG
class DebugPage;
#endif

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    VpnManager* manager() const { return m_manager; }

private:
    enum class Page
    {
        Loading = 0,
        NotInstalled,
        Login,
        Vpn,
        Countries,
        Account,
        Settings,
        CliNotResponding,
#ifdef QT_DEBUG
        Debug,
#endif
    };

    VpnManager* m_manager;

    QWidget* m_sidebar;
    QFrame* m_sidebarDivider;
    QStackedWidget* m_stack;

    QToolButton* m_logoBtn;
    QToolButton* m_countriesNavBtn;
    QToolButton* m_accountNavBtn;
    QToolButton* m_settingsNavBtn;
#ifdef QT_DEBUG
    QToolButton* m_debugNavBtn = nullptr;
    QToolButton* m_loginDebugBtn = nullptr;
#endif

    NotInstalledPage* m_notInstalledPage;
    LoginPage* m_loginPage;
    VpnPage* m_vpnPage;
    CountriesPage* m_countriesPage;
    AccountPage* m_accountPage;
    SettingsPage* m_settingsPage;
    CliNotRespondingPage* m_cliNotRespondingPage;
#ifdef QT_DEBUG
    DebugPage* m_debugPage;
#endif

    void showPage(Page page);
    // Startup failure pages, also reachable from the Debug page for testing.
    void showNotInstalled();     // the protonvpn CLI could not be found
    void showCliNotResponding(); // the startup login check timed out
    void repositionLoginDebugBtn();
    void setupSidebar();
    void refreshIcons();
    void setNavActive(const QToolButton* btn);
    void startupCheck() const;
    void checkForUpdates();
    void updateTrayIcon(VpnState state);
    void updateTrayTooltipAndAction(VpnState state) const;
    void notifyStateTransition(VpnState state);
    // Prompts before quitting with an active connection.  Returns true when the
    // caller should proceed with the quit.
    bool confirmQuit();
    void sendNotification(const QString& title, const QString& message) const;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void maybeShowWhatsNew();

    QNetworkAccessManager* m_networkManager = nullptr;
    // Null when the desktop provides no system tray; every use must be guarded.
    QSystemTrayIcon* m_trayIcon = nullptr;
    QAction* m_trayConnectAction = nullptr;
    bool m_startupAutoConnectPending = false; // fire auto-connect once on first Disconnected state
    VpnState m_lastNotifiedState = VpnState::Unknown;
    bool m_whatsNewShown = false; // guard so we only show the dialog once per launch
#ifdef QT_DEBUG
    Page m_preDebugPage = Page::Login; // page to return to when leaving Debug
#endif

    // Credentials held in memory only while a login is in progress or retrying 2FA.
    // Wiped on successful login or when the user cancels back to the credentials page.
    QString m_loginUsername;
    QString m_loginPassword;
    QString m_pending2FAToken; // set when re-running login to auto-submit a 2FA retry
};
