#include <QtTest/QtTest>
#include "motionPreference.h"

// How the desktop's reduce-motion preference is read from each source, and
// how the Globe Animation setting combines with it.

using MotionPreferenceParsing::fromGsettings;
using MotionPreferenceParsing::fromKdeGlobals;
using MotionPreferenceParsing::fromPortal;

namespace
{
const QString APPEARANCE = QStringLiteral("org.freedesktop.appearance");
const QString REDUCED_MOTION = QStringLiteral("reduced-motion");
const QString GNOME = QStringLiteral("org.gnome.desktop.interface");
const QString ENABLE_ANIMATIONS = QStringLiteral("enable-animations");
} // namespace

class TstMotionPreference : public QObject
{
    Q_OBJECT

private slots:
    // Portal ---------------------------------------------------------------

    void fromPortal_reducedMotionReduce_isTrue()
    {
        QCOMPARE(fromPortal(APPEARANCE, REDUCED_MOTION, QVariant::fromValue(1u)), std::optional<bool>(true));
    }

    void fromPortal_reducedMotionNoPreference_isFalse()
    {
        QCOMPARE(fromPortal(APPEARANCE, REDUCED_MOTION, QVariant::fromValue(0u)), std::optional<bool>(false));
    }

    void fromPortal_reducedMotionNotANumber_isUnknown()
    {
        QVERIFY(fromPortal(APPEARANCE, REDUCED_MOTION, QVariant(QStringLiteral("reduce"))).has_value() == false);
    }

    void fromPortal_gnomeAnimationsDisabled_isTrue()
    {
        QCOMPARE(fromPortal(GNOME, ENABLE_ANIMATIONS, QVariant(false)), std::optional<bool>(true));
        QCOMPARE(fromPortal(GNOME, ENABLE_ANIMATIONS, QVariant(true)), std::optional<bool>(false));
    }

    void fromPortal_unrelatedSetting_isUnknown()
    {
        QVERIFY(fromPortal(APPEARANCE, QStringLiteral("color-scheme"), QVariant::fromValue(1u)).has_value() == false);
    }

    // kdeglobals -------------------------------------------------------------

    void fromKdeGlobals_factorZero_isTrue()
    {
        QCOMPARE(fromKdeGlobals(QStringLiteral("[General]\nfoo=1\n\n[KDE]\nAnimationDurationFactor=0\n")), true);
    }

    void fromKdeGlobals_slowerAnimations_isFalse()
    {
        QCOMPARE(fromKdeGlobals(QStringLiteral("[KDE]\nAnimationDurationFactor=0.5\n")), false);
    }

    void fromKdeGlobals_noFactor_isFalse()
    {
        // KDE's default is 1, so a missing key means animations are on.
        QCOMPARE(fromKdeGlobals(QStringLiteral("[KDE]\nLookAndFeelPackage=org.kde.breezedark.desktop\n")), false);
    }

    void fromKdeGlobals_factorInOtherGroup_isIgnored()
    {
        QCOMPARE(fromKdeGlobals(QStringLiteral("[Other]\nAnimationDurationFactor=0\n[KDE]\n")), false);
    }

    // gsettings ---------------------------------------------------------------

    void fromGsettings_values()
    {
        QCOMPARE(fromGsettings(QStringLiteral("true\n")), std::optional<bool>(false));
        QCOMPARE(fromGsettings(QStringLiteral("false\n")), std::optional<bool>(true));
        QVERIFY(fromGsettings(QStringLiteral("No such schema")).has_value() == false);
    }

    // Setting + preference ----------------------------------------------------

    void animationEnabled_onAndOff_ignoreDesktop()
    {
        using S = MotionPreference::Support;
        for (const S support : {S::Pending, S::Unavailable, S::Available})
        {
            QVERIFY(MotionPreference::animationEnabled(AppConfig::GlobeAnimation::On, support, true));
            QVERIFY(MotionPreference::animationEnabled(AppConfig::GlobeAnimation::Off, support, false) == false);
        }
    }

    void animationEnabled_auto_followsDesktop()
    {
        using S = MotionPreference::Support;
        const AppConfig::GlobeAnimation automatic = AppConfig::GlobeAnimation::Auto;
        QVERIFY(MotionPreference::animationEnabled(automatic, S::Available, false));
        QVERIFY(MotionPreference::animationEnabled(automatic, S::Available, true) == false);
    }

    void animationEnabled_autoWhileUnknown_defaultsOnButWaitsForAnswer()
    {
        using S = MotionPreference::Support;
        const AppConfig::GlobeAnimation automatic = AppConfig::GlobeAnimation::Auto;
        // No answer possible: on, as the setting's documented fallback.
        QVERIFY(MotionPreference::animationEnabled(automatic, S::Unavailable, false));
        // Still asking: held back so it does not flash on and then off.
        QVERIFY(MotionPreference::animationEnabled(automatic, S::Pending, false) == false);
    }
};

QTEST_MAIN(TstMotionPreference)
#include "tst_motionPreference.moc"
