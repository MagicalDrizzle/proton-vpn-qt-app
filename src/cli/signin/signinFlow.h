#pragma once

// signinFlow.h
// The conversation with `protonvpn signin`. The CLI asks for the password and
// then a second factor on its terminal, and how it does that depends on its
// version, so each version range has its own SigninFlow subclass;
// SigninFlows::forCliVersion() (signinFlows.h) picks the one to use.
//
// A flow only reads the CLI's output and decides what happens next. It never
// starts or touches a process itself: it answers with SigninEffects, which
// VpnManager carries out. That keeps every flow testable by feeding it the
// CLI's exact output.

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <functional>

// What the sign-in screen should show.
enum class SigninPrompt
{
    Code,               // an authenticator app or recovery code
    SecurityKey,        // waiting for a security key
    SecurityKeyTouch,   // the key is waiting to be touched
    SecurityKeyPin,     // the key wants its PIN
    SecurityKeyMissing, // no key found: insert one, then try again
    SecurityKeyChoose,  // several keys found: touch the one to use
    SecurityKeyRead,    // the key was read; signing in
    SecurityKeyFailed,  // the key could not be used: try again
};

// Which conversation a flow has with the CLI.
enum class SigninFlowKind
{
    AuthenticatorCode, // CLI 1.0.0 to 1.0.3: password, then an authenticator code
    SecurityKey,       // CLI 1.0.4 and newer: a security key first, a code with --totp
};

// Short names for logs and test output, e.g. "security key PIN".
[[nodiscard]] QString signinPromptName(SigninPrompt prompt);
[[nodiscard]] QString signinFlowName(SigninFlowKind kind);
// Shown with a prompt the CLI asks again in the same run: the answer was wrong.
[[nodiscard]] QString signinRejectedMessage(SigninPrompt prompt);

// One step VpnManager carries out for a flow, in order.
struct SigninEffect
{
    enum class Type
    {
        Start,  // start the CLI with `arguments`, ending any CLI already running
        Write,  // send `text` to the CLI's input
        Prompt, // show `prompt`, with `message` as its error when not empty
        Finish, // sign-in is over: `ok`, with `message` as the error when not
    };

    Type type = Type::Finish;
    QStringList arguments;
    QString text;
    SigninPrompt prompt = SigninPrompt::Code;
    QString message;
    bool ok = false;

    static SigninEffect start(const QStringList& arguments);
    static SigninEffect write(const QString& text);
    static SigninEffect showPrompt(SigninPrompt prompt, const QString& error = QString());
    static SigninEffect finish(bool ok, const QString& error = QString());
};

using SigninEffects = QList<SigninEffect>;

class SigninFlow
{
public:
    virtual ~SigninFlow() = default;
    // Markers hold handlers bound to `this`.
    SigninFlow(const SigninFlow&) = delete;
    SigninFlow& operator=(const SigninFlow&) = delete;

    [[nodiscard]] virtual SigninFlowKind kind() const = 0;

    // Starts signing in. The password is kept until the flow is destroyed, so
    // that after the CLI gives up on a second factor it can be started again
    // without asking for the password a second time.
    [[nodiscard]] SigninEffects begin(const QString& username, const QString& password);

    // New output from the CLI, standard output and error together, in the
    // order it arrived. Prompts may arrive in pieces.
    [[nodiscard]] SigninEffects onOutput(const QString& text);

    // The running CLI exited.
    [[nodiscard]] SigninEffects onFinished(int exitCode);

    // The user's answers. Each goes to the CLI if it is waiting for it; if the
    // CLI already gave up on the last attempt, it is started again and the
    // answer is sent when it asks.
    [[nodiscard]] SigninEffects submitCode(const QString& code);
    // Security keys exist only from CLI 1.0.4; flows without them ignore these.
    [[nodiscard]] virtual SigninEffects submitPin(const QString& pin);
    [[nodiscard]] virtual SigninEffects retrySecurityKey();
    [[nodiscard]] virtual SigninEffects useCodeInstead();

protected:
    SigninFlow();

    // `protonvpn` arguments for one attempt.
    [[nodiscard]] virtual QStringList arguments(const QString& username) const;

    // How much of the CLI's output a marker stands for.
    enum class MarkerScope
    {
        Text, // just the text, e.g. a prompt, which the next output can follow on the same line
        Line, // the whole line it starts, e.g. a progress message
    };

    // Recognizes `text` in the CLI's output and calls `handler` each time it
    // appears. Text is matched once, in the order it was printed. Markers are
    // never part of an error message.
    void addMarker(const QString& text, const std::function<SigninEffects()>& handler,
                   MarkerScope scope = MarkerScope::Text);

    // Which prompt shows `error` after the CLI exited with it during the
    // second factor, so the user can answer again.
    [[nodiscard]] virtual SigninPrompt retryPrompt(const QString& error) const = 0;

    // Starts a new attempt: the CLI again, with the password and any answers
    // waiting in m_answers sent when asked.
    [[nodiscard]] SigninEffects restart();

    // Sends `text` for `prompt` to the running CLI, or, if the CLI already
    // gave up, keeps it for that prompt and starts a new attempt.
    [[nodiscard]] SigninEffects answer(SigninPrompt prompt, const QString& text);

    // Sends the answer waiting for `prompt`, or else shows the prompt. A
    // prompt that comes back after being answered in the same run means the
    // answer was rejected, and is shown with that error.
    [[nodiscard]] SigninEffects answerOrShow(SigninPrompt prompt);

    // Error text for the user from a failed CLI run: its "Error: ..." messages,
    // or otherwise whatever it printed that is not a marker or Python noise.
    [[nodiscard]] QString errorText(const QString& output) const;

    bool m_running = false; // a CLI is running and may ask for input
    SigninPrompt m_lastPrompt = SigninPrompt::Code; // last second-factor prompt reached

private:
    struct Marker
    {
        QString text;
        std::function<SigninEffects()> handler;
        MarkerScope scope;
    };

    [[nodiscard]] SigninEffects onPasswordPrompt();

    QList<Marker> m_markers;
    QString m_username;
    QString m_password;
    QMap<SigninPrompt, QString> m_answers; // given before the CLI asked for them
    QList<SigninPrompt> m_answered;        // answered in the current run
    QString m_unread;      // output not yet matched against the markers
    QString m_output;      // everything the current CLI run printed
    bool m_passwordSent = false;
    bool m_secondFactor = false; // the current attempt reached a second factor
};
