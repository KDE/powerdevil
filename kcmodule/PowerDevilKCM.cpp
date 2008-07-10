#include "PowerDevilKCM.h"

#include <kdemacros.h>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KPassivePopup>
#include <KIcon>
#include <klocalizedstring.h>

K_PLUGIN_FACTORY(PowerDevilKCMFactory,
        registerPlugin<PowerDevilKCM>();
        )
K_EXPORT_PLUGIN(PowerDevilKCMFactory("kcmpowerdevil"))

PowerDevilKCM::PowerDevilKCM(QWidget *parent, const QVariantList &):
	KCModule(PowerDevilKCMFactory::componentData(), parent)
{

}

void PowerDevilKCM::load()
{
  
}

void PowerDevilKCM::save()
{
  
}

void PowerDevilKCM::defaults()
{
  
}

#include "PowerDevilKCM.moc"
