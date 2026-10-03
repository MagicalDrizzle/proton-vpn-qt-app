#pragma once

// logRedaction.h
// Masks Proton account names before they reach the log. Log files are
// routinely attached to bug reports, so the account they were produced with
// should not be readable in them.

#include <QLatin1String>
#include <QRegularExpression>
#include <QString>

namespace LogRedaction
{
namespace Detail
{
// How many leading characters of each masked part stay readable.
constexpr qsizetype VISIBLE_PREFIX_LENGTH = 1;
} // namespace Detail

// Masks an account name down to its first character, plus the first character
// of the domain and the top-level domain for an email address:
//   "nick"                  -> "n***"
//   "nick@nicholaspage.dev" -> "n***@n***.dev"
// The mask has a fixed width so it does not give away the name's length.
//
// "None" is passed through unchanged: it is the CLI's marker for "not signed
// in", not a name, and masking it would hide whether anyone was signed in.
inline QString username(const QString& name)
{
    if (name.isEmpty())
    {
        return QStringLiteral("(empty)");
    }
    if (name == QLatin1String("None"))
    {
        return name;
    }

    const QString mask = QStringLiteral("***");
    const qsizetype atPos = name.indexOf(QLatin1Char('@'));
    if (atPos < 0)
    {
        return name.left(Detail::VISIBLE_PREFIX_LENGTH) + mask;
    }

    // The domain is masked too: a personal domain identifies its owner just as
    // well as the local part does.
    const QString domain = name.section(QLatin1Char('@'), 1);
    const qsizetype dotPos = domain.lastIndexOf(QLatin1Char('.'));
    const QString tld = (dotPos > 0) ? domain.mid(dotPos) : QString();
    return name.left(Detail::VISIBLE_PREFIX_LENGTH) + mask + QLatin1Char('@')
           + domain.left(Detail::VISIBLE_PREFIX_LENGTH) + mask + tld;
}

// Masks every account name inside captured CLI output:
//   `protonvpn info`   prints "Account: 'name@example.com'"
//   `protonvpn signin` prints "Successfully signed in as 'name@example.com'"
inline QString cliOutput(const QString& text)
{
    static const QRegularExpression accountRe(
        QStringLiteral(R"((Account:\s*|signed in as\s*)'([^']*)')"));

    QString result = text;
    QRegularExpressionMatchIterator it = accountRe.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        result.replace(m.captured(0),
                       m.captured(1) + QLatin1Char('\'') + username(m.captured(2)) + QLatin1Char('\''));
    }
    return result;
}
} // namespace LogRedaction
