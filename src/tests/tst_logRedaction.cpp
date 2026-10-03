#include <QtTest/QtTest>
#include "cli/logRedaction.h"

// Log files end up attached to public bug reports, so these pin down exactly
// how much of an account name survives into them.

class TstLogRedaction : public QObject
{
    Q_OBJECT

private slots:
    void username_plainName_keepsFirstCharacterOnly()
    {
        QCOMPARE(LogRedaction::username(QStringLiteral("nick")), QStringLiteral("n***"));
    }

    void username_email_masksLocalPartAndDomain()
    {
        // A personal domain identifies its owner, so it must not survive intact.
        QCOMPARE(LogRedaction::username(QStringLiteral("nick@nicholaspage.dev")),
                 QStringLiteral("n***@n***.dev"));
    }

    void username_subdomain_keepsOnlyTopLevelDomain()
    {
        QCOMPARE(LogRedaction::username(QStringLiteral("nick@mail.example.co.uk")),
                 QStringLiteral("n***@m***.uk"));
    }

    void username_domainWithoutDot_masksWholeDomain()
    {
        QCOMPARE(LogRedaction::username(QStringLiteral("nick@localhost")),
                 QStringLiteral("n***@l***"));
    }

    void username_differentLengths_produceSameWidthMask()
    {
        // The mask must not give away how long the name is.
        QCOMPARE(LogRedaction::username(QStringLiteral("ab")),
                 QStringLiteral("a***"));
        QCOMPARE(LogRedaction::username(QStringLiteral("averyveryverylongname")),
                 QStringLiteral("a***"));
    }

    void username_none_isUnchanged()
    {
        // "None" is the CLI's signed-out marker; masking it to "N***" made a
        // signed-out log look like a signed-in one.
        QCOMPARE(LogRedaction::username(QStringLiteral("None")), QStringLiteral("None"));
    }

    void username_empty_returnsPlaceholder()
    {
        QCOMPARE(LogRedaction::username(QString()), QStringLiteral("(empty)"));
    }

    void cliOutput_accountLine_masksName()
    {
        QCOMPARE(LogRedaction::cliOutput(QStringLiteral("Account: 'nick@proton.me'")),
                 QStringLiteral("Account: 'n***@p***.me'"));
    }

    void cliOutput_signedOut_keepsNone()
    {
        QCOMPARE(LogRedaction::cliOutput(QStringLiteral("Account: 'None'")),
                 QStringLiteral("Account: 'None'"));
    }

    void cliOutput_signInConfirmation_masksName()
    {
        QCOMPARE(LogRedaction::cliOutput(QStringLiteral("Successfully signed in as 'nick@proton.me'")),
                 QStringLiteral("Successfully signed in as 'n***@p***.me'"));
    }

    void cliOutput_surroundingLines_areUnchanged()
    {
        const QString in = QStringLiteral("Server list is outdated, updating...\n"
                                          "Account: 'nick'\n"
                                          "Plan: VPN Plus");
        const QString expected = QStringLiteral("Server list is outdated, updating...\n"
                                                "Account: 'n***'\n"
                                                "Plan: VPN Plus");
        QCOMPARE(LogRedaction::cliOutput(in), expected);
    }

    void cliOutput_noAccountName_isUnchanged()
    {
        const QString in = QStringLiteral("Status: Connected\nServer: US-NY#167");
        QCOMPARE(LogRedaction::cliOutput(in), in);
    }
};

QTEST_MAIN(TstLogRedaction)
#include "tst_logRedaction.moc"
