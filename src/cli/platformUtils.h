#pragma once
// platformUtils.h
// Utilities for identifying the current packaging/runtime environment.

#include "appImageUtils.h"
#include "flatpakUtils.h"

#include <QCoreApplication>
#include <QLatin1Char>
#include <QString>

// Returns a human-readable string identifying the current packaging format:
// "Flatpak", "AppImage", or "System".
inline QString packageTypeName()
{
    if (isRunningAsFlatpak())
    {
        return QStringLiteral("Flatpak");
    }
    if (isRunningAsAppImage())
    {
        return QStringLiteral("AppImage");
    }
    return QStringLiteral("System");
}

namespace PlatformUtils
{
namespace Detail
{
// Quotes a program path for a .desktop file's Exec= key.
//
// The Desktop Entry spec requires arguments containing spaces to be enclosed in
// double quotes, with a backslash escape for the reserved characters. Without
// this, an install path such as /home/me/My Apps/ProtonVPN.AppImage is parsed
// as two separate arguments and the entry never launches.
inline QString quoteExecArgument(const QString& value)
{
    if (value.contains(QLatin1Char(' ')) == false &&
        value.contains(QLatin1Char('"')) == false &&
        value.contains(QLatin1Char('\\')) == false &&
        value.contains(QLatin1Char('$')) == false &&
        value.contains(QLatin1Char('`')) == false)
    {
        return value;
    }

    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'),  QStringLiteral("\\\""));
    escaped.replace(QLatin1Char('$'),  QStringLiteral("\\$"));
    escaped.replace(QLatin1Char('`'),  QStringLiteral("\\`"));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}
} // namespace Detail

// Returns the command that should go in the Exec= key of a .desktop file so the
// app relaunches correctly on the next login, for every packaging format:
//
//   Flatpak  : "flatpak run <app-id>"        (the binary is inside the sandbox)
//   AppImage : the path of the .AppImage     ($APPIMAGE - see appImagePath())
//   System   : the installed executable path
inline QString autostartExecCommand()
{
    if (isRunningAsFlatpak())
    {
        return QStringLiteral("flatpak run ") + qEnvironmentVariable("FLATPAK_ID");
    }

    if (isRunningAsAppImage())
    {
        const QString path = appImagePath();
        if (path.isEmpty() == false)
        {
            return Detail::quoteExecArgument(path);
        }
    }

    return Detail::quoteExecArgument(QCoreApplication::applicationFilePath());
}
} // namespace PlatformUtils
