#pragma once

// cliSession.h
// Recognizes protonvpn CLI errors that mean the command failed because there
// is no usable session - either nobody is signed in or the server rejected the
// saved one. Lives in its own header so the matching rules can be unit tested.

#include <QLatin1String>
#include <QString>

namespace CliSession
{
// True when the output says the user has to sign in again, e.g.:
//   "Error: [HTTP/401, 401] Invalid access token"           (session revoked or expired)
//   "Error: Authentication required.Please sign in with ..." (no session at all)
//
// `protonvpn info` keeps printing the saved account name after the server has
// rejected the session, so these errors are the only sign that it has expired.
inline bool requiresSignIn(const QString& output)
{
    const QString lower = output.toLower();
    return lower.contains(QLatin1String("invalid access token"))
        || lower.contains(QLatin1String("http/401"))
        || lower.contains(QLatin1String("authentication required"))
        || lower.contains(QLatin1String("please sign in with"));
}
} // namespace CliSession
