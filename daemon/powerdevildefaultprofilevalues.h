#pragma once

#include <QString>

namespace PowerDevilProfileDefaults {
    QString defaultIcon(const QString& profileGroup);
    int defaultPowerButtonAction();
    int defaultPowerDownAction(const QString& profileGroup);
    int defaultLidAction();
    bool defaultManageSuspendSession(const QString& profileGroup);
    int defaultSuspendSessionIdleTimeMsec(const QString& profileGroup);
    int defaultSuspendType(const QString& profileGroup);
    int defaultManageDimDisplay(const QString& profileGroup);
    int defaultDimDisplayIdleTimeMsec(const QString& profileGroup);
    int defaultManageDpms(const QString& profileGroup);
    bool defaultLockBeforeTurnOff();
    int defaultDpmsIdleTimeSec(const QString& profileGroup);
    int defaultManageBrightnessControl(const QString& profileGroup);
    int defaultBrightnessControl(const QString& profileGroup);
}
