#pragma once

// cliBanner.h
// What `protonvpn` prints without a command, as CLI 1.0.5 prints it, for the
// tests that read the CLI's version from it.

#include <QString>

namespace CliBannerTest
{
// The output with `version` ending the banner's last line, where the CLI
// prints it (with a trailing space). An empty version leaves it out.
inline QString banner(const QString& version)
{
    return QStringLiteral(
        "Usage: protonvpn [OPTIONS] COMMAND [ARGS]...\n"
        "\n"
        "   ____            _               __     ______  _   _\n"
        "  |  _ \\ _ __ ___ | |_ ___  _ __   \\ \\   / /  _ \\| \\ | |\n"
        "  | |_) | '__/ _ \\| __/ _ \\| '_ \\   \\ \\ / /| |_) |  \\| |\n"
        "  |  __/| | | (_) | || (_) | | | |   \\ V / |  __/| |\\  |\n"
        "  |_|   |_|  \\___/ \\__\\___/|_| |_|    \\_/  |_|   |_| \\_| %1 \n"
        "\n"
        "  Proton VPN command-line interface for Linux.\n"
        "\n"
        "Options:\n"
        "  -v, --verbose  Show detailed output during command execution\n"
        "  -h, --help     Show this message and exit.\n"
        "\n"
        "Commands:\n"
        "  signin      Sign in to Proton VPN with your credentials.\n"
        "  connect     Connect to Proton VPN server.\n"
        "\n"
        "  Examples:\n"
        "    protonvpn connect --country US        # Connect to US\n"
        "\n").arg(version);
}
} // namespace CliBannerTest
