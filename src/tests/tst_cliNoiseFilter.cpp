#include <QtTest/QtTest>
#include "cli/cliNoiseFilter.h"

// CliNoise::strip() decides which lines of protonvpn output are data and which
// are chatter. Both the status parser and the connect-result handler depend on
// it, so the predicate is pinned down here.

class TstCliNoiseFilter : public QObject
{
    Q_OBJECT

private slots:
    void isNoiseLine_updateNotice_isNoise()
    {
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("This version is outdated, please update.")));
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("Updating server list...")));
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("This may take a moment")));
    }

    void isNoiseLine_portForwardingGuidance_isNoise()
    {
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("To get your forwarded port, run natpmpc.")));
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("natpmpc -a 1 0 udp 60")));
    }

    void isNoiseLine_guideLinkWithUrl_isNoise()
    {
        QVERIFY(CliNoise::isNoiseLine(
            QStringLiteral("Guide: https://protonvpn.com/support/port-forwarding")));
    }

    void isNoiseLine_guideWithoutUrl_isKept()
    {
        // The "guide:" prefix alone is not enough - only the link line is noise.
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("Guide: see the manual")) == false);
    }

    void isNoiseLine_statusFields_areKept()
    {
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("Status: Connected")) == false);
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("Server: US-NJ#203 in Secaucus, United States")) == false);
    }

    void isNoiseLine_matchIsCaseInsensitive()
    {
        QVERIFY(CliNoise::isNoiseLine(QStringLiteral("THIS VERSION IS OUTDATED")));
    }

    void strip_removesOnlyNoise_andKeepsOrder()
    {
        const QStringList input = {
            QStringLiteral("Status: Connected"),
            QStringLiteral("This version is outdated."),
            QStringLiteral("Server: DE#42"),
            QStringLiteral("natpmpc -a 1 0 udp 60"),
            QStringLiteral("Load: 42%"),
        };

        const QStringList kept = CliNoise::strip(input);

        QCOMPARE(kept.size(), 3);
        QCOMPARE(kept.at(0), QStringLiteral("Status: Connected"));
        QCOMPARE(kept.at(1), QStringLiteral("Server: DE#42"));
        QCOMPARE(kept.at(2), QStringLiteral("Load: 42%"));
    }

    void strip_emptyInput_returnsEmpty()
    {
        QVERIFY(CliNoise::strip({}).isEmpty());
    }
};

QTEST_MAIN(TstCliNoiseFilter)
#include "tst_cliNoiseFilter.moc"
