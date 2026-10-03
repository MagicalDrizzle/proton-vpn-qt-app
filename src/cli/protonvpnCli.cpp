// protonvpnCli.cpp
// All VpnManager methods that interact with the protonvpn CLI by spawning
// a QProcess.  State management, settings, and polling infrastructure live
// in vpnManager.cpp.

#include "../vpnManager.h"

#include "../debug.h"
#include "cliNoiseFilter.h"
#include "logRedaction.h"
#include "flatpakUtils.h"
#include "statusMonitor.h"

#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <functional>
#include <memory>
#include <ranges>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Flatpak helper - when running inside a Flatpak sandbox, all CLI calls must
// be forwarded to the host via flatpak-spawn.
// ---------------------------------------------------------------------------

namespace
{
constexpr int DISCONNECT_SYNC_TIMEOUT_MS = 10000;
constexpr int LOGIN_CHECK_RETRIES = 5;
constexpr int MIN_COUNTRY_PARTS          = 2;
constexpr int NETWORK_READY_CHECK_RETRIES = 5;
constexpr int AUTO_CONNECT_RETRIES        = 5;

// `protonvpn info` normally answers in well under a second. The limit is
// generous because the CLI may be legitimately waiting on a keyring unlock
// prompt the user is still typing into.
constexpr int LOGIN_CHECK_TIMEOUT_MS = 30000;
// How long a timed-out CLI gets to exit after SIGTERM before it is killed.
constexpr int CLI_TERMINATE_GRACE_MS = 2000;
// Exit codes runCommand() reports when the CLI never produced one of its own.
// Negative, so they can never collide with a real process exit status.
constexpr int CLI_EXIT_FAILED_TO_START = -1;
constexpr int CLI_EXIT_TIMED_OUT       = -2;

// Returns {program, fullArgs} ready for QProcess::start.
std::pair<QString, QStringList> buildCliCommand(const QStringList& args)
{
    return buildHostCommand(QStringLiteral("protonvpn"), args);
}

// True when the CLI output contains a line that actually reports a failure.
//
// This deliberately anchors on the start of a line instead of searching the
// whole output for "error": the CLI prints warnings and guidance that merely
// contain that word, and matching those marked a *successful* sign-in as
// failed - leaving the user staring at a login error while already signed in,
// with the status monitor never started.
bool hasCliErrorLine(const QString& combined)
{
    const QStringList lines = combined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines)
    {
        const QString l = line.trimmed().toLower();
        if (l.startsWith(QLatin1String("error"))) return true;
        if (l.startsWith(QLatin1String("failed"))) return true;
        if (l.contains(QLatin1String("invalid credentials"))) return true;
        if (l.contains(QLatin1String("incorrect login"))) return true;
    }
    return false;
}
} // namespace

// ---------------------------------------------------------------------------
// Internal helper - run any protonvpn sub-command asynchronously.
// ---------------------------------------------------------------------------

void VpnManager::runCommand(const QStringList& args,
                            const std::function<void(int, const QString&, const QString&)>& callback,
                            int timeoutMs)
{
    QProcess* process = new QProcess(this);
    const QString cmdLine = QStringLiteral("protonvpn ") + args.join(QLatin1Char(' '));
    DBG_CLI(QStringLiteral(">>> ") + cmdLine);

    // Set when the watchdog gives up on the process, so finished() reports a
    // timeout rather than the exit code of the signal that ended it.
    std::shared_ptr<bool> timedOut = std::make_shared<bool>(false);
    QTimer* watchdog = nullptr;
    if (timeoutMs != CLI_NO_TIMEOUT)
    {
        watchdog = new QTimer(process);
        watchdog->setSingleShot(true);
        connect(watchdog, &QTimer::timeout, process, [process, timedOut, cmdLine, timeoutMs]()
        {
            *timedOut = true;
            DBG_CLI(QStringLiteral("No response from '%1' after %2 ms - terminating it.")
                        .arg(cmdLine).arg(timeoutMs));
            // SIGTERM first: under Flatpak the child is flatpak-spawn, which
            // forwards SIGTERM to the real CLI on the host, whereas SIGKILL
            // would only kill flatpak-spawn and leave the CLI running.
            process->terminate();
            QTimer::singleShot(CLI_TERMINATE_GRACE_MS, process, [process]()
            {
                if (process->state() != QProcess::NotRunning)
                {
                    process->kill();
                }
            });
        });
        watchdog->start(timeoutMs);
    }

    // A program that cannot be started never emits finished(), so without this
    // the callback - and anything waiting on it - would never run.
    connect(process, &QProcess::errorOccurred,
            this, [process, callback, cmdLine, watchdog](QProcess::ProcessError error)
            {
                if (error != QProcess::FailedToStart) return;
                if (watchdog != nullptr)
                {
                    watchdog->stop();
                }
                DBG_CLI(QStringLiteral("<<< ") + cmdLine +
                        QStringLiteral(" [failed to start: ") + process->errorString() + QStringLiteral("]"));
                callback(CLI_EXIT_FAILED_TO_START, QString(), process->errorString());
                process->deleteLater();
            });

    connect(process, &QProcess::finished,
            this, [process, callback, cmdLine, watchdog, timedOut](int exitCode, QProcess::ExitStatus)
            {
                if (watchdog != nullptr)
                {
                    watchdog->stop();
                }
                if (*timedOut == true)
                {
                    exitCode = CLI_EXIT_TIMED_OUT;
                }
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                const QString err = QString::fromUtf8(process->readAllStandardError()).trimmed();
                DBG_CLI(QStringLiteral("<<< ") + cmdLine +
                        (*timedOut == true
                             ? QStringLiteral(" [timed out]")
                             : QStringLiteral(" [exit=") + QString::number(exitCode) + QStringLiteral("]")));
                if (out.isEmpty() == false)
                {
                    DBG_CLI(QStringLiteral("    stdout: ") + LogRedaction::cliOutput(out));
                }
                if (err.isEmpty() == false)
                {
                    DBG_CLI(QStringLiteral("    stderr: ") + LogRedaction::cliOutput(err));
                }
                callback(exitCode, out, err);
                process->deleteLater();
            });
    auto [program, fullArgs] = buildCliCommand(args);
    process->start(program, fullArgs);
}

// ---------------------------------------------------------------------------
// Installation / login
// ---------------------------------------------------------------------------

void VpnManager::checkInstalled()
{
    DBG_CLI(QStringLiteral("Checking if protonvpn CLI is installed..."));
    QProcess* process = new QProcess(this);

    // Guard so exactly one of finished()/errorOccurred() reports the result.
    // The previous version blocked the GUI on waitForStarted() during startup
    // and could both report "not installed" and later delete a process that
    // was still starting.
    std::shared_ptr<bool> reported = std::make_shared<bool>(false);

    connect(process, &QProcess::finished,
            this, [this, process, reported](const int exitCode, QProcess::ExitStatus)
            {
                Q_UNUSED(exitCode)
                if (*reported == true)
                {
                    process->deleteLater();
                    return;
                }
                *reported = true;
                const QString out = QString::fromUtf8(process->readAllStandardOutput());
                const QString err = QString::fromUtf8(process->readAllStandardError());
                const bool installed = out.isEmpty() == false || err.isEmpty() == false;
                process->deleteLater();
                DBG_CLI(installed ? QStringLiteral("protonvpn CLI found.") : QStringLiteral("protonvpn CLI NOT found."));
                emit installedResult(installed);
            });

    connect(process, &QProcess::errorOccurred,
            this, [this, process, reported](QProcess::ProcessError error)
            {
                if (*reported == true) return;
                *reported = true;
                DBG_CLI(QStringLiteral("protonvpn CLI NOT found (QProcess::ProcessError=%1).")
                            .arg(error));
                process->deleteLater();
                emit installedResult(false);
            });

    auto [program, fullArgs] = buildCliCommand({QStringLiteral("--help")});
    process->start(program, fullArgs);
}

void VpnManager::checkLoginStatus()
{
    checkLoginStatus(LOGIN_CHECK_RETRIES);
}

void VpnManager::checkLoginStatus(int retriesLeft)
{
    runCommand({QStringLiteral("info")}, [this, retriesLeft](int exitCode, const QString& out, const QString&)
    {
        // A CLI that never answers is usually blocked on something only the
        // user can resolve (e.g. a keyring unlock prompt). Retrying on a timer
        // would just hold the app on "Starting..." for another full timeout.
        if (exitCode == CLI_EXIT_TIMED_OUT)
        {
            DBG_CLI(QStringLiteral("checkLoginStatus: protonvpn info did not respond within %1 ms.")
                        .arg(LOGIN_CHECK_TIMEOUT_MS));
            emit loginCheckTimedOut();
            return;
        }

        // checkInstalled() found the CLI moments ago, but it can no longer be
        // started (e.g. it was uninstalled in the meantime).
        if (exitCode == CLI_EXIT_FAILED_TO_START)
        {
            emit installedResult(false);
            return;
        }

        const QRegularExpression re(QStringLiteral(R"(Account:\s*'([^']+)')"));
        const QRegularExpressionMatch match = re.match(out);

        if (match.hasMatch())
        {
            // CLI responded correctly - determine logged-in state from the value.
            const QString accountVal = match.captured(1).trimmed();
            const bool loggedIn = (accountVal != QStringLiteral("None") && accountVal.isEmpty() == false);
            if (loggedIn)
            {
                startStatusMonitor();
                fetchAccountType();
                emit loginStatusResult(true, accountVal);
            }
            else
            {
                // Explicit "Account: 'None'" - genuinely not logged in, no point retrying.
                emit loginStatusResult(false, QString());
            }
            return;
        }

        // No "Account:" field in the output: the CLI didn't respond cleanly.
        // This is the transient case (daemon/keyring not ready on autostart).
        DBG_CLI(QStringLiteral("checkLoginStatus: no Account field (exitCode=%1), retries left: %2")
                    .arg(exitCode).arg(retriesLeft));

        if (retriesLeft > 0)
        {
            const int attempt = LOGIN_CHECK_RETRIES - retriesLeft + 1;
            const int delayMs = attempt * 1000;
            DBG_CLI(QStringLiteral("Retrying in %1 ms...").arg(delayMs));
            QTimer::singleShot(delayMs, this, [this, retriesLeft]()
            {
                checkLoginStatus(retriesLeft - 1);
            });
        }
        else
        {
            emit loginStatusResult(false, QString());
        }
    }, LOGIN_CHECK_TIMEOUT_MS);
}

void VpnManager::login(const QString& username, const QString& password)
{
    DBG_CLI(QStringLiteral("Login attempt for user: ") + LogRedaction::username(username));
    // CLI flow: protonvpn signin <username>
    //   stderr: "Password: "   -> write password + '\n' to stdin
    //   stderr: "2FA Token: "  -> optional; emit twoFactorRequired()

    if (m_signinProcess != nullptr)
    {
        m_signinProcess->kill();
        m_signinProcess->deleteLater();
        m_signinProcess = nullptr;
    }

    m_signinProcess = new QProcess(this);
    QProcess* process = m_signinProcess;

    // When the Qt app is launched from a terminal, child processes inherit the
    // controlling terminal.  Python's getpass.getpass() then opens /dev/tty
    // directly, bypassing the QProcess stdin pipe, so the password prompt appears
    // on the user's terminal rather than being captured here.
    // setsid() in the child (between fork and exec) creates a new session with
    // no controlling terminal, so getpass falls back to writing the prompt to
    // stderr and reading the answer from stdin, both of which QProcess pipes.
    m_signinProcess->setChildProcessModifier([]()
    {
        ::setsid();
    });

    struct State
    {
        QString accumulated;
        bool passwordSent = false;
        bool twoFAEmitted = false;
    };
    State* state = new State();

    auto processOutput = [this, process, state, password]()
    {
        if (state->passwordSent == false &&
            state->accumulated.contains(QStringLiteral("Password:")))
        {
            state->passwordSent = true;
            process->write((password + QLatin1Char('\n')).toUtf8());
        }

        if (state->twoFAEmitted == false &&
            state->accumulated.contains(QStringLiteral("2FA Token:")))
        {
            state->twoFAEmitted = true;
            DBG_CLI(QStringLiteral("Two-factor authentication required."));
            emit twoFactorRequired();
        }
    };

    connect(process, &QProcess::readyReadStandardOutput, this,
            [process, state, processOutput]()
            {
                state->accumulated.append(QString::fromUtf8(process->readAllStandardOutput()));
                processOutput();
            });

    connect(process, &QProcess::readyReadStandardError, this,
            [process, state, processOutput]()
            {
                state->accumulated.append(QString::fromUtf8(process->readAllStandardError()));
                processOutput();
            });

    connect(process, &QProcess::finished,
            this, [this, process, state](const int exitCode, QProcess::ExitStatus)
            {
                const QString combined = state->accumulated.trimmed();
                delete state;

                // If cancelLogin() was called first, m_signinProcess is already
                // nullptr - don't emit loginFinished for a deliberate cancellation.
                if (process != m_signinProcess)
                {
                    process->deleteLater();
                    return;
                }
                m_signinProcess = nullptr;
                process->deleteLater();

                const bool ok = exitCode == 0 && hasCliErrorLine(combined) == false;
                DBG_CLI(ok ? QStringLiteral("Login succeeded.") : QStringLiteral("Login failed (exit=") + QString::number(exitCode) + QStringLiteral(")."));
                QString errorMsg;
                if (ok == false)
                {
                    const QStringList lines = combined.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                    QStringList errorLines;
                    for (const QString& line : lines)
                    {
                        const QString l = line.trimmed();
                        if (l.isEmpty() == false
                            && l.startsWith(QStringLiteral("Password:")) == false
                            && l.startsWith(QStringLiteral("2FA")) == false
                            && l.startsWith(QStringLiteral("Warning:")) == false
                            && l.startsWith(QStringLiteral("Traceback")) == false
                            && l.contains(QStringLiteral(".py:")) == false
                            && line.front() != QLatin1Char(' ')
                            && line.front() != QLatin1Char('\t'))
                        {
                            errorLines.append(l);
                        }
                    }
                    errorMsg = errorLines.isEmpty() ? combined : errorLines.join(QLatin1Char('\n'));
                }
                else
                {
                    startStatusMonitor();
                    fetchAccountType();
                }
                emit loginFinished(ok, errorMsg);
            });

    auto [program, fullArgs] = buildCliCommand({QStringLiteral("signin"), username});
    process->start(program, fullArgs);
}

void VpnManager::cancelLogin()
{
    if (m_signinProcess != nullptr)
    {
        m_signinProcess->kill();
        m_signinProcess->deleteLater();
        m_signinProcess = nullptr;
    }
}

void VpnManager::submit2FA(const QString& token) const
{
    if (m_signinProcess != nullptr && m_signinProcess->state() == QProcess::Running)
    {
        m_signinProcess->write((token + QStringLiteral("\n")).toUtf8());
    }
}

void VpnManager::signOut()
{
    DBG_CLI(QStringLiteral("Signing out..."));
    stopStatusMonitor();
    runCommand({QStringLiteral("signout")}, [this](int exitCode, const QString&, const QString&)
    {
        DBG_CLI(exitCode == 0 ? QStringLiteral("Sign-out succeeded.") : QStringLiteral("Sign-out failed (exit=") + QString::number(exitCode) + QStringLiteral(")."));
        m_state = VpnState::Disconnected;
        emit signOutFinished(exitCode == 0);
    });
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

void VpnManager::connectVpn(const QString& country, const QString& city)
{
    DBG_CLI(QStringLiteral("Connecting to VPN - country: '") + (country.isEmpty() ? QStringLiteral("(fastest)") : country) +
            QStringLiteral("'  city: '") + (city.isEmpty() ? QStringLiteral("(any)") : city) + QStringLiteral("'"));
    m_lastConnectCountry = country;
    m_lastConnectCity    = city;
    m_connectedServer.clear();

    m_state = VpnState::Connecting;
    emit connectionStateChanged(m_state, QString());

    issueConnect(country, city, 0);
}

void VpnManager::startupAutoConnect(const QString& country, const QString& city)
{
    DBG_CLI(QStringLiteral("Auto-connecting to VPN on startup - country: '") + (country.isEmpty() ? QStringLiteral("(fastest)") : country) +
            QStringLiteral("'  city: '") + (city.isEmpty() ? QStringLiteral("(any)") : city) + QStringLiteral("'"));
    m_lastConnectCountry = country;
    m_lastConnectCity    = city;
    m_connectedServer.clear();

    m_state = VpnState::Connecting;
    emit connectionStateChanged(m_state, QString());

    checkNetworkReady(NETWORK_READY_CHECK_RETRIES, [this, country, city]()
    {
        issueConnect(country, city, AUTO_CONNECT_RETRIES);
    });
}

void VpnManager::checkNetworkReady(int retriesLeft, const std::function<void()>& onReady)
{
    QProcess* process = new QProcess(this);

    // Guard so onReady() runs exactly once: a launch failure and a late finish
    // must not both fire it, which would issue the connect twice.
    std::shared_ptr<bool> reported = std::make_shared<bool>(false);

    connect(process, &QProcess::finished, this,
            [this, process, retriesLeft, onReady, reported](int exitCode, QProcess::ExitStatus)
    {
        if (*reported == true)
        {
            process->deleteLater();
            return;
        }
        *reported = true;
        const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        process->deleteLater();

        // nmcli terse "STATE" is e.g. "connected", "connecting", "disconnected",
        // "asleep" - only the "connected*" family means NetworkManager has a
        // default route up. startsWith() (not contains()) so "disconnected"
        // does not falsely match.
        const bool ready = exitCode == 0 && out.startsWith(QStringLiteral("connected"));
        if (ready || retriesLeft <= 0)
        {
            onReady();
            return;
        }

        const int attempt = NETWORK_READY_CHECK_RETRIES - retriesLeft + 1;
        const int delayMs = attempt * 1000;
        DBG_CLI(QStringLiteral("Network not ready yet (nmcli state: '") + out +
                QStringLiteral("'), retrying in ") + QString::number(delayMs) + QStringLiteral(" ms..."));
        QTimer::singleShot(delayMs, this, [this, retriesLeft, onReady]()
        {
            checkNetworkReady(retriesLeft - 1, onReady);
        });
    });

    // nmcli missing or failed to launch - fail open rather than block
    // auto-connect indefinitely on a system without NetworkManager/nmcli.
    connect(process, &QProcess::errorOccurred, this,
            [process, onReady, reported](QProcess::ProcessError error)
    {
        if (*reported == true) return;
        *reported = true;
        DBG_CLI(QStringLiteral("nmcli could not be launched (QProcess::ProcessError=%1) - "
                               "proceeding without a network-ready check.").arg(error));
        process->deleteLater();
        onReady();
    });

    auto [program, fullArgs] = buildHostCommand(QStringLiteral("nmcli"),
        {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("STATE"),
         QStringLiteral("general"), QStringLiteral("status")});
    process->start(program, fullArgs);
}

void VpnManager::issueConnect(const QString& country, const QString& city, int retriesLeft)
{
    QStringList args{QStringLiteral("connect")};
    if (country.isEmpty() == false)
    {
        args << QStringLiteral("--country") << country;
    }
    if (city.isEmpty() == false)
    {
        args << QStringLiteral("--city") << city;
    }

    runCommand(args, [this, country, city, retriesLeft](int exitCode, const QString& out, const QString& err)
    {
        if (exitCode == 0)
        {
            DBG_CLI(QStringLiteral("VPN connected successfully."));
            m_state = VpnState::Connected;
            // Strip noise / port-forwarding guidelines from CLI output.
            QStringList lines = CliNoise::strip(out.split(QLatin1Char('\n')));

            while (lines.isEmpty() == false && lines.first().trimmed().isEmpty())
            {
                lines.removeFirst();
            }

            emit connectionStateChanged(m_state, lines.join(QLatin1Char('\n')));
            return;
        }

        if (retriesLeft > 0)
        {
            const int attempt = AUTO_CONNECT_RETRIES - retriesLeft + 1;
            const int delayMs = attempt * 1000;
            DBG_CLI(QStringLiteral("VPN connect attempt failed (exit=") + QString::number(exitCode) +
                    QStringLiteral("), retrying in ") + QString::number(delayMs) + QStringLiteral(" ms..."));
            QTimer::singleShot(delayMs, this, [this, country, city, retriesLeft]()
            {
                issueConnect(country, city, retriesLeft - 1);
            });
            return;
        }

        DBG_CLI(QStringLiteral("VPN connection failed (exit=") + QString::number(exitCode) + QStringLiteral("): ") + (err.isEmpty() ? out : err));
        m_state = VpnState::Error;
        emit connectionStateChanged(m_state, err.isEmpty() ? out : err);
        emit errorOccurred(err.isEmpty() ? out : err);
    });
}

void VpnManager::disconnectVpn()
{
    DBG_CLI(QStringLiteral("Disconnecting VPN..."));
    m_state = VpnState::Disconnecting;
    emit connectionStateChanged(m_state, QString());

    runCommand({QStringLiteral("disconnect")}, [this](const int exitCode, const QString& out, const QString& err)
    {
        if (exitCode == 0)
        {
            DBG_CLI(QStringLiteral("VPN disconnected successfully."));
            m_state = VpnState::Disconnected;
            emit connectionStateChanged(m_state, out);
        }
        else
        {
            DBG_CLI(QStringLiteral("VPN disconnect failed (exit=") + QString::number(exitCode) + QStringLiteral("): ") + (err.isEmpty() ? out : err));
            m_state = VpnState::Error;
            emit connectionStateChanged(m_state, err.isEmpty() ? out : err);
            emit errorOccurred(err.isEmpty() ? out : err);
        }
    });
}

void VpnManager::disconnectVpnSync()
{
    // Used at application exit - run synchronously so the event loop does not
    // need to stay alive.
    QProcess process;
    auto [program, fullArgs] = buildCliCommand({QStringLiteral("disconnect")});
    process.start(program, fullArgs);
    process.waitForFinished(DISCONNECT_SYNC_TIMEOUT_MS); // up to 10 s
}

void VpnManager::applyConfigValueAndReconnect(const QString& key, const QString& value)
{
    const QString country = m_lastConnectCountry;
    const QString city    = m_lastConnectCity;

    m_state = VpnState::Disconnecting;
    emit connectionStateChanged(m_state, QString());

    runCommand({QStringLiteral("disconnect")},
               [this, key, value, country, city](int exitCode, const QString&, const QString& err)
    {
        if (exitCode != 0)
        {
            m_state = VpnState::Error;
            emit connectionStateChanged(m_state, err);
            emit errorOccurred(err);
            return;
        }

        m_state = VpnState::Disconnected;
        emit connectionStateChanged(m_state, QString());

        QStringList args{QStringLiteral("config"), QStringLiteral("set")};
        args << key;
        args << value.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        runCommand(args, [this, country, city](int, const QString& out, const QString& err2)
        {
            emit configApplied((out + QLatin1Char('\n') + err2).trimmed());
            connectVpn(country, city);
        });
    });
}

// ---------------------------------------------------------------------------
// Server / city / account queries
// ---------------------------------------------------------------------------

void VpnManager::fetchCountries()
{
    runCommand({QStringLiteral("countries"), QStringLiteral("list")},
               [this](int exitCode, const QString& out, const QString& err)
    {
        if (exitCode != 0)
            return;
        const QString combined = out + QLatin1Char('\n') + err;
        QMap<QString, QString> countries;
        const QStringList lines = combined.split(QLatin1Char('\n'));
        // Output format: separator line starting with "--", then "Name   Code" rows.
        bool pastSeparator = false;
        for (const QString& line : lines)
        {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            if (trimmed.startsWith(QStringLiteral("--")))
            {
                pastSeparator = true;
                continue;
            }
            if (pastSeparator == false) continue;
            const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
            if (parts.size() < MIN_COUNTRY_PARTS) continue;
            const QString name = parts.first().trimmed();
            const QString code = parts.last().trimmed();
            if (name.isEmpty() == false && code.isEmpty() == false)
            {
                countries.insert(name, code);
            }
        }
        emit countriesReady(countries);
    });
}

void VpnManager::fetchCities(const QString& countryCode)
{
    runCommand({QStringLiteral("cities"), QStringLiteral("list"), countryCode},
               [this, countryCode](int exitCode, const QString& out, const QString& err)
               {
                   if (exitCode != 0)
                       return; // not authenticated or other error - don't overwrite with an empty list
                   const QString combined = out + QLatin1Char('\n') + err;
                   QList<QPair<QString, QString>> cities;
                   const QStringList lines = combined.split(QLatin1Char('\n'));
                   // Output format: separator line, then "City   Features" rows.
                   bool pastSeparator = false;
                   for (const QString& line : lines)
                   {
                       const QString trimmed = line.trimmed();
                       if (trimmed.isEmpty()) continue;
                       if (trimmed.startsWith(QStringLiteral("--")))
                       {
                           pastSeparator = true;
                           continue;
                       }
                       if (pastSeparator == false) continue;
                       const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
                       const QString city     = parts.value(0).trimmed();
                       const QString features = parts.value(1).trimmed();
                       if (city.isEmpty() == false)
                       {
                           cities.append({city, features});
                       }
                   }
                   emit citiesReady(countryCode, cities);
               });
}

void VpnManager::fetchCityFeatures(const QString& countryCode, const QString& city,
                                   const std::function<void(const QString& features)>& callback)
{
    runCommand({QStringLiteral("cities"), QStringLiteral("list"), countryCode},
               [city, callback](int exitCode, const QString& out, const QString& err)
               {
                   if (exitCode != 0)
                       return;
                   const QString combined = out + QLatin1Char('\n') + err;
                   const QStringList lines = combined.split(QLatin1Char('\n'));
                   bool pastSeparator = false;
                   for (const QString& line : lines)
                   {
                       const QString trimmed = line.trimmed();
                       if (trimmed.isEmpty()) continue;
                       if (trimmed.startsWith(QStringLiteral("--")))
                       {
                           pastSeparator = true;
                           continue;
                       }
                       if (pastSeparator == false) continue;
                       const QStringList parts = line.split(QStringLiteral("  "), Qt::SkipEmptyParts);
                       const QString cityName = parts.value(0).trimmed();
                       if (cityName.compare(city, Qt::CaseInsensitive) == 0)
                       {
                           callback(parts.value(1).trimmed());
                           return;
                       }
                   }
                   callback(QString()); // city not found
               });
}

void VpnManager::fetchInfo()
{
    runCommand({QStringLiteral("info")}, [this](int, const QString& out, const QString&)
    {
        QMap<QString, QString> result;
        const QRegularExpression re(QStringLiteral(R"((\w[\w ]*):\s*'([^']*)')"));
        QRegularExpressionMatchIterator it = re.globalMatch(out);
        while (it.hasNext())
        {
            const QRegularExpressionMatch m = it.next();
            result.insert(m.captured(1).trimmed(), m.captured(2).trimmed());
        }
        emit infoReady(result);
    });
}

void VpnManager::fetchAccountType()
{
    runCommand({QStringLiteral("config"), QStringLiteral("list")},
               [this](int exitCode, const QString& out, const QString& err)
    {
        // Nothing to classify: keep the current account type rather than
        // falling through to the Plus default below.
        if (exitCode == CLI_EXIT_FAILED_TO_START) return;

        const QString combined = out + QLatin1Char('\n') + err;
        const AccountType type =
            combined.contains(QStringLiteral("To upgrade to VPN Plus"), Qt::CaseInsensitive)
            ? AccountType::Free
            : AccountType::Plus;
        m_accountType = type;
        emit accountTypeReady(type);
    });
}

void VpnManager::fetchCliVersion()
{
    // "protonvpn" in the terminal with no args prints the ASCII banner; the version number
    // (semver) appears on the last banner line.
    runCommand({}, [this](int, const QString& out, const QString& err)
    {
        const QString combined = out + QLatin1Char('\n') + err;
        const QRegularExpression re(QStringLiteral(R"(\b(\d+\.\d+\.\d+)\b)"));
        const QStringList lines = combined.split(QLatin1Char('\n'));
        for (const QString& line : std::ranges::reverse_view(lines))
        {
            const QRegularExpressionMatch match = re.match(line);
            if (match.hasMatch())
            {
                emit cliVersionReady(match.captured(1));
                return;
            }
        }
        emit cliVersionReady(QString());
    });
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

void VpnManager::applyConfig(const QString& key, const bool enabled)
{
    applyConfigValue(key, enabled == true ? QStringLiteral("on") : QStringLiteral("off"));
}

void VpnManager::applyConfigValue(const QString& key, const QString& value)
{
    DBG_CLI(QStringLiteral("Applying CLI config: ") + key + QStringLiteral(" = ") + value);
    QStringList args{QStringLiteral("config"), QStringLiteral("set"), key};
    args << value.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    runCommand(args, [this](int, const QString& out, const QString& err)
    {
        const QString combined = (out + QLatin1Char('\n') + err).trimmed();
        emit configApplied(combined);
    });
}

