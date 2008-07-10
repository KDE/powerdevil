#ifndef CONFIGWIDGET_H
#define CONFIGWIDGET_H

#include <QWidget>
#include "ui_dialog.h"

class ConfigWidget : public QWidget, private Ui_powerDevilConfig
{
      Q_OBJECT

public:
      ConfigWidget(QWidget *parent = 0);
};

#endif /*CONFIGWIDGET_H*/