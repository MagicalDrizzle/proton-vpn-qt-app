#pragma once

// motionPreference.h
// The desktop's "reduce motion" accessibility preference, for animations that
// should follow it (the VPN page's background globe).
//
// Read, in order, from whichever source answers first:
//   1. XDG Desktop Portal: org.freedesktop.appearance / reduced-motion.
//      Works natively, in the AppImage and inside the Flatpak sandbox, and
//      updates live through the portal's SettingChanged signal.
//   2. XDG Desktop Portal: org.gnome.desktop.interface / enable-animations,
//      which older GNOME portal backends expose instead.
//   3. Outside the Flatpak sandbox only, the desktop's own setting:
//      KDE's kdeglobals [KDE] AnimationDurationFactor (watched for changes)
//      or GNOME's gsettings enable-animations.
// If none answers, support() is Unavailable and callers treat motion as allowed.

#include <QDBusVariant>
#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>
#include <optional>
#include "appConfig.h"

class QFileSystemWatcher;

namespace MotionPreferenceParsing
{
// A portal setting value, already unwrapped from its D-Bus variants. Returns
// whether it asks for reduced motion, or nullopt for a namespace/key/value
// this code does not understand.
[[nodiscard]] std::optional<bool> fromPortal(const QString& ns, const QString& key, const QVariant& value);

// Contents of KDE's kdeglobals. An AnimationDurationFactor of 0 is KDE's
// "animations off"; any other value, or none (KDE's default is 1), is not.
[[nodiscard]] bool fromKdeGlobals(const QString& contents);

// Output of `gsettings get org.gnome.desktop.interface enable-animations`.
[[nodiscard]] std::optional<bool> fromGsettings(const QString& output);
} // namespace MotionPreferenceParsing

class MotionPreference : public QObject
{
    Q_OBJECT

public:
    enum class Support
    {
        Pending,     // still asking the desktop
        Unavailable, // no source answered; the preference cannot be followed
        Available,   // reduceMotion() reflects the desktop
    };

    static MotionPreference& instance();

    // Starts detection. Call once, after QApplication exists.
    void start();

    [[nodiscard]] Support support() const { return m_support; }
    [[nodiscard]] bool reduceMotion() const { return m_reduceMotion; }

    // Whether an animation governed by `setting` should run. Auto follows the
    // desktop when its preference is known, waits (returns false) while it is
    // still being read so the animation does not flash on and off at startup,
    // and falls back to on when it cannot be read.
    [[nodiscard]] static bool animationEnabled(AppConfig::GlobeAnimation setting, Support support, bool reduceMotion);
    [[nodiscard]] bool animationEnabled(AppConfig::GlobeAnimation setting) const;

signals:
    // support() or reduceMotion() changed.
    void changed();

private slots:
    void onPortalSettingChanged(const QString& ns, const QString& key, const QDBusVariant& value);

private:
    MotionPreference() = default;

    // Reads one portal setting; calls onUnanswered if the portal is missing,
    // does not know the key, or returns a value that cannot be interpreted.
    void readPortal(const QString& ns, const QString& key, const std::function<void()>& onUnanswered);
    void readDesktopSettings();
    void readKdeGlobals();
    // Records and logs the answer; `source` says where it came from (or, when
    // unavailable, why there is none).
    void setResult(Support support, bool reduceMotion, const QString& source);

    Support m_support = Support::Pending;
    bool m_reduceMotion = false;
    bool m_started = false;
    bool m_portalSubscribed = false;
    QFileSystemWatcher* m_kdeGlobalsWatcher = nullptr;
};
