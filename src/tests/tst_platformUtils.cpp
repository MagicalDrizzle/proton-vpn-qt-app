#include <QtTest/QtTest>
#include "cli/platformUtils.h"

// autostartExecCommand() produces the Exec= line of the XDG autostart entry.
// Getting it wrong is silent: the entry is written, and simply never launches
// anything on the next login.

class TstPlatformUtils : public QObject
{
    Q_OBJECT

private slots:
    void cleanup()
    {
        qunsetenv("APPIMAGE");
        qunsetenv("FLATPAK_ID");
    }

    void quoteExecArgument_plainPath_isUnquoted()
    {
        QCOMPARE(PlatformUtils::Detail::quoteExecArgument(QStringLiteral("/usr/bin/proton_vpn_qt")),
                 QStringLiteral("/usr/bin/proton_vpn_qt"));
    }

    void quoteExecArgument_pathWithSpace_isQuoted()
    {
        QCOMPARE(PlatformUtils::Detail::quoteExecArgument(QStringLiteral("/home/me/My Apps/VPN.AppImage")),
                 QStringLiteral("\"/home/me/My Apps/VPN.AppImage\""));
    }

    void quoteExecArgument_reservedCharacters_areEscaped()
    {
        QCOMPARE(PlatformUtils::Detail::quoteExecArgument(QStringLiteral("/opt/a$b")),
                 QStringLiteral("\"/opt/a\\$b\""));
    }

    void autostartExecCommand_appImage_usesAppImagePathNotMountPath()
    {
        // The mount path under /tmp disappears when the app exits, so the entry
        // has to point at the .AppImage file itself.
        qputenv("APPIMAGE", "/home/me/Apps/ProtonVPN.AppImage");
        QCOMPARE(PlatformUtils::autostartExecCommand(),
                 QStringLiteral("/home/me/Apps/ProtonVPN.AppImage"));
    }

    void autostartExecCommand_appImageWithSpaces_isQuoted()
    {
        qputenv("APPIMAGE", "/home/me/My Apps/ProtonVPN.AppImage");
        QCOMPARE(PlatformUtils::autostartExecCommand(),
                 QStringLiteral("\"/home/me/My Apps/ProtonVPN.AppImage\""));
    }

    void autostartExecCommand_flatpak_usesFlatpakRun()
    {
        qputenv("FLATPAK_ID", "io.github.wheat32.ProtonVPNQt");
        QCOMPARE(PlatformUtils::autostartExecCommand(),
                 QStringLiteral("flatpak run io.github.wheat32.ProtonVPNQt"));
    }

    void autostartExecCommand_system_usesApplicationFilePath()
    {
        QCOMPARE(PlatformUtils::autostartExecCommand(),
                 PlatformUtils::Detail::quoteExecArgument(QCoreApplication::applicationFilePath()));
    }

    void packageTypeName_reflectsEnvironment()
    {
        QCOMPARE(packageTypeName(), QStringLiteral("System"));
        qputenv("APPIMAGE", "/home/me/ProtonVPN.AppImage");
        QCOMPARE(packageTypeName(), QStringLiteral("AppImage"));
        qputenv("FLATPAK_ID", "io.github.wheat32.ProtonVPNQt");
        QCOMPARE(packageTypeName(), QStringLiteral("Flatpak"));
    }
};

QTEST_MAIN(TstPlatformUtils)
#include "tst_platformUtils.moc"
