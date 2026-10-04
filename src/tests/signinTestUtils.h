#pragma once

// signinTestUtils.h
// Shared by the sign-in flow tests: the CLI's output exactly as it reaches the
// app (standard output and error, with no terminal), and effects written out
// as text, so that a whole conversation compares, and fails, readably.

#include <QStringList>
#include "cli/signin/signinFlow.h"

namespace SigninTest
{
inline const QString USERNAME = QStringLiteral("alice");
inline const QString PASSWORD = QStringLiteral("hunter2");
inline const QString CODE     = QStringLiteral("123456");
inline const QString PIN      = QStringLiteral("2468");

inline constexpr int EXIT_OK    = 0;
inline constexpr int EXIT_ERROR = 1; // click's exit for "Error: ..."

// Every CLI version
// getpass() without a terminal, the first time in a run: Python's warning
// with its source line, the fallback notice, then the prompt, all on stderr.
// No newline follows the prompt, so whatever the CLI prints next continues
// its line.
inline const QString PASSWORD_PROMPT = QStringLiteral(
    "/usr/lib/python3.13/getpass.py:90: GetPassWarning: Can not control echo on the terminal.\n"
    "  passwd = fallback_getpass(prompt, stream)\n"
    "Warning: Password input may be echoed.\n"
    "Password: ");
// Later getpass() calls in the same run repeat only the notice.
inline const QString ECHO_WARNING = QStringLiteral("Warning: Password input may be echoed.\n");
inline const QString SIGNED_IN    = QStringLiteral("Successfully signed in as 'alice'\n");
inline const QString WRONG_PASSWORD = QStringLiteral(
    "Error: Authentication failed. Please check your username and password and try again.\n");
inline const QString ALREADY_SIGNED_IN = QStringLiteral(
    "Error: Already signed in, please sign out first before changing accounts.\n");
// An unexpected failure: a traceback and no "Error:" line.
inline const QString CRASH = QStringLiteral(
    "Traceback (most recent call last):\n"
    "  File \"/usr/lib/python3.13/site-packages/proton/session/api.py\", line 412, in _process\n"
    "    raise ProtonAPINotReachable(\"Proton API not reachable\")\n"
    "proton.session.exceptions.ProtonAPINotReachable: Proton API not reachable\n");
inline const QString CRASH_MESSAGE = QStringLiteral(
    "proton.session.exceptions.ProtonAPINotReachable: Proton API not reachable");

// A hidden prompt written whole by getpass on stderr: CLI 1.0.3's own
// getpass("2FA Token: "), and click.prompt(..., hide_input=True) from click
// 8.5 on (Arch's python-click, and the Standalone AppImage's).
inline const QString GETPASS_CODE_PROMPT = ECHO_WARNING + QStringLiteral("2FA Token: ");
inline const QString GETPASS_PIN_PROMPT  = ECHO_WARNING + QStringLiteral("Security key PIN: ");

// CLI 1.0.0 to 1.0.3
// A wrong code is asked for again in the same run, silently; once the
// session is no longer valid the CLI exits.
inline const QString LEGACY_CODE_FAILED = QStringLiteral("Error: 2FA Authentication failed. Please try again.\n");

// CLI 1.0.4 and newer
// click.prompt(..., hide_input=True) up to click 8.4 (e.g. Debian's and
// Ubuntu's python3-click) prints the prompt on stdout, then calls
// getpass(" "), which writes its notice and a space on stderr.
inline const QString CODE_PROMPT       = QStringLiteral("2FA Token:");
inline const QString KEY_PIN_PROMPT    = QStringLiteral("Security key PIN:");
inline const QString HIDDEN_INPUT_TAIL = ECHO_WARNING + QStringLiteral(" ");
// click.prompt(..., default="") for a visible answer, all on stdout with
// every click version.
inline const QString NO_KEY_PROMPT = QStringLiteral(
    "No security key detected. Insert your security key and press Enter: ");
inline const QString WAITING_FOR_KEY = QStringLiteral(
    "Waiting for security key... (or run 'protonvpn signin alice --totp' to use an authenticator code)\n");
inline const QString TOUCH_KEY  = QStringLiteral("Press the button on your security key.\n");
inline const QString CHOOSE_KEY = QStringLiteral("Multiple security keys were found. Tap the one you want to use.\n");
inline const QString KEY_READ   = QStringLiteral("Security key read. Signing in...\n");
inline const QString TOTP_IGNORED = QStringLiteral(
    "Ignoring --totp: this account has no two-factor authentication enabled.\n");
// Printed before the code prompt comes back in the same run.
inline const QString CODE_REJECTED_LINE = QStringLiteral(
    "Invalid two-factor authentication code. Please try again.\n");
inline const QString CODE_FAILED = QStringLiteral(
    "Error: Invalid two-factor authentication code. Please try again.\n");
inline const QString PIN_FAILED = QStringLiteral("Error: Incorrect PIN. Please try again.\n");
inline const QString KEY_FAILED = QStringLiteral(
    "Error: Security key authentication failed. Please try again.\n");

// The messages the app shows, as the CLI words them.
inline const QString CODE_REJECTED_MESSAGE  = QStringLiteral("Invalid two-factor authentication code. Please try again.");
inline const QString PIN_REJECTED_MESSAGE   = QStringLiteral("Incorrect PIN. Please try again.");
inline const QString KEY_FAILED_MESSAGE     = QStringLiteral("Security key authentication failed. Please try again.");
inline const QString LEGACY_FAILED_MESSAGE  = QStringLiteral("2FA Authentication failed. Please try again.");
inline const QString UNEXPLAINED_FAILURE_MESSAGE = QStringLiteral("Sign-in failed. Please try again.");
inline const QString WRONG_PASSWORD_MESSAGE = QStringLiteral(
    "Authentication failed. Please check your username and password and try again.");
inline const QString ALREADY_SIGNED_IN_MESSAGE = QStringLiteral(
    "Already signed in, please sign out first before changing accounts.");

// Effects as text
// "start signin alice", "write hunter2\n", "show security key PIN: <error>",
// "finish ok", "finish failed: <error>".
inline const QString START          = QStringLiteral("start signin alice");
inline const QString START_TOTP     = QStringLiteral("start signin alice --totp");
inline const QString WRITE_PASSWORD = QStringLiteral("write hunter2\\n");
inline const QString WRITE_CODE     = QStringLiteral("write 123456\\n");
inline const QString WRITE_PIN      = QStringLiteral("write 2468\\n");
inline const QString WRITE_ENTER    = QStringLiteral("write \\n");
inline const QString SHOW_CODE      = QStringLiteral("show two-factor code");
inline const QString SHOW_KEY       = QStringLiteral("show security key");
inline const QString SHOW_TOUCH     = QStringLiteral("show security key touch");
inline const QString SHOW_PIN       = QStringLiteral("show security key PIN");
inline const QString SHOW_MISSING   = QStringLiteral("show security key missing");
inline const QString SHOW_CHOOSE    = QStringLiteral("show security key choice");
inline const QString SHOW_READ      = QStringLiteral("show security key read");
inline const QString SHOW_FAILED    = QStringLiteral("show security key failed");
inline const QString FINISH_OK      = QStringLiteral("finish ok");
inline const QString FINISH_FAILED  = QStringLiteral("finish failed");

// A shown prompt or a failure, with its message.
inline QString withMessage(const QString& line, const QString& message)
{
    return line + QStringLiteral(": ") + message;
}

inline QString describe(const SigninEffect& effect)
{
    const QString message = effect.message.isEmpty() ? QString() : QStringLiteral(": ") + effect.message;
    switch (effect.type)
    {
        case SigninEffect::Type::Start:
            return QStringLiteral("start ") + effect.arguments.join(QLatin1Char(' '));

        case SigninEffect::Type::Write:
            return QStringLiteral("write ") + QString(effect.text).replace(QLatin1Char('\n'), QStringLiteral("\\n"));

        case SigninEffect::Type::Prompt:
            return QStringLiteral("show ") + signinPromptName(effect.prompt) + message;

        case SigninEffect::Type::Finish:
            return (effect.ok == true ? QStringLiteral("finish ok") : QStringLiteral("finish failed")) + message;
    }
    return {};
}

inline QStringList describe(const SigninEffects& effects)
{
    QStringList lines;
    for (const SigninEffect& effect : effects)
    {
        lines << describe(effect);
    }
    return lines;
}

// Output the CLI prints in several reads, as the app may receive it.
inline QStringList feed(SigninFlow& flow, const QStringList& chunks)
{
    QStringList lines;
    for (const QString& chunk : chunks)
    {
        lines << describe(flow.onOutput(chunk));
    }
    return lines;
}

// Output fed one character at a time, the worst split the app could receive.
inline QStringList feedCharByChar(SigninFlow& flow, const QString& output)
{
    QStringList lines;
    for (const QChar c : output)
    {
        lines << describe(flow.onOutput(QString(c)));
    }
    return lines;
}
} // namespace SigninTest
