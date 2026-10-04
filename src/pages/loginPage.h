#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "../widgets/infoBanner.h"
#include "../dialogs/errorDetailsDialog.h"
#include "../cli/signin/signinFlow.h"

class LoginPage : public QWidget
{
    Q_OBJECT

public:
    explicit LoginPage(QWidget* parent = nullptr);

    void setError(const QString& error) const;
    void setLoading(bool loading) const;
    // Shows what sign-in needs next (VpnManager::signinPrompt): the code view,
    // or the security key view in that state. `error` says why the last
    // answer failed.
    void showSigninPrompt(SigninPrompt prompt, const QString& error);
#ifdef QT_DEBUG
    // showSigninPrompt() for a Debug page preview: a screen without a Go Back
    // gets one, marked as a debug control, to return to the Debug page.
    void showSigninPreview(SigninPrompt prompt, const QString& error);
#endif
    void reset() const; // return to username/password view
    void checkPrereleaseBanner();

public slots:
    void onCliVersionReady(const QString& version);

signals:
    void loginRequested(const QString& username, const QString& password);
    void twoFASubmitted(const QString& token);
    void securityKeyPinSubmitted(const QString& pin);
    // Try Again on the security key view.
    void securityKeyRetryRequested();
    void useAuthenticatorCodeRequested();
    void loginCancelRequested();

private:
    // --- credentials view ---
    QWidget* m_credsWidget;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QPushButton* m_togglePasswordBtn;
    QPushButton* m_loginBtn;

    // --- 2FA view ---
    QWidget* m_tfaWidget;
    QLineEdit* m_tfaEdit;
    QPushButton* m_tfaSubmitBtn;
    QPushButton* m_tfaCancelBtn;

    // --- security key view ---
    QWidget* m_keyWidget = nullptr;
    QLabel* m_keyStatusLabel = nullptr;
    QLabel* m_keyPinLabel = nullptr;
    QLineEdit* m_keyPinEdit = nullptr;
    QPushButton* m_keyActionBtn = nullptr;  // Continue (with a PIN) or Try Again
    QPushButton* m_keyUseCodeBtn = nullptr;
    QPushButton* m_keyCancelBtn = nullptr;
    SigninPrompt m_keyPrompt = SigninPrompt::SecurityKey;

    // shared
    QStackedWidget* m_stack;
    QWidget* m_errorContainer = nullptr;
    QLabel* m_errorLabel;
    QScrollArea* m_errorScrollArea = nullptr;
    QPushButton* m_errorDetailsBtn = nullptr;
    mutable QString m_rawError;
    QVBoxLayout* m_outerLayout = nullptr;
    // Banner scroll area - holds all warning banners below the login card.
    // Scrollable so that multiple banners never squish the input fields.
    QScrollArea*  m_bannerScrollArea = nullptr;
    QVBoxLayout*  m_bannerLayout     = nullptr;
    InfoBanner* m_versionBanner = nullptr;
    InfoBanner* m_prereleaseBanner = nullptr;

    bool m_passwordVisible = false;
    void togglePasswordVisibility() const;
    // Keeps the Sign In button disabled until both fields have content.
    void updateSignInEnabled() const;
    void buildCredsWidget();
    void buildTFAWidget();
    void buildSecurityKeyWidget();
    void showCodePrompt() const;
    void showSecurityKey(SigninPrompt prompt);
};
