#include "ConfigWidget.h"

#include <solid/control/powermanager.h>

ConfigWidget::ConfigWidget(QWidget *parent)
  : QWidget(parent)
{
    setupUi(this);

    fillUi();
}

void ConfigWidget::fillUi()
{
    
    PoweredIdleCombo->addItem("Do nothing", 0);
    PoweredIdleCombo->addItem("Shutdown", Shutdown);
    BatteryCriticalCombo->addItem("Do nothing", 0);
    BatteryCriticalCombo->addItem("Shutdown", Shutdown);
    BatteryIdleCombo->addItem("Do nothing", 0);
    BatteryIdleCombo->addItem("Shutdown", Shutdown);

    Solid::Control::PowerManager::SuspendMethods methods = Solid::Control::PowerManager::supportedSuspendMethods();

    if(methods | Solid::Control::PowerManager::ToDisk)
    {
        PoweredIdleCombo->addItem("Suspend to Disk", S2Disk);
        BatteryCriticalCombo->addItem("Suspend to Disk", S2Disk);
        BatteryIdleCombo->addItem("Suspend to Disk", S2Disk);
    }
    if(methods | Solid::Control::PowerManager::ToRam)
    {
        PoweredIdleCombo->addItem("Suspend to Ram", S2Ram);
        BatteryCriticalCombo->addItem("Suspend to Ram", S2Ram);
        BatteryIdleCombo->addItem("Suspend to Ram", S2Ram);
    }
    if(methods | Solid::Control::PowerManager::Standby)
    {
        PoweredIdleCombo->addItem("Standby", Standby);
        BatteryCriticalCombo->addItem("Standby", Standby);
        BatteryIdleCombo->addItem("Standby", Standby);
    }

    Solid::Control::PowerManager::CpuFreqPolicies policies = Solid::Control::PowerManager::supportedCpuFreqPolicies();

    if(policies | Solid::Control::PowerManager::Performance)
    {
        PoweredFreqCombo->addItem("Performance", (int) Solid::Control::PowerManager::Performance);
        BatteryFreqCombo->addItem("Performance", (int) Solid::Control::PowerManager::Performance);
    }
    if(policies | Solid::Control::PowerManager::OnDemand)
    {
        PoweredFreqCombo->addItem("Dynamic (aggressive)", (int) Solid::Control::PowerManager::OnDemand);
        BatteryFreqCombo->addItem("Dynamic (aggressive)", (int) Solid::Control::PowerManager::OnDemand);
    }
    if(policies | Solid::Control::PowerManager::Conservative)
    {
        PoweredFreqCombo->addItem("Dynamic (less agressive)", (int) Solid::Control::PowerManager::Conservative);
        BatteryFreqCombo->addItem("Dynamic (less agressive)", (int) Solid::Control::PowerManager::Conservative);
    }
    if(policies | Solid::Control::PowerManager::Powersave)
    {
        PoweredFreqCombo->addItem("Powersave", (int) Solid::Control::PowerManager::Powersave);
        BatteryFreqCombo->addItem("Powersave", (int) Solid::Control::PowerManager::Powersave);
    }
}

void ConfigWidget::load()
{

}

void ConfigWidget::save()
{

}

#include "ConfigWidget.moc"
