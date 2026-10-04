#include "securityKeySigninFlow.h"

namespace
{
// What CLI 1.0.4+ prints while signing in with a security key
// (proton/vpn/cli/commands/account.py). The waiting line goes on to suggest
// --totp, so only its start is matched.
const QString WAITING_FOR_KEY = QStringLiteral("Waiting for security key...");
const QString TOUCH_KEY       = QStringLiteral("Press the button on your security key.");
const QString KEY_PIN_PROMPT  = QStringLiteral("Security key PIN:");
const QString NO_KEY_PROMPT   = QStringLiteral("No security key detected. Insert your security key and press Enter");
const QString CHOOSE_KEY      = QStringLiteral("Multiple security keys were found. Tap the one you want to use.");
const QString KEY_READ        = QStringLiteral("Security key read. Signing in...");
// Lines that need no step of their own, only kept out of error messages:
// the notice before a wrong code is asked for again in the same run (asked
// again after an answer already shows the code was wrong; with click 8.5 the
// notice continues the hidden prompt's line), and the notice that --totp
// means nothing for an account without two-factor authentication.
const QString CODE_REJECTED   = QStringLiteral("Invalid two-factor authentication code. Please try again.");
const QString TOTP_IGNORED    = QStringLiteral("Ignoring --totp:");
// The message the CLI exits with after a wrong PIN.
const QString PIN_REJECTED    = QStringLiteral("Incorrect PIN. Please try again.");

const QString TOTP_FLAG = QStringLiteral("--totp");
// Enter, at the "No security key detected" prompt.
const QString ENTER = QStringLiteral("\n");
} // namespace

SecurityKeySigninFlow::SecurityKeySigninFlow()
{
    addMarker(WAITING_FOR_KEY, [this]() { return answerOrShow(SigninPrompt::SecurityKey); }, MarkerScope::Line);
    addMarker(TOUCH_KEY, [this]() { return answerOrShow(SigninPrompt::SecurityKeyTouch); }, MarkerScope::Line);
    addMarker(KEY_PIN_PROMPT, [this]() { return answerOrShow(SigninPrompt::SecurityKeyPin); });
    addMarker(NO_KEY_PROMPT, [this]() { return answerOrShow(SigninPrompt::SecurityKeyMissing); });
    addMarker(CHOOSE_KEY, [this]() { return answerOrShow(SigninPrompt::SecurityKeyChoose); }, MarkerScope::Line);
    addMarker(KEY_READ, [this]() { return answerOrShow(SigninPrompt::SecurityKeyRead); }, MarkerScope::Line);
    addMarker(CODE_REJECTED, []() { return SigninEffects{}; }, MarkerScope::Line);
    addMarker(TOTP_IGNORED, []() { return SigninEffects{}; }, MarkerScope::Line);
}

SigninEffects SecurityKeySigninFlow::submitPin(const QString& pin)
{
    return answer(SigninPrompt::SecurityKeyPin, pin);
}

SigninEffects SecurityKeySigninFlow::retrySecurityKey()
{
    if (m_running == false)
    {
        return restart();
    }
    if (m_lastPrompt == SigninPrompt::SecurityKeyMissing)
    {
        return {SigninEffect::write(ENTER)};
    }
    return {};
}

SigninEffects SecurityKeySigninFlow::useCodeInstead()
{
    m_useCode = true;
    return restart();
}

QStringList SecurityKeySigninFlow::arguments(const QString& username) const
{
    QStringList args = SigninFlow::arguments(username);
    if (m_useCode == true)
    {
        args << TOTP_FLAG;
    }
    return args;
}

SigninPrompt SecurityKeySigninFlow::retryPrompt(const QString& error) const
{
    if (m_lastPrompt == SigninPrompt::Code)
    {
        return SigninPrompt::Code;
    }
    if (error.contains(PIN_REJECTED) == true)
    {
        return SigninPrompt::SecurityKeyPin;
    }
    return SigninPrompt::SecurityKeyFailed;
}
