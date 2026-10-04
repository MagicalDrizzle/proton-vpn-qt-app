#pragma once

// securityKeySigninFlow.h
// Sign-in with CLI 1.0.4 and newer, which added security keys (U2F/FIDO2).
// After the password, an account with a registered security key is asked for
// the key: insert it, touch it, and give its PIN if it has one. `--totp` asks
// for an authenticator or recovery code instead, and accounts without a key
// are asked for a code as before. A wrong code is answered with "Invalid
// two-factor authentication code. Please try again." and asked for again in
// the same run.

#include "signinFlow.h"

class SecurityKeySigninFlow : public SigninFlow
{
public:
    SecurityKeySigninFlow();

    [[nodiscard]] SigninFlowKind kind() const override { return SigninFlowKind::SecurityKey; }

    [[nodiscard]] SigninEffects submitPin(const QString& pin) override;
    // Presses Enter at "No security key detected", or starts a new attempt
    // after the key could not be used.
    [[nodiscard]] SigninEffects retrySecurityKey() override;
    // Signs in again with --totp, which asks for a code instead of the key.
    [[nodiscard]] SigninEffects useCodeInstead() override;

protected:
    [[nodiscard]] QStringList arguments(const QString& username) const override;
    [[nodiscard]] SigninPrompt retryPrompt(const QString& error) const override;

private:
    bool m_useCode = false; // sign in with --totp
};
