#include "PowerDevilDaemon.h"

#include <kdemacros.h>
#include <KPluginFactory>
#include <KPluginLoader>

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
}

PowerDevilDaemon::~PowerDevilDaemon()
{
}

#include "PowerDevilDaemon.moc"
