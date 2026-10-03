#include <QtTest/QtTest>
#include "cli/cliSession.h"

// CliSession::requiresSignIn() decides when the app signs the user out on its
// own, so both directions matter: a missed match leaves the user stuck on a
// dead session, a false match signs them out for no reason.

class TstCliSession : public QObject
{
    Q_OBJECT

private slots:
    void requiresSignIn_invalidAccessToken_isTrue()
    {
        QVERIFY(CliSession::requiresSignIn(QStringLiteral(
            "Server list is outdated, updating... This may take a moment.\n"
            "Error: [HTTP/401, 401] Invalid access token")));
    }

    void requiresSignIn_connectWithoutSession_isTrue()
    {
        QVERIFY(CliSession::requiresSignIn(QStringLiteral(
            "Error: Authentication required.Please sign in with 'protonvpn signin' before connecting.\n\n"
            "Try 'protonvpn connect --help' for more information.")));
    }

    void requiresSignIn_citiesWithoutSession_isTrue()
    {
        QVERIFY(CliSession::requiresSignIn(QStringLiteral(
            "Error: Authentication required to view cities. Please sign in with 'protonvpn signin'")));
    }

    void requiresSignIn_connectionFailed_isFalse()
    {
        QVERIFY(CliSession::requiresSignIn(QStringLiteral(
            "Error: Connection failed. Try connecting to a different server or check your network settings."))
            == false);
    }

    void requiresSignIn_serverNumberContaining401_isFalse()
    {
        // A bare "401" used to count, which matched server names like this one.
        QVERIFY(CliSession::requiresSignIn(QStringLiteral(
            "Error: Timed out connecting to US-NY#401 in New York, United States.")) == false);
    }

    void requiresSignIn_emptyOutput_isFalse()
    {
        QVERIFY(CliSession::requiresSignIn(QString()) == false);
    }
};

QTEST_MAIN(TstCliSession)
#include "tst_cliSession.moc"
