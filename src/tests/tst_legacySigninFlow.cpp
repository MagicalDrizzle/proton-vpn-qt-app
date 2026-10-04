#include <QtTest/QtTest>
#include "cli/signin/legacySigninFlow.h"
#include "signinTestUtils.h"

// CLI 1.0.0 to 1.0.3 ask for the password and then an authenticator code,
// both with getpass() on stderr. These tests replay that output exactly and
// check every step the flow asks VpnManager to take.

using namespace SigninTest;

namespace
{
// Signed in up to the code prompt.
const QStringList AT_CODE_PROMPT = {START, WRITE_PASSWORD, SHOW_CODE};
} // namespace

class TstLegacySigninFlow : public QObject
{
    Q_OBJECT

private:
    // Starts signing in and plays the CLI up to its code prompt.
    static QStringList reachCodePrompt(SigninFlow& flow)
    {
        QStringList lines = describe(flow.begin(USERNAME, PASSWORD));
        lines << feed(flow, {PASSWORD_PROMPT, GETPASS_CODE_PROMPT});
        return lines;
    }

private slots:
    void kind_isAuthenticatorCode()
    {
        const LegacySigninFlow flow;
        QCOMPARE(flow.kind(), SigninFlowKind::AuthenticatorCode);
    }

    void begin_startsSigninWithUsernameOnly()
    {
        LegacySigninFlow flow;
        QCOMPARE(describe(flow.begin(USERNAME, PASSWORD)), QStringList{START});
    }

    //  Password

    void passwordPrompt_sendsPassword()
    {
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT}), QStringList{WRITE_PASSWORD});
    }

    void passwordPrompt_splitAnywhere_sendsPasswordOnceWhole()
    {
        for (qsizetype at = 1; at < PASSWORD_PROMPT.size(); ++at)
        {
            LegacySigninFlow flow;
            (void)flow.begin(USERNAME, PASSWORD);
            QCOMPARE(feed(flow, {PASSWORD_PROMPT.left(at), PASSWORD_PROMPT.mid(at)}),
                     QStringList{WRITE_PASSWORD});
        }
    }

    void passwordPrompt_seenTwice_sendsPasswordOnce()
    {
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, PASSWORD_PROMPT}), QStringList{WRITE_PASSWORD});
    }

    void noTwoFactor_signsIn()
    {
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, SIGNED_IN}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void wrongPassword_finishesWithCliMessage()
    {
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        // The error continues the "Password: " line.
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, WRONG_PASSWORD}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(FINISH_FAILED, WRONG_PASSWORD_MESSAGE)});
    }

    void alreadySignedIn_finishesWithCliMessage()
    {
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {ALREADY_SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(FINISH_FAILED, ALREADY_SIGNED_IN_MESSAGE)});
    }

    void crashBeforeSecondFactor_finishesWithWhatCliSaid()
    {
        // No "Error:" line: the message is what is left without the prompts,
        // getpass's warnings, and the traceback.
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, CRASH}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(FINISH_FAILED, CRASH_MESSAGE)});
    }

    void failureWithoutOutput_finishesWithGenericMessage()
    {
        // E.g. a CLI that could not be started: there is nothing to show but
        // that signing in failed.
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(FINISH_FAILED, UNEXPLAINED_FAILURE_MESSAGE)});
    }

    void errorLineWithZeroExit_isFailure()
    {
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, WRONG_PASSWORD}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_OK)),
                 QStringList{withMessage(FINISH_FAILED, WRONG_PASSWORD_MESSAGE)});
    }

    //  Authenticator code

    void codePrompt_showsCodePrompt()
    {
        LegacySigninFlow flow;
        QCOMPARE(reachCodePrompt(flow), AT_CODE_PROMPT);
    }

    void wholeConversation_oneCharacterAtATime_sameSteps()
    {
        LegacySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feedCharByChar(flow, PASSWORD_PROMPT + GETPASS_CODE_PROMPT),
                 QStringList({WRITE_PASSWORD, SHOW_CODE}));
    }

    void codeAccepted_signsIn()
    {
        LegacySigninFlow flow;
        QCOMPARE(reachCodePrompt(flow), AT_CODE_PROMPT);
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void codeAskedAgainInSameRun_showsRejected()
    {
        // The CLI asks again without saying why: asked again after an answer
        // means the answer was wrong.
        LegacySigninFlow flow;
        QCOMPARE(reachCodePrompt(flow), AT_CODE_PROMPT);
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {GETPASS_CODE_PROMPT}), QStringList{withMessage(SHOW_CODE, CODE_REJECTED_MESSAGE)});

        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void codeFailedExit_showsCliMessage_thenNextCodeStartsNewAttempt()
    {
        LegacySigninFlow flow;
        QCOMPARE(reachCodePrompt(flow), AT_CODE_PROMPT);
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        // The error continues the "2FA Token: " line.
        QCOMPARE(feed(flow, {LEGACY_CODE_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(SHOW_CODE, LEGACY_FAILED_MESSAGE)});

        // The CLI has exited: the code starts it again (never with --totp,
        // which these versions do not have), the password is sent again, and
        // the code is sent when asked.
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{START});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT}), QStringList{WRITE_PASSWORD});
        QCOMPARE(feed(flow, {GETPASS_CODE_PROMPT}), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void newAttempt_codeWrongAgain_showsRejected()
    {
        LegacySigninFlow flow;
        QCOMPARE(reachCodePrompt(flow), AT_CODE_PROMPT);
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {LEGACY_CODE_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(SHOW_CODE, LEGACY_FAILED_MESSAGE)});

        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{START});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, GETPASS_CODE_PROMPT}), QStringList({WRITE_PASSWORD, WRITE_CODE}));
        QCOMPARE(feed(flow, {GETPASS_CODE_PROMPT}), QStringList{withMessage(SHOW_CODE, CODE_REJECTED_MESSAGE)});
    }

    void exitDuringCodeWithoutMessage_showsGenericMessage()
    {
        LegacySigninFlow flow;
        QCOMPARE(reachCodePrompt(flow), AT_CODE_PROMPT);
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(SHOW_CODE, UNEXPLAINED_FAILURE_MESSAGE)});
    }

    //  Security keys do not exist before CLI 1.0.4

    void securityKeyActions_whileRunning_doNothing()
    {
        LegacySigninFlow flow;
        QCOMPARE(reachCodePrompt(flow), AT_CODE_PROMPT);
        QCOMPARE(describe(flow.submitPin(PIN)), QStringList());
        QCOMPARE(describe(flow.retrySecurityKey()), QStringList());
        QCOMPARE(describe(flow.useCodeInstead()), QStringList());
    }

    void securityKeyActions_afterCliGaveUp_doNotStartIt()
    {
        LegacySigninFlow flow;
        QCOMPARE(reachCodePrompt(flow), AT_CODE_PROMPT);
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {LEGACY_CODE_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(SHOW_CODE, LEGACY_FAILED_MESSAGE)});

        QCOMPARE(describe(flow.submitPin(PIN)), QStringList());
        QCOMPARE(describe(flow.retrySecurityKey()), QStringList());
        QCOMPARE(describe(flow.useCodeInstead()), QStringList());
        // Still the plain sign-in when the code comes.
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{START});
    }
};

QTEST_MAIN(TstLegacySigninFlow)
#include "tst_legacySigninFlow.moc"
