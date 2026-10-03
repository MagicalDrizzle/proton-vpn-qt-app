#include <QtTest/QtTest>
#include "cli/cliTable.h"

// Every CLI table the app reads (countries, cities, settings) goes through
// CliTable, so a slip here drops or invents rows everywhere. The samples are
// real CLI 1.0.3 output.

class TstCliTable : public QObject
{
    Q_OBJECT

private slots:
    // rows ----------------------------------------------------------------------

    void rows_countriesWithNoticeAbove_skipsNoticeAndHeader()
    {
        const QStringList rows = CliTable::rows(QStringLiteral(
            "Server list is outdated, updating... This may take a moment.\n"
            "Country                           Code\n"
            "--------------------------------  ------\n"
            "Afghanistan                       AF\n"
            "United States                     US"));
        QCOMPARE(rows, QStringList({QStringLiteral("Afghanistan                       AF"),
                                    QStringLiteral("United States                     US")}));
    }

    void rows_citiesWithBlankLinesAround_readsOnlyTheTable()
    {
        const QStringList rows = CliTable::rows(QStringLiteral(
            "\n"
            "Cities in Switzerland:\n"
            "City    Features\n"
            "------  ----------\n"
            "Zurich  P2P, Tor\n"
            "\n"));
        QCOMPARE(rows, QStringList({QStringLiteral("Zurich  P2P, Tor")}));
    }

    void rows_footerAfterBlankLine_isNotARow()
    {
        const QStringList rows = CliTable::rows(QStringLiteral(
            "Setting  Value\n"
            "-------  -----\n"
            "ipv6     on\n"
            "\n"
            "Use 'protonvpn config set <setting> <value>' to change settings.\n"));
        QCOMPARE(rows, QStringList({QStringLiteral("ipv6     on")}));
    }

    void rows_noTable_isEmpty()
    {
        QVERIFY(CliTable::rows(QStringLiteral(
            "Error: Authentication required to view cities. Please sign in with 'protonvpn signin'"))
            .isEmpty());
        QVERIFY(CliTable::rows(QString()).isEmpty());
    }

    // cells ---------------------------------------------------------------------

    void cells_paddedColumns_splitOnPaddingOnly()
    {
        QCOMPARE(CliTable::cells(QStringLiteral("United States                     US")),
                 QStringList({QStringLiteral("United States"), QStringLiteral("US")}));
        QCOMPARE(CliTable::cells(QStringLiteral("Zurich  P2P, Tor")),
                 QStringList({QStringLiteral("Zurich"), QStringLiteral("P2P, Tor")}));
    }

    void cells_oddPaddingWidth_leavesNoStraySpaces()
    {
        QCOMPARE(CliTable::cells(QStringLiteral("New York     P2P")),
                 QStringList({QStringLiteral("New York"), QStringLiteral("P2P")}));
    }

    void cells_cityWithoutFeatures_isOneCell()
    {
        QCOMPARE(CliTable::cells(QStringLiteral("Secaucus")), QStringList({QStringLiteral("Secaucus")}));
    }
};

QTEST_MAIN(TstCliTable)
#include "tst_cliTable.moc"
