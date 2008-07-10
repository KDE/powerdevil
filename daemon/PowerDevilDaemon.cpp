#include "PowerDevilDaemon.h"

#include <kdemacros.h>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KPassivePopup>
#include <KIcon>
#include <klocalizedstring.h>
#include <kjob.h>
#include <kworkspace/kdisplaymanager.h>

#include "PowerDevilSettings.h"

#include <solid/devicenotifier.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>

K_PLUGIN_FACTORY(PowerDevilFactory,
                 registerPlugin<PowerDevilDaemon>();)
K_EXPORT_PLUGIN(PowerDevilFactory("powerdevildaemon"))

PowerDevilDaemon::PowerDevilDaemon(QObject *parent, const QList<QVariant>&)
 : KDEDModule(parent),
 m_notifier(Solid::Control::PowerManager::notifier()),
 m_displayManager(new KDisplayManager())
{
    /* First of all, let's check if a battery is present. If not, this
    module has to be shut down. */

    //Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();
	
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
    if ( state == Solid::Control::PowerManager::Plugged )
    {
        Solid::Control::PowerManager::setBrightness(PowerDevilSettings::aCBrightness());
        Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
	PowerDevilSettings::aCCpuPolicy());
    }
    
    else if ( state == Solid::Control::PowerManager::Unplugged )
    {
        Solid::Control::PowerManager::setBrightness(PowerDevilSettings::batBrightness());
        Solid::Control::PowerManager::setCpuFreqPolicy((Solid::Control::PowerManager::CpuFreqPolicy)
	PowerDevilSettings::batCpuPolicy());
    }
}

void PowerDevilDaemon::batteryStateChanged(int state)
{
    switch(state)
    {
      //FIXME: Is this the right way?
        case Solid::Control::PowerManager::Critical:
            KPassivePopup::message(KPassivePopup::Boxed, "PowerDevil", i18n("Battery is at critical level"),
                           KIcon("dialog-warning").pixmap(20,20), new QWidget(), 15000);
            break;
        case Solid::Control::PowerManager::Warning:
            KPassivePopup::message(KPassivePopup::Boxed, "PowerDevil", i18n("Battery is at warning level"),
                           KIcon("dialog-warning").pixmap(20,20), new QWidget(), 15000);
            break;
        case Solid::Control::PowerManager::Low:
            KPassivePopup::message(KPassivePopup::Boxed, "PowerDevil", i18n("Battery is at low level"),
                           KIcon("dialog-warning").pixmap(20,20), new QWidget(), 15000);
            break;
    }
    
    if(state == Solid::Control::PowerManager::Critical && 
      Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Unplugged)
    {
        switch(PowerDevilSettings::batLowAction())
        {
            case Shutdown:
                shutdown();
                break;
            case S2Disk:
                Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
                break;
            case S2Ram:
                Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
                break;
            case Standby:
                Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
                break;
            default:
                break;
        }
    }
}

void PowerDevilDaemon::buttonPressed(int but)
{
    if(but == Solid::Control::PowerManager::LidClose)
    {
        if(Solid::Control::PowerManager::acAdapterState() == Solid::Control::PowerManager::Plugged)
        {
            switch(PowerDevilSettings::aCLidAction())
            {
                case Shutdown:
                    shutdown();
                    break;
                case S2Disk:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
                    break;
                case S2Ram:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
                    break;
                case Standby:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
                    break;
                case Lock:
                    //lockScreen();
                    break;
                default:
                    break;
            }
        }
        else
        {
            switch(PowerDevilSettings::batLidAction())
            {
                case Shutdown:
                    shutdown();
                    break;
                case S2Disk:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
                    break;
                case S2Ram:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
                    break;
                case Standby:
                    Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
                    break;
                case Lock:
                    //lockScreen();
                    break;
                default:
                    break;
            }
        }
    } 
}

void PowerDevilDaemon::decreaseBrightness()
{
    int currentBrightness = Solid::Control::PowerManager::brightness() - 10;
    
    if(currentBrightness < 0)
    {
        currentBrightness = 0;
    }
    
    //actionBrightnessSlider->setValue(currentBrightness);
    Solid::Control::PowerManager::setBrightness(currentBrightness);
}

void PowerDevilDaemon::increaseBrightness()
{
    int currentBrightness = Solid::Control::PowerManager::brightness() + 10;
    
    if(currentBrightness > 100)
    {
        currentBrightness = 100;
    }
    
    //actionBrightnessSlider->setValue(currentBrightness);
    Solid::Control::PowerManager::setBrightness(currentBrightness);
}


void PowerDevilDaemon::shutdown()
{
    m_displayManager->shutdown(KWorkSpace::ShutdownTypeHalt, KWorkSpace::ShutdownModeTryNow);
}

void PowerDevilDaemon::suspendToDisk()
{
    /*if(configLockScreen)
    {
        lockScreen();
    }*/
    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToDisk);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::suspendToRam()
{
    /*if(configLockScreen)
    {
        lockScreen();
    }*/
    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::ToRam);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::standby()
{
    /*if(configLockScreen)
    {
        lockScreen();
    }*/
    KJob * job = Solid::Control::PowerManager::suspend(Solid::Control::PowerManager::Standby);
    connect(job, SIGNAL(result(KJob *)), this, SLOT(suspendJobResult(KJob *)));
    job->start();
}

void PowerDevilDaemon::suspendJobResult(KJob * job)
{
    if(job->error())
    {
        KPassivePopup::message(KPassivePopup::Boxed, i18n("WARNING"), job->errorString(),
                           KIcon("dialog-warning").pixmap(20,20), dynamic_cast<QWidget *>(this), 15000);
    }
    //m_screenSaverIface->SimulateUserActivity(); //prevent infinite suspension loops
}

#include "PowerDevilDaemon.moc"
