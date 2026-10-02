#pragma once
// cliNoiseFilter.h
// Shared filter for the informational chatter the protonvpn CLI mixes into
// its output. Both StatusMonitor::parseStatusFields() and the connect-result
// handler in protonvpnCli.cpp have to strip exactly the same lines, so the
// predicate lives here rather than being duplicated in each of them.

#include <QLatin1String>
#include <QString>
#include <QStringList>

#include <ranges>

namespace CliNoise
{

// Returns true when the line is CLI chatter (update notices, port-forwarding
// guidance, progress messages) rather than data worth showing or parsing.
inline bool isNoiseLine(const QString& line)
{
    const QString ll = line.toLower();
    return ll.contains(QLatin1String("outdated"))                   ||
           ll.contains(QLatin1String("updating"))                   ||
           ll.contains(QLatin1String("this may take"))              ||
           ll.contains(QLatin1String("to get your forwarded port")) ||
           ll.contains(QLatin1String("natpmpc"))                    ||
           (ll.startsWith(QLatin1String("guide:")) &&
            ll.contains(QLatin1String("http")));
}

// Removes every noise line, preserving the order of the remaining lines.
inline QStringList strip(const QStringList& lines)
{
    QStringList kept = lines;
    kept.erase(std::ranges::remove_if(kept, isNoiseLine).begin(), kept.end());
    return kept;
}

} // namespace CliNoise
