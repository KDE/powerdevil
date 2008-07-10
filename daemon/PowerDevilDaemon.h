#ifndef POWERDEVILDAEMON_H
#define POWERDEVILDAEMON_H

#include <kdedmodule.h>

#include <solid/control/powermanager.h>

class PowerDevilDaemon : public KDEDModule
{
    Q_OBJECT
    
public:
	PowerDevilDaemon(QObject *parent, const QList<QVariant>&);
	virtual ~PowerDevilDaemon();

private slots:
	void acAdapterStateChanged(int state);

private:
	Solid::Control::PowerManager::Notifier * m_notifier;
	
};

#endif /*POWERDEVILDAEMON_H*/
