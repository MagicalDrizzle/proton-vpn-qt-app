#include <QtTest/QtTest>
#include "cli/signin/securityKeySigninFlow.h"
#include "signinTestUtils.h"

// CLI 1.0.4 and newer: after the password, an account with a security key is
// asked for the key (and its PIN, if it has one); `signin --totp` asks for an
// authenticator code instead, as accounts without a key always are. These
// tests replay CLI 1.0.5's exact output and check every step the flow asks
// VpnManager to take.

using namespace SigninTest;

namespace
{
// Signed in up to "Waiting for security key...".
const QStringList AT_KEY_PROMPT = {START, WRITE_PASSWORD, SHOW_KEY};
} // namespace

class TstSecurityKeySigninFlow : public QObject
{
    Q_OBJECT

private:
    // Starts signing in and plays the CLI up to where it waits for the key.
    static QStringList reachKeyPrompt(SigninFlow& flow)
    {
        QStringList lines = describe(flow.begin(USERNAME, PASSWORD));
        lines << feed(flow, {PASSWORD_PROMPT, WAITING_FOR_KEY});
        return lines;
    }

    // From the key prompt, asks for a code instead and plays the CLI up to
    // its code prompt.
    static QStringList switchToCode(SigninFlow& flow)
    {
        QStringList lines = describe(flow.useCodeInstead());
        lines << feed(flow, {PASSWORD_PROMPT, CODE_PROMPT, HIDDEN_INPUT_TAIL});
        return lines;
    }

    // Signs in with the key from the touch on, as every successful key
    // sign-in ends.
    static QStringList touchAndSignIn(SigninFlow& flow)
    {
        QStringList lines = feed(flow, {TOUCH_KEY, KEY_READ, SIGNED_IN});
        lines << describe(flow.onFinished(EXIT_OK));
        return lines;
    }

private slots:
    void kind_isSecurityKey()
    {
        const SecurityKeySigninFlow flow;
        QCOMPARE(flow.kind(), SigninFlowKind::SecurityKey);
    }

    void begin_startsSigninWithoutTotp()
    {
        // The key is the default second factor; --totp only on request.
        SecurityKeySigninFlow flow;
        QCOMPARE(describe(flow.begin(USERNAME, PASSWORD)), QStringList{START});
    }

    //  Password

    void noTwoFactor_signsIn()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, SIGNED_IN}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void wrongPassword_finishesWithCliMessage()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, WRONG_PASSWORD}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(FINISH_FAILED, WRONG_PASSWORD_MESSAGE)});
    }

    void alreadySignedIn_finishesWithCliMessage()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {ALREADY_SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(FINISH_FAILED, ALREADY_SIGNED_IN_MESSAGE)});
    }

    void crashBeforeSecondFactor_finishesWithWhatCliSaid()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, CRASH}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(FINISH_FAILED, CRASH_MESSAGE)});
    }

    //  Security key

    void keyPrompt_showsWaitingForKey()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
    }

    void keyTouched_showsEachStep_thenSignsIn()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(touchAndSignIn(flow), QStringList({SHOW_TOUCH, SHOW_READ, FINISH_OK}));
    }

    void severalLinesInOneRead_shownInOrder()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {TOUCH_KEY + KEY_READ + SIGNED_IN}), QStringList({SHOW_TOUCH, SHOW_READ}));
    }

    void wholeConversation_oneCharacterAtATime_sameSteps()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        const QString output = PASSWORD_PROMPT + WAITING_FOR_KEY + NO_KEY_PROMPT + CHOOSE_KEY
                               + KEY_PIN_PROMPT + HIDDEN_INPUT_TAIL + TOUCH_KEY + KEY_READ + SIGNED_IN;
        QCOMPARE(feedCharByChar(flow, output),
                 QStringList({WRITE_PASSWORD, SHOW_KEY, SHOW_MISSING, SHOW_CHOOSE, SHOW_PIN, SHOW_TOUCH, SHOW_READ}));
    }

    void multipleKeys_showsChoice_thenSignsIn()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {CHOOSE_KEY, KEY_READ, SIGNED_IN}), QStringList({SHOW_CHOOSE, SHOW_READ}));
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void retryWhileWaitingForKey_doesNothing()
    {
        // Nothing to retry while the CLI is still reading the key.
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(describe(flow.retrySecurityKey()), QStringList());
        QCOMPARE(feed(flow, {TOUCH_KEY}), QStringList{SHOW_TOUCH});
        QCOMPARE(describe(flow.retrySecurityKey()), QStringList());
    }

    //  No key inserted

    void noKey_tryAgain_pressesEnter()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {NO_KEY_PROMPT}), QStringList{SHOW_MISSING});
        QCOMPARE(describe(flow.retrySecurityKey()), QStringList{WRITE_ENTER});
        QCOMPARE(touchAndSignIn(flow), QStringList({SHOW_TOUCH, SHOW_READ, FINISH_OK}));
    }

    void noKey_stillMissingAfterTryAgain_asksAgainWithoutError()
    {
        // Not an answer the CLI rejected: the key just is not there yet.
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {NO_KEY_PROMPT}), QStringList{SHOW_MISSING});
        QCOMPARE(describe(flow.retrySecurityKey()), QStringList{WRITE_ENTER});
        QCOMPARE(feed(flow, {NO_KEY_PROMPT}), QStringList{SHOW_MISSING});
        QCOMPARE(describe(flow.retrySecurityKey()), QStringList{WRITE_ENTER});
        QCOMPARE(touchAndSignIn(flow), QStringList({SHOW_TOUCH, SHOW_READ, FINISH_OK}));
    }

    void noKeyPrompt_splitAnywhere_shownOnce()
    {
        for (qsizetype at = 1; at < NO_KEY_PROMPT.size(); ++at)
        {
            SecurityKeySigninFlow flow;
            QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
            QCOMPARE(feed(flow, {NO_KEY_PROMPT.left(at), NO_KEY_PROMPT.mid(at)}), QStringList{SHOW_MISSING});
        }
    }

    //  PIN

    void pin_sentWhenSubmitted_thenSignsIn()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {KEY_PIN_PROMPT, HIDDEN_INPUT_TAIL}), QStringList{SHOW_PIN});
        QCOMPARE(describe(flow.submitPin(PIN)), QStringList{WRITE_PIN});
        QCOMPARE(touchAndSignIn(flow), QStringList({SHOW_TOUCH, SHOW_READ, FINISH_OK}));
    }

    void pinPrompt_noticeArrivesFirst_showsPinOnce()
    {
        // The prompt is on stdout and getpass's notice on stderr, so the app
        // can read them in either order.
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {HIDDEN_INPUT_TAIL, KEY_PIN_PROMPT}), QStringList{SHOW_PIN});
    }

    void wrongPin_showsIncorrectPin_thenSendsNextPinWhenAsked()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {KEY_PIN_PROMPT, HIDDEN_INPUT_TAIL}), QStringList{SHOW_PIN});
        QCOMPARE(describe(flow.submitPin(PIN)), QStringList{WRITE_PIN});
        // The error continues getpass's " " line.
        QCOMPARE(feed(flow, {PIN_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_PIN, PIN_REJECTED_MESSAGE)});

        // The CLI has exited: the PIN starts a new attempt and waits for the
        // CLI to ask for it.
        QCOMPARE(describe(flow.submitPin(PIN)), QStringList{START});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT}), QStringList{WRITE_PASSWORD});
        QCOMPARE(feed(flow, {WAITING_FOR_KEY}), QStringList{SHOW_KEY});
        QCOMPARE(feed(flow, {KEY_PIN_PROMPT, HIDDEN_INPUT_TAIL}), QStringList{WRITE_PIN});
        QCOMPARE(touchAndSignIn(flow), QStringList({SHOW_TOUCH, SHOW_READ, FINISH_OK}));
    }

    void pinAskedAgainInSameRun_showsIncorrectPin()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {KEY_PIN_PROMPT, HIDDEN_INPUT_TAIL}), QStringList{SHOW_PIN});
        QCOMPARE(describe(flow.submitPin(PIN)), QStringList{WRITE_PIN});
        QCOMPARE(feed(flow, {KEY_PIN_PROMPT, HIDDEN_INPUT_TAIL}),
                 QStringList{withMessage(SHOW_PIN, PIN_REJECTED_MESSAGE)});
    }

    //  Key failures

    void keyFailed_showsCliMessage_tryAgainStartsNewAttempt()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {TOUCH_KEY, KEY_FAILED}), QStringList{SHOW_TOUCH});
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_FAILED, KEY_FAILED_MESSAGE)});

        QCOMPARE(describe(flow.retrySecurityKey()), QStringList{START});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, WAITING_FOR_KEY}), QStringList({WRITE_PASSWORD, SHOW_KEY}));
        QCOMPARE(touchAndSignIn(flow), QStringList({SHOW_TOUCH, SHOW_READ, FINISH_OK}));
    }

    void crashAfterKeyRead_showsWhatCliSaid()
    {
        // Without an "Error:" line, the message leaves out every progress
        // line, the waiting line's --totp hint included.
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {TOUCH_KEY, KEY_READ, CRASH}), QStringList({SHOW_TOUCH, SHOW_READ}));
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_FAILED, CRASH_MESSAGE)});
    }

    void exitWhileWaitingWithoutMessage_showsGenericMessage()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)),
                 QStringList{withMessage(SHOW_FAILED, UNEXPLAINED_FAILURE_MESSAGE)});
    }

    //  Authenticator code instead of the key (--totp)

    void useCodeInstead_restartsWithTotp_thenSignsInWithCode()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(switchToCode(flow), QStringList({START_TOTP, WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void useCodeInstead_fromEveryKeyScreen_restartsWithTotp()
    {
        // What the CLI printed, and the screen it showed.
        const QList<QPair<QString, QString>> screens = {
            {NO_KEY_PROMPT, SHOW_MISSING},
            {CHOOSE_KEY, SHOW_CHOOSE},
            {KEY_PIN_PROMPT, SHOW_PIN},
            {TOUCH_KEY, SHOW_TOUCH},
        };
        for (const auto& [output, shown] : screens)
        {
            SecurityKeySigninFlow flow;
            QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
            QCOMPARE(feed(flow, {output}), QStringList{shown});
            QCOMPARE(switchToCode(flow), QStringList({START_TOTP, WRITE_PASSWORD, SHOW_CODE}));
        }
    }

    void useCodeInstead_afterKeyFailed_restartsWithTotp()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {KEY_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_FAILED, KEY_FAILED_MESSAGE)});
        QCOMPARE(switchToCode(flow), QStringList({START_TOTP, WRITE_PASSWORD, SHOW_CODE}));
    }

    void useCodeInstead_withPinWaitingForNextAttempt_asksForCode()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {KEY_PIN_PROMPT, HIDDEN_INPUT_TAIL}), QStringList{SHOW_PIN});
        QCOMPARE(describe(flow.submitPin(PIN)), QStringList{WRITE_PIN});
        QCOMPARE(feed(flow, {PIN_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_PIN, PIN_REJECTED_MESSAGE)});
        QCOMPARE(describe(flow.submitPin(PIN)), QStringList{START});

        // The PIN is not for the code prompt: it is asked for, not answered.
        QCOMPARE(switchToCode(flow), QStringList({START_TOTP, WRITE_PASSWORD, SHOW_CODE}));
    }

    void totpIgnoredThenCrash_messageLeavesOutIgnoredNotice()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(describe(flow.useCodeInstead()), QStringList{START_TOTP});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, TOTP_IGNORED, CRASH}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(FINISH_FAILED, CRASH_MESSAGE)});
    }

    void totpCodeWrong_askedAgainInSameRun_showsCliMessage()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(switchToCode(flow), QStringList({START_TOTP, WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {CODE_REJECTED_LINE + CODE_PROMPT, HIDDEN_INPUT_TAIL}),
                 QStringList{withMessage(SHOW_CODE, CODE_REJECTED_MESSAGE)});

        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void totpCodeWrong_linesInSeparateReads_showsCliMessageOnce()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(switchToCode(flow), QStringList({START_TOTP, WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {CODE_REJECTED_LINE, CODE_PROMPT, HIDDEN_INPUT_TAIL}),
                 QStringList{withMessage(SHOW_CODE, CODE_REJECTED_MESSAGE)});
    }

    void totpCodeFailedExit_nextCodeRestartsWithTotp()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(switchToCode(flow), QStringList({START_TOTP, WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {CODE_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_CODE, CODE_REJECTED_MESSAGE)});

        // Still the code: the user chose it over the key.
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{START_TOTP});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, CODE_PROMPT, HIDDEN_INPUT_TAIL}),
                 QStringList({WRITE_PASSWORD, WRITE_CODE}));
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void totpForAccountWithoutTwoFactor_signsIn()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(describe(flow.useCodeInstead()), QStringList{START_TOTP});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, TOTP_IGNORED, SIGNED_IN}), QStringList{WRITE_PASSWORD});
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    //  Accounts with only an authenticator app

    void codeOnlyAccount_asksForCodeWithoutTotp()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, CODE_PROMPT, HIDDEN_INPUT_TAIL}),
                 QStringList({WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void codeOnlyAccount_codeFailedExit_nextCodeRestartsWithoutTotp()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, CODE_PROMPT, HIDDEN_INPUT_TAIL}),
                 QStringList({WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {CODE_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_CODE, CODE_REJECTED_MESSAGE)});
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{START});
    }

    //  click 8.5 and newer: hidden prompts written whole by getpass on stderr

    void click85_wholeConversation_oneCharacterAtATime_sameSteps()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        const QString output = PASSWORD_PROMPT + WAITING_FOR_KEY + NO_KEY_PROMPT + CHOOSE_KEY
                               + GETPASS_PIN_PROMPT + TOUCH_KEY + KEY_READ + SIGNED_IN;
        QCOMPARE(feedCharByChar(flow, output),
                 QStringList({WRITE_PASSWORD, SHOW_KEY, SHOW_MISSING, SHOW_CHOOSE, SHOW_PIN, SHOW_TOUCH, SHOW_READ}));
    }

    void click85_codeOnlyAccount_asksForCode()
    {
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, GETPASS_CODE_PROMPT}), QStringList({WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void click85_wrongPin_errorContinuesPromptLine_thenSendsNextPinWhenAsked()
    {
        SecurityKeySigninFlow flow;
        QCOMPARE(reachKeyPrompt(flow), AT_KEY_PROMPT);
        QCOMPARE(feed(flow, {GETPASS_PIN_PROMPT}), QStringList{SHOW_PIN});
        QCOMPARE(describe(flow.submitPin(PIN)), QStringList{WRITE_PIN});
        QCOMPARE(feed(flow, {PIN_FAILED}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_PIN, PIN_REJECTED_MESSAGE)});

        QCOMPARE(describe(flow.submitPin(PIN)), QStringList{START});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, WAITING_FOR_KEY, GETPASS_PIN_PROMPT}),
                 QStringList({WRITE_PASSWORD, SHOW_KEY, WRITE_PIN}));
        QCOMPARE(touchAndSignIn(flow), QStringList({SHOW_TOUCH, SHOW_READ, FINISH_OK}));
    }

    void click85_totpCodeWrong_askedAgainInSameRun_showsRejected()
    {
        // The notice is on stdout and the prompt on stderr.
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(describe(flow.useCodeInstead()), QStringList{START_TOTP});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, GETPASS_CODE_PROMPT}), QStringList({WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {CODE_REJECTED_LINE, GETPASS_CODE_PROMPT}),
                 QStringList{withMessage(SHOW_CODE, CODE_REJECTED_MESSAGE)});
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {SIGNED_IN}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_OK)), QStringList{FINISH_OK});
    }

    void click85_totpCodeWrongThenCrash_messageLeavesOutRejectedNotice()
    {
        // The notice continues the "2FA Token: " line, so only its marker
        // keeps it out of the message.
        SecurityKeySigninFlow flow;
        (void)flow.begin(USERNAME, PASSWORD);
        QCOMPARE(describe(flow.useCodeInstead()), QStringList{START_TOTP});
        QCOMPARE(feed(flow, {PASSWORD_PROMPT, GETPASS_CODE_PROMPT}), QStringList({WRITE_PASSWORD, SHOW_CODE}));
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {CODE_REJECTED_LINE, GETPASS_CODE_PROMPT}),
                 QStringList{withMessage(SHOW_CODE, CODE_REJECTED_MESSAGE)});
        QCOMPARE(describe(flow.submitCode(CODE)), QStringList{WRITE_CODE});
        QCOMPARE(feed(flow, {CRASH}), QStringList());
        QCOMPARE(describe(flow.onFinished(EXIT_ERROR)), QStringList{withMessage(SHOW_CODE, CRASH_MESSAGE)});
    }
};

QTEST_MAIN(TstSecurityKeySigninFlow)
#include "tst_securityKeySigninFlow.moc"
