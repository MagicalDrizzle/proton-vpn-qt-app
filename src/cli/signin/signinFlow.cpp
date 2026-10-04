#include "signinFlow.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <algorithm>

namespace
{
// Prompts every CLI version prints. getpass() writes "Password: " itself;
// every version asks for the code as "2FA Token:" (1.0.4 and newer through
// click.prompt, which up to click 8.4 prints it on stdout and leaves the
// trailing space to getpass, and from click 8.5 hands all of it to getpass).
const QString PASSWORD_PROMPT = QStringLiteral("Password:");
const QString CODE_PROMPT     = QStringLiteral("2FA Token:");

// What the CLI reads as the end of an answer.
const QString ANSWER_END = QStringLiteral("\n");

// Lines that are Python's own noise rather than anything the CLI said:
// getpass's "Warning: Password input may be echoed." without a terminal, and
// tracebacks.
const QStringList NOISE_PREFIXES = {QStringLiteral("Warning:"), QStringLiteral("Traceback")};
// Source locations in warnings and tracebacks ("getpass.py:183: ...").
const QString SOURCE_LOCATION = QStringLiteral(".py:");

// An error the CLI reported. click prints them as "Error: <message>"; a
// prompt has no newline, so the message can follow one on the same line.
// Requiring a space or the line start before "Error:" skips Python's own
// "ValueError:" and the like.
const QRegularExpression ERROR_LINE(QStringLiteral(R"((?:^|\s)Error:\s*(.+)$)"));

// Shown when the CLI failed without saying why.
QString unexplainedFailureMessage()
{
    return QCoreApplication::translate("SigninFlow", "Sign-in failed. Please try again.");
}
} // namespace

QString signinPromptName(const SigninPrompt prompt)
{
    switch (prompt)
    {
        case SigninPrompt::Code:
            return QStringLiteral("two-factor code");

        case SigninPrompt::SecurityKey:
            return QStringLiteral("security key");

        case SigninPrompt::SecurityKeyTouch:
            return QStringLiteral("security key touch");

        case SigninPrompt::SecurityKeyPin:
            return QStringLiteral("security key PIN");

        case SigninPrompt::SecurityKeyMissing:
            return QStringLiteral("security key missing");

        case SigninPrompt::SecurityKeyChoose:
            return QStringLiteral("security key choice");

        case SigninPrompt::SecurityKeyRead:
            return QStringLiteral("security key read");

        case SigninPrompt::SecurityKeyFailed:
            return QStringLiteral("security key failed");
    }
    return {};
}

QString signinRejectedMessage(const SigninPrompt prompt)
{
    // CLI 1.0.4+ prints the code's wording before asking again; 1.0.3 and
    // older say nothing.
    if (prompt == SigninPrompt::SecurityKeyPin)
    {
        return QCoreApplication::translate("SigninFlow", "Incorrect PIN. Please try again.");
    }
    return QCoreApplication::translate("SigninFlow", "Invalid two-factor authentication code. Please try again.");
}

QString signinFlowName(const SigninFlowKind kind)
{
    switch (kind)
    {
        case SigninFlowKind::AuthenticatorCode:
            return QStringLiteral("authenticator code flow (CLI 1.0.0 to 1.0.3)");

        case SigninFlowKind::SecurityKey:
            return QStringLiteral("security key flow (CLI 1.0.4 and newer)");
    }
    return {};
}

// static
SigninEffect SigninEffect::start(const QStringList& arguments)
{
    SigninEffect e;
    e.type = Type::Start;
    e.arguments = arguments;
    return e;
}

// static
SigninEffect SigninEffect::write(const QString& text)
{
    SigninEffect e;
    e.type = Type::Write;
    e.text = text;
    return e;
}

// static
SigninEffect SigninEffect::showPrompt(const SigninPrompt prompt, const QString& error)
{
    SigninEffect e;
    e.type = Type::Prompt;
    e.prompt = prompt;
    e.message = error;
    return e;
}

// static
SigninEffect SigninEffect::finish(const bool ok, const QString& error)
{
    SigninEffect e;
    e.type = Type::Finish;
    e.ok = ok;
    e.message = error;
    return e;
}

SigninFlow::SigninFlow()
{
    addMarker(PASSWORD_PROMPT, [this]() { return onPasswordPrompt(); });
    addMarker(CODE_PROMPT, [this]() { return answerOrShow(SigninPrompt::Code); });
}

SigninEffects SigninFlow::begin(const QString& username, const QString& password)
{
    m_username = username;
    m_password = password;
    m_answers.clear();
    return restart();
}

SigninEffects SigninFlow::onOutput(const QString& text)
{
    m_output += text;
    m_unread += text;

    SigninEffects effects;
    while (true)
    {
        // The earliest marker in the unread output, so that two prompts in
        // one read are handled in the order the CLI printed them.
        qsizetype earliest = -1;
        const Marker* found = nullptr;
        for (const Marker& marker : std::as_const(m_markers))
        {
            const qsizetype at = m_unread.indexOf(marker.text);
            if (at >= 0 && (earliest < 0 || at < earliest))
            {
                earliest = at;
                found = &marker;
            }
        }
        if (found == nullptr) break;

        m_unread.remove(0, earliest + found->text.size());
        effects += found->handler();
    }

    // Keep only what could still be the start of a marker that has not
    // fully arrived yet.
    qsizetype longest = 0;
    for (const Marker& marker : std::as_const(m_markers))
    {
        longest = std::max(longest, marker.text.size());
    }
    if (m_unread.size() >= longest)
    {
        m_unread = m_unread.right(longest - 1);
    }
    return effects;
}

SigninEffects SigninFlow::onFinished(const int exitCode)
{
    m_running = false;
    const QString error = errorText(m_output);
    // Every 1.0.x CLI reports a failure as an "Error: ..." line and a non-zero
    // exit; either one alone is enough to call it failed.
    const bool reportedError = std::ranges::any_of(m_output.split(QLatin1Char('\n')), [](const QString& line)
    {
        return ERROR_LINE.match(line.trimmed()).hasMatch();
    });
    if (exitCode == 0 && reportedError == false)
    {
        return {SigninEffect::finish(true)};
    }

    // Past the password the CLI gave up on the second factor; the user can
    // answer again, and the next answer starts a new attempt.
    const QString message = error.isEmpty() ? unexplainedFailureMessage() : error;
    if (m_secondFactor == true)
    {
        return {SigninEffect::showPrompt(retryPrompt(error), message)};
    }
    return {SigninEffect::finish(false, message)};
}

SigninEffects SigninFlow::submitCode(const QString& code)
{
    return answer(SigninPrompt::Code, code);
}

SigninEffects SigninFlow::submitPin(const QString&)
{
    return {};
}

SigninEffects SigninFlow::retrySecurityKey()
{
    return {};
}

SigninEffects SigninFlow::useCodeInstead()
{
    return {};
}

QStringList SigninFlow::arguments(const QString& username) const
{
    return {QStringLiteral("signin"), username};
}

void SigninFlow::addMarker(const QString& text, const std::function<SigninEffects()>& handler,
                           const MarkerScope scope)
{
    m_markers.append({text, handler, scope});
}

SigninEffects SigninFlow::restart()
{
    m_running = true;
    m_passwordSent = false;
    m_secondFactor = false;
    m_answered.clear();
    m_unread.clear();
    m_output.clear();
    return {SigninEffect::start(arguments(m_username))};
}

SigninEffects SigninFlow::answer(const SigninPrompt prompt, const QString& text)
{
    if (m_running == true)
    {
        m_answered.append(prompt);
        return {SigninEffect::write(text + ANSWER_END)};
    }
    m_answers.insert(prompt, text);
    return restart();
}

SigninEffects SigninFlow::answerOrShow(const SigninPrompt prompt)
{
    m_secondFactor = true;
    m_lastPrompt = prompt;

    if (m_answers.contains(prompt) == true)
    {
        m_answered.append(prompt);
        return {SigninEffect::write(m_answers.take(prompt) + ANSWER_END)};
    }

    // Asked again after an answer in this run: the answer was rejected.
    const bool rejected = m_answered.removeOne(prompt);
    return {SigninEffect::showPrompt(prompt, rejected == true ? signinRejectedMessage(prompt) : QString())};
}

QString SigninFlow::errorText(const QString& output) const
{
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    QStringList errors;
    for (const QString& line : lines)
    {
        const QRegularExpressionMatch match = ERROR_LINE.match(line.trimmed());
        if (match.hasMatch() == true)
        {
            errors << match.captured(1).trimmed();
        }
    }
    if (errors.isEmpty() == false)
    {
        return errors.join(QLatin1Char('\n'));
    }

    // No "Error:" line (an unexpected failure): whatever the CLI said, minus
    // its prompts, progress messages, and Python's noise.
    QStringList said;
    for (const QString& line : lines)
    {
        if (line.front().isSpace() == true || line.contains(SOURCE_LOCATION) == true) continue;
        QString text = line;
        bool progressLine = false;
        for (const Marker& marker : std::as_const(m_markers))
        {
            if (marker.scope == MarkerScope::Line && text.contains(marker.text) == true)
            {
                progressLine = true;
                break;
            }
            text.remove(marker.text);
        }
        if (progressLine == true) continue;
        text = text.trimmed();
        const bool noise = std::ranges::any_of(NOISE_PREFIXES, [&text](const QString& prefix)
        {
            return text.startsWith(prefix);
        });
        if (text.isEmpty() == false && noise == false)
        {
            said << text;
        }
    }
    return said.join(QLatin1Char('\n'));
}

SigninEffects SigninFlow::onPasswordPrompt()
{
    if (m_passwordSent == true) return {};
    m_passwordSent = true;
    return {SigninEffect::write(m_password + ANSWER_END)};
}
