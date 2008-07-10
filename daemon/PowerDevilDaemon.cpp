#include "PowerDevilDaemon.h"

#include <kdemacros.h>
#include <KPluginFactory>
#include <KPluginLoader>

#include "PowerDevilSettings.h"

#include <solid/devicenotifier.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>

K_PLUGIN_FACTORY(PowerDevilFactory,
                 registerPlugin<PowerDevilDaemon>();)
K_EXPORT_PLUGIN(PowerDevilFactory("powerdevildaemon"))

PowerDevilDaemon::PowerDevilDaemon(QObject *parent, const QList<QVariant>&)
 : KDEDModule(parent),
 m_notifier(Solid::Control::PowerManager::notifier())
{
    /* First of all, let's check if a battery is present. If not, this
    module has to be shut down. */

    Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();
	
    bool found = false;
    //get a list of all devices that are Batteries
    foreach (Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::Battery, QString()))
      found = true;
    
    if ( !found )
    {
      //TODO: Shut the daemon down.
    }

    connect(m_notifier, SIGNAL(acAdapterStateChanged(int)), this, SLOT(acAdapterStateChanged(int)));
    connect(m_notifier, SIGNAL(batteryStateChanged(int)), this, SLOT(batteryStateChanged(int)));
    connect(m_notifier, SIGNAL(buttonPressed(int)), this, SLOT(buttonPressed(int)));
    
    //Setup initial state
    acAdapterStateChanged(Solid::Control::PowerManager::acAdapterState());
}

PowerDevilDaemon::~PowerDevilDaemon()
{
}

void PowerDevilDaemon::acAdapterStateChanged(int state)
{
    using namespace Solid::Control::PowerManager;
    
    if ( state == Plugged )
    {
        setBrightness(PowerDevilSettings::aCBrightness());
        setCpuFreqPolicy((CpuFreqPolicy) PowerDevilSettings::aCCpuPolicy());
    }
    
    else if ( state == Unplugged )
    {
        setBrightness(PowerDevilSettings::batBrightness());
        setCpuFreqPolicy((CpuFreqPolicy) PowerDevilSettings::batCpuPolicy());
    }
}

#include "PowerDevilDaemon.moc"
