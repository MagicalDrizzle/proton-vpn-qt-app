#pragma once

// cliTable.h
// Splits the tables the protonvpn CLI prints (`countries list`, `cities
// list`, `config list`) into rows and cells. Lives in its own header so every
// command parses its table the same way, and so that can be unit tested.

#include <QLatin1Char>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace CliTable
{
// The data rows of a table, trimmed:
//
//   Server list is outdated, updating... This may take a moment.
//   Country                           Code
//   --------------------------------  ------
//   Afghanistan                       AF
//   Albania                           AL
//
// gives {"Afghanistan                       AF", "Albania ..."}. Rows start
// below the dashed rule under the header and end at the first blank line,
// where any footer text (as after `config list`) begins. Empty when the
// output has no table, for example because the CLI printed an error.
inline QStringList rows(const QString& output)
{
    QStringList result;
    bool inTable = false;
    for (const QString& line : output.split(QLatin1Char('\n')))
    {
        const QString trimmed = line.trimmed();
        if (inTable == false)
        {
            inTable = trimmed.startsWith(QStringLiteral("--"));
            continue;
        }
        if (trimmed.isEmpty() == true) break;
        result << trimmed;
    }
    return result;
}

// The cells of a row. Cells are padded apart by two or more spaces, while a
// single space stays inside a cell: "United States   US" gives
// {"United States", "US"} and "Zurich  P2P, Tor" gives {"Zurich", "P2P, Tor"}.
inline QStringList cells(const QString& row)
{
    static const QRegularExpression padding(QStringLiteral("\\s{2,}"));
    return row.trimmed().split(padding, Qt::SkipEmptyParts);
}
} // namespace CliTable
