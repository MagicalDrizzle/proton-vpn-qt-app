#include "signinFlows.h"

#include "legacySigninFlow.h"
#include "securityKeySigninFlow.h"

namespace SigninFlows
{
SigninFlowKind kindForCliVersion(const QVersionNumber& version)
{
    if (version.isNull() == false && version < SECURITY_KEY_CLI_VERSION)
    {
        return SigninFlowKind::AuthenticatorCode;
    }
    return SigninFlowKind::SecurityKey;
}

std::unique_ptr<SigninFlow> forCliVersion(const QVersionNumber& version)
{
    switch (kindForCliVersion(version))
    {
        case SigninFlowKind::AuthenticatorCode:
            return std::make_unique<LegacySigninFlow>();

        case SigninFlowKind::SecurityKey:
        default:
            return std::make_unique<SecurityKeySigninFlow>();
    }
}
} // namespace SigninFlows
