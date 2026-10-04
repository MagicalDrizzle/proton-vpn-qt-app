#include <QtTest/QtTest>
#include "cli/cliVersion.h"
#include "cliBanner.h"

// CliVersion::fromBanner() reads the CLI's version from what `protonvpn`
// prints without a command. The version picks the sign-in flow, so a misread
// one would sign in the wrong way.

class TstCliVersion : public QObject
{
    Q_OBJECT

private slots:
    void fromBanner_realBanner_returnsVersion()
    {
        const QString version = QStringLiteral("1.0.5");
        QCOMPARE(CliVersion::fromBanner(CliBannerTest::banner(version)), version);
    }

    void fromBanner_withEmptyStandardError_returnsVersion()
    {
        const QString version = QStringLiteral("1.0.5");
        QCOMPARE(CliVersion::fromBanner(CliBannerTest::banner(version) + QLatin1Char('\n')), version);
    }

    void fromBanner_multiDigitParts_returnsWholeVersion()
    {
        for (const QString& version : {QStringLiteral("1.0.12"), QStringLiteral("10.20.30")})
        {
            QCOMPARE(CliVersion::fromBanner(CliBannerTest::banner(version)), version);
        }
    }

    void fromBanner_noVersion_returnsEmpty()
    {
        QCOMPARE(CliVersion::fromBanner(CliBannerTest::banner(QString())), QString());
    }

    void fromBanner_emptyOutput_returnsEmpty()
    {
        QCOMPARE(CliVersion::fromBanner(QString()), QString());
    }

    void fromBanner_twoPartNumber_isNotAVersion()
    {
        QCOMPARE(CliVersion::fromBanner(QStringLiteral("Python 3.13 is required\n")), QString());
    }
};

QTEST_MAIN(TstCliVersion)
#include "tst_cliVersion.moc"
