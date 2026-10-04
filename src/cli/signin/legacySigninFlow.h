#pragma once

// legacySigninFlow.h
// Sign-in with CLI 1.0.0 to 1.0.3: `protonvpn signin <user>` asks for the
// password, then, if the account has two-factor authentication, for an
// authenticator code ("2FA Token:"). These versions know nothing of security
// keys and have no --totp.
//
// A wrong code is handled one of two ways depending on what Proton's API
// answers: the CLI asks for the code again in the same run, or it exits with
// "2FA Authentication failed. Please try again." SigninFlow covers both.

#include "signinFlow.h"

class LegacySigninFlow : public SigninFlow
{
public:
    [[nodiscard]] SigninFlowKind kind() const override { return SigninFlowKind::AuthenticatorCode; }

protected:
    // The only second factor these versions ask for is a code.
    [[nodiscard]] SigninPrompt retryPrompt(const QString&) const override { return SigninPrompt::Code; }
};
