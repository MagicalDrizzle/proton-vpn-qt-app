#include "motionPreference.h"
#include "cli/flatpakUtils.h"
#include "debug.h"
#include "uiHelpers.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace
{
const QString PORTAL_SERVICE     = QStringLiteral("org.freedesktop.portal.Desktop");
const QString PORTAL_PATH        = QStringLiteral("/org/freedesktop/portal/desktop");
const QString SETTINGS_INTERFACE = QStringLiteral("org.freedesktop.portal.Settings");

const QString APPEARANCE_NAMESPACE = QStringLiteral("org.freedesktop.appearance");
const QString REDUCED_MOTION_KEY   = QStringLiteral("reduced-motion");
const QString GNOME_NAMESPACE      = QStringLiteral("org.gnome.desktop.interface");
const QString ENABLE_ANIMATIONS    = QStringLiteral("enable-animations");

// org.freedesktop.appearance reduced-motion: 0 = no preference, 1 = reduce.
constexpr uint REDUCED_MOTION_REDUCE = 1;

// Generous: the first portal call of a session can start the portal.
constexpr int PORTAL_TIMEOUT_MS = 3000;
constexpr int GSETTINGS_TIMEOUT_MS = 2000;

// Portal values arrive wrapped in one or two D-Bus variants (Read wraps twice).
QVariant unwrap(QVariant value)
{
    while (value.metaType() == QMetaType::fromType<QDBusVariant>())
    {
        value = qvariant_cast<QDBusVariant>(value).variant();
    }
    return value;
}

bool desktopIs(const QString& name)
{
    return qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':')).contains(name, Qt::CaseInsensitive);
}
} // namespace

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

std::optional<bool> MotionPreferenceParsing::fromPortal(const QString& ns, const QString& key, const QVariant& value)
{
    if (ns == APPEARANCE_NAMESPACE && key == REDUCED_MOTION_KEY)
    {
        bool ok = false;
        const uint v = value.toUInt(&ok);
        if (ok == false)
        {
            return std::nullopt;
        }
        return v == REDUCED_MOTION_REDUCE;
    }
    if (ns == GNOME_NAMESPACE && key == ENABLE_ANIMATIONS && value.metaType() == QMetaType::fromType<bool>())
    {
        return value.toBool() == false;
    }
    return std::nullopt;
}

bool MotionPreferenceParsing::fromKdeGlobals(const QString& contents)
{
    bool inKdeGroup = false;
    const QStringList lines = contents.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines)
    {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QLatin1Char('[')))
        {
            inKdeGroup = (line == QLatin1String("[KDE]"));
            continue;
        }
        if (inKdeGroup == false) continue;

        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq < 0 || line.left(eq).trimmed() != QLatin1String("AnimationDurationFactor")) continue;

        bool ok = false;
        const double factor = line.mid(eq + 1).trimmed().toDouble(&ok);
        return ok == true && qFuzzyIsNull(factor);
    }
    return false;
}

std::optional<bool> MotionPreferenceParsing::fromGsettings(const QString& output)
{
    const QString value = output.trimmed();
    if (value == QLatin1String("true")) return false;
    if (value == QLatin1String("false")) return true;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// MotionPreference
// ---------------------------------------------------------------------------

MotionPreference& MotionPreference::instance()
{
    static MotionPreference preference;
    return preference;
}

bool MotionPreference::animationEnabled(const AppConfig::GlobeAnimation setting, const Support support,
                                        const bool reduceMotion)
{
    switch (setting)
    {
        case AppConfig::GlobeAnimation::On:
            return true;

        case AppConfig::GlobeAnimation::Off:
            return false;

        default:
            if (support == Support::Pending) return false;
            if (support == Support::Unavailable) return true;
            return reduceMotion == false;
    }
}

bool MotionPreference::animationEnabled(const AppConfig::GlobeAnimation setting) const
{
    return animationEnabled(setting, m_support, m_reduceMotion);
}

void MotionPreference::start()
{
    if (m_started == true) return;
    m_started = true;

    readPortal(APPEARANCE_NAMESPACE, REDUCED_MOTION_KEY, [this]()
    {
        readPortal(GNOME_NAMESPACE, ENABLE_ANIMATIONS, [this]()
        {
            readDesktopSettings();
        });
    });
}

void MotionPreference::readPortal(const QString& ns, const QString& key, const std::function<void()>& onUnanswered)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected() == false)
    {
        onUnanswered();
        return;
    }

    // Read, not ReadOne: Read exists in every portal version (ReadOne only
    // since version 2), and unwrap() handles its extra variant layer.
    QDBusMessage message = QDBusMessage::createMethodCall(PORTAL_SERVICE, PORTAL_PATH, SETTINGS_INTERFACE,
                                                          QStringLiteral("Read"));
    message << ns << key;
    QDBusPendingCallWatcher* watcher =
        new QDBusPendingCallWatcher(bus.asyncCall(message, PORTAL_TIMEOUT_MS), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, ns, key, onUnanswered](QDBusPendingCallWatcher* w)
            {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                std::optional<bool> reduce;
                if (reply.type() == QDBusMessage::ReplyMessage && reply.arguments().isEmpty() == false)
                {
                    reduce = MotionPreferenceParsing::fromPortal(ns, key, unwrap(reply.arguments().constFirst()));
                }
                if (reduce.has_value() == false)
                {
                    onUnanswered();
                    return;
                }

                if (m_portalSubscribed == false)
                {
                    m_portalSubscribed = QDBusConnection::sessionBus().connect(
                        PORTAL_SERVICE, PORTAL_PATH, SETTINGS_INTERFACE, QStringLiteral("SettingChanged"),
                        this, SLOT(onPortalSettingChanged(QString, QString, QDBusVariant)));
                }
                setResult(Support::Available, *reduce, QStringLiteral("portal %1 %2").arg(ns, key));
            });
}

void MotionPreference::onPortalSettingChanged(const QString& ns, const QString& key, const QDBusVariant& value)
{
    const std::optional<bool> reduce = MotionPreferenceParsing::fromPortal(ns, key, unwrap(value.variant()));
    if (reduce.has_value() == false) return;
    setResult(Support::Available, *reduce, QStringLiteral("portal %1 %2, changed").arg(ns, key));
}

void MotionPreference::readDesktopSettings()
{
    // Inside the sandbox the desktop's own settings are out of reach; the
    // portal was the only way in.
    if (isRunningAsFlatpak() == true)
    {
        setResult(Support::Unavailable, false, QStringLiteral("not exposed by the desktop portal"));
        return;
    }

    if (desktopIs(QStringLiteral("KDE")) == true)
    {
        readKdeGlobals();
        return;
    }

    if (desktopIs(QStringLiteral("GNOME")) == true)
    {
        QProcess* process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process](const int exitCode, QProcess::ExitStatus)
        {
            process->deleteLater();
            const std::optional<bool> reduce = (exitCode == 0)
                ? MotionPreferenceParsing::fromGsettings(QString::fromUtf8(process->readAllStandardOutput()))
                : std::nullopt;
            setResult(reduce.has_value() ? Support::Available : Support::Unavailable, reduce.value_or(false),
                      QStringLiteral("gsettings"));
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError)
        {
            process->deleteLater();
            setResult(Support::Unavailable, false, QStringLiteral("gsettings could not run"));
        });
        process->start(QStringLiteral("gsettings"), {QStringLiteral("get"), GNOME_NAMESPACE, ENABLE_ANIMATIONS});
        QTimer::singleShot(GSETTINGS_TIMEOUT_MS, process, [process]()
        {
            if (process->state() != QProcess::NotRunning)
            {
                process->kill();
            }
        });
        return;
    }

    setResult(Support::Unavailable, false, QStringLiteral("no way to read it on this desktop"));
}

void MotionPreference::readKdeGlobals()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                       + QStringLiteral("/kdeglobals");
    QFile file(path);
    const bool reduce = (file.open(QIODevice::ReadOnly) == true)
        && MotionPreferenceParsing::fromKdeGlobals(QString::fromUtf8(file.readAll()));

    if (m_kdeGlobalsWatcher == nullptr)
    {
        m_kdeGlobalsWatcher = new QFileSystemWatcher(this);
        connect(m_kdeGlobalsWatcher, &QFileSystemWatcher::fileChanged, this, &MotionPreference::readKdeGlobals);
    }
    // KDE saves by replacing the file, which drops it from the watch list, so
    // it is put back on every read.
    if (QFile::exists(path) == true && m_kdeGlobalsWatcher->files().contains(path) == false)
    {
        m_kdeGlobalsWatcher->addPath(path);
    }
    setResult(Support::Available, reduce, QStringLiteral("kdeglobals"));
}

void MotionPreference::setResult(const Support support, const bool reduceMotion, const QString& source)
{
    if (support == m_support && reduceMotion == m_reduceMotion) return;
    if (support == Support::Available)
    {
        DBG_APP(QStringLiteral("Reduce motion: %1 (%2)").arg(onOffString(reduceMotion), source));
    }
    else
    {
        DBG_APP(QStringLiteral("Reduce motion: unknown (%1)").arg(source));
    }
    m_support = support;
    m_reduceMotion = reduceMotion;
    emit changed();
}
