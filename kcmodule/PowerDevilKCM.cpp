#include "PowerDevilKCM.h"

#include "ConfigWidget.h"

#include <kdemacros.h>
#include <KPluginFactory>
#include <KAboutData>
#include <klocalizedstring.h>

#include <QLayout>

K_PLUGIN_FACTORY(PowerDevilKCMFactory,
        registerPlugin<PowerDevilKCM>();
        )
K_EXPORT_PLUGIN(PowerDevilKCMFactory("kcmpowerdevil"))

PowerDevilKCM::PowerDevilKCM(QWidget *parent, const QVariantList &):
	KCModule(PowerDevilKCMFactory::componentData(), parent)
{
	QVBoxLayout *lay = new QVBoxLayout(this);
	lay->setMargin(0);

	m_widget = new ConfigWidget(this);
	lay->addWidget(m_widget);
	
	setButtons( Apply );
	
	connect(m_widget, SIGNAL(changed(bool)), SIGNAL(changed(bool)));

	//TODO: Add yourself to copyright here
	KAboutData *about =
	new KAboutData(I18N_NOOP("kcmpowerdevil"), 0, ki18n("PowerDevil Configuration"),
			0, KLocalizedString(), KAboutData::License_GPL,
			ki18n("(c), 2008 Dario Freddi"));

	about->addAuthor(ki18n("Dario Freddi"), KLocalizedString() , "drf@kdemod.ath.cx");
	setAboutData( about );
}

void PowerDevilKCM::load()
{
	m_widget->load();
}

void PowerDevilKCM::save()
{
	m_widget->save();
}

void PowerDevilKCM::defaults()
{
  
}

#include "PowerDevilKCM.moc"
