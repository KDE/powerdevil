#ifndef POWERDEVILDAEMON_H
#define POWERDEVILDAEMON_H

#include <kdedmodule.h>
#include <solid/control/powermanager.h>

class KDisplayManager;

class KDE_EXPORT PowerDevilDaemon : public KDEDModule
{
    Q_OBJECT
    
public:
	PowerDevilDaemon(QObject *parent, const QList<QVariant>&);
	virtual ~PowerDevilDaemon();

private slots:
	void acAdapterStateChanged(int state);
	void batteryStateChanged(int state);
	void decreaseBrightness();
        void increaseBrightness();
	void shutdown();
        void suspendJobResult(KJob * job);
        void suspendToDisk();
        void suspendToRam();
        void standby();
	void buttonPressed(int but);

private:
	enum IdleAction {
            Shutdown,
	    S2Disk, 
	    S2Ram, 
	    Standby, 
	    Lock, 
	    None
        };
	
	Solid::Control::PowerManager::Notifier * m_notifier;
	KDisplayManager * m_displayManager;
};

#endif /*POWERDEVILDAEMON_H*/
