#pragma once

// cliVersion.h
// Reads the CLI's version from the banner `protonvpn` prints when run without
// a command: the version ends the banner's last line ("... \_| 1.0.5").

#include <QLatin1Char>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <ranges>

namespace CliVersion
{
// "1.0.5" from the banner, or an empty string when it has no version. The
// banner holds the only version in the output; the last one found is taken.
inline QString fromBanner(const QString& output)
{
    static const QRegularExpression version(QStringLiteral(R"(\b(\d+\.\d+\.\d+)\b)"));
    const QStringList lines = output.split(QLatin1Char('\n'));
    for (const QString& line : std::ranges::reverse_view(lines))
    {
        const QRegularExpressionMatch match = version.match(line);
        if (match.hasMatch() == true)
        {
            return match.captured(1);
        }
    }
    return {};
}
} // namespace CliVersion
