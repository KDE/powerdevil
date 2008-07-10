#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QWidget>
#include "ui_dialog.h"

class ConfigWidget : public QWidget, private Ui_powerDevilConfig
{
      Q_OBJECT

public:
      ConfigWidget(QWidget *parent = 0);

      void fillUi();

      void load();
      void save();

private:
	enum IdleAction {
            Shutdown,
	    S2Disk, 
	    S2Ram, 
	    Standby, 
	    Lock, 
	    None
        };
};

#endif /*CONFIGWIDGET_H*/