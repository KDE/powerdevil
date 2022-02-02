#include "powerdevildefaultprofilevalues.h"
#include "powerdevilenums.h"

#include <powerdevilpowermanagement.h>

namespace PowerDevilProfileDefaults {

QString defaultIcon(const QString& profileGroup)
{
    return profileGroup == QStringLiteral("AC") ? QStringLiteral("battery-charging") :
           profileGroup == QStringLiteral("Battery") ? QStringLiteral("battery-060") :
           profileGroup == QStringLiteral("LowBattery") ? QStringLiteral("battery-low") :
           QStringLiteral("battery-charging");
}

int defaultPowerButtonAction()
{
    return !qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MOBILE")
        ? PowerDevilEnums::PowerButtonMode::ToggleScreenOnOffMode
        : PowerDevilEnums::PowerButtonMode::LogoutDialogMode;

}

int defaultPowerDownAction(const QString& profileGroup)
{
    return profileGroup == QStringLiteral("AC") ? PowerDevilEnums::PowerButtonMode::LogoutDialogMode :
           profileGroup == QStringLiteral("Battery") ? PowerDevilEnums::PowerButtonMode::LogoutDialogMode :
           profileGroup == QStringLiteral("LowBattery") ? PowerDevilEnums::PowerButtonMode::LogoutDialogMode :
           PowerDevilEnums::PowerButtonMode::LogoutDialogMode;
}

int defaultLidAction() {
    return PowerDevil::PowerManagement::instance()->canSuspend()
        ? PowerDevilEnums::PowerButtonMode::ToRamMode
        : PowerDevilEnums::PowerButtonMode::TurnOffScreenMode;

}

bool defaultManageSuspendSession(const QString& profileGroup)
{
    return profileGroup == QStringLiteral("AC") ? false :
        profileGroup == QStringLiteral("Battery") ? PowerDevil::PowerManagement::instance()->canSuspend() :
        profileGroup == QStringLiteral("LowBattery") ? PowerDevil::PowerManagement::instance()->canSuspend() :
        false;
}

int defaultSuspendSessionIdleTimeMsec(const QString& profileGroup)
{
    return profileGroup == QStringLiteral("AC")         ?   600000 :
           profileGroup == QStringLiteral("Battery")    ?   300000 :
           profileGroup == QStringLiteral("LowBattery") ?   300000 :
           /* profileGroup == QStringLiteral("AnyOther")   ?*/ 300000;

}

int defaultSuspendType(const QString& profileGroup)
{
    return profileGroup == QStringLiteral("AC") ? PowerDevilEnums::PowerButtonMode::ToRamMode :
        profileGroup == QStringLiteral("Battery") ? PowerDevilEnums::PowerButtonMode::ToRamMode :
        profileGroup == QStringLiteral("LowBattery") ? PowerDevilEnums::PowerButtonMode::ToRamMode :
        PowerDevilEnums::PowerButtonMode::ToRamMode;
}

int defaultManageDimDisplay(const QString& profileGroup)
{
    // everything currently is true but that might change in the future.
    return profileGroup == QStringLiteral("AC") ?   true :
        profileGroup == QStringLiteral("Battery") ?   true :
        profileGroup == QStringLiteral("LowBattery") ?   true :
        /* profileGroup == QStringLiteral("AnyOther")   ?*/ true;
}

int defaultDimDisplayIdleTimeMsec(const QString& profileGroup)
{
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MOBILE")) {
        return profileGroup == QStringLiteral("AC")         ?   300000 :
            profileGroup == QStringLiteral("Battery")    ?   30000 :
            profileGroup == QStringLiteral("LowBattery") ?   30000 :
            /* profileGroup == QStringLiteral("AnyOther")   ?*/ 30000;
    }

    return profileGroup == QStringLiteral("AC")         ?   300000 :
        profileGroup == QStringLiteral("Battery")    ?   120000 :
        profileGroup == QStringLiteral("LowBattery") ?   60000 :
        /* profileGroup == QStringLiteral("AnyOther")   ?*/ 300000;
}

int defaultManageDpms(const QString& profileGroup)
{
    return profileGroup == QStringLiteral("AC")         ?   false :
           profileGroup == QStringLiteral("Battery")    ?   false :
           profileGroup == QStringLiteral("LowBattery") ?   true :
           /* profileGroup == QStringLiteral("AnyOther")   ?*/ false;
}

bool defaultLockBeforeTurnOff()
{
    return !qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MOBILE");
}

int defaultDpmsIdleTimeSec(const QString& profileGroup)
{
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MOBILE")) {
        return profileGroup == QStringLiteral("AC")      ?   600 :
            profileGroup == QStringLiteral("Battery")    ?   300 :
            profileGroup == QStringLiteral("LowBattery") ?   120 :
            /* profileGroup == QStringLiteral("AnyOther")   ?*/ false;
    }

    return profileGroup == QStringLiteral("AC")      ?   600 :
        profileGroup == QStringLiteral("Battery")    ?   300 :
        profileGroup == QStringLiteral("LowBattery") ?   120 :
        /* profileGroup == QStringLiteral("AnyOther")   ?*/ false;
}

int defaultManageBrightnessControl(const QString& profileGroup)
{
    return profileGroup == QStringLiteral("AC")  ?   false :
    profileGroup == QStringLiteral("Battery")    ?   false :
    profileGroup == QStringLiteral("LowBattery") ?   true :
    /* profileGroup == QStringLiteral("AnyOther")   ?*/ false;
}

int defaultBrightnessControl(const QString& profileGroup)
{
    return profileGroup == QStringLiteral("AC")  ?   0 :
    profileGroup == QStringLiteral("Battery")    ?   0 :
    profileGroup == QStringLiteral("LowBattery") ?   30 :
    /* profileGroup == QStringLiteral("AnyOther")   ?*/ false;
}

}
