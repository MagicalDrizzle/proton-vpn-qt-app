#pragma once

// signinFlows.h
// Picks the sign-in conversation for the installed CLI. This is the only
// place that compares CLI versions for signing in: each flow is written for
// its own versions and never checks them itself.

#include <QVersionNumber>
#include <memory>
#include "signinFlow.h"

namespace SigninFlows
{
// The first CLI that signs in with security keys and has `signin --totp`.
inline const QVersionNumber SECURITY_KEY_CLI_VERSION(1, 0, 4);

// The flow for a CLI version. The version is always known in practice (it is
// read from the CLI just before signing in); an unreadable one gets the
// newest flow.
[[nodiscard]] SigninFlowKind kindForCliVersion(const QVersionNumber& version);

[[nodiscard]] std::unique_ptr<SigninFlow> forCliVersion(const QVersionNumber& version);
} // namespace SigninFlows
