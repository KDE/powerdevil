#ifndef POWERDEVILKCM_H
#define POWERDEVILKCM_H

#include <kcmodule.h>

class PowerDevilKCM : public KCModule
{
	Q_OBJECT
	
  public:
	PowerDevilKCM(QWidget *parent, const QVariantList &args);

	void load();
	void save();
	void defaults();
};

#endif /*POWERDEVILKCM_H*/