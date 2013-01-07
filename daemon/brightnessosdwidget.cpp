/*******************************************************************
* brightnessosdwidget.cpp
* adapted from kdemultimedia/kmix/osdwidget.cpp
* Copyright  2009    Aurélien Gâteau <agateau@kde.org>
* Copyright  2009    Dario Andres Rodriguez <andresbajotierra@gmail.com>
* Copyright  2009    Christian Esken <christian.esken@arcor.de>
* Copyright  2010    Felix Geyer <debfx-kde@fobos.de>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License as
* published by the Free Software Foundation; either version 2 of
* the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************/

#include "brightnessosdwidget.h"

// Qt
#include <QGraphicsLinearLayout>
#include <QPainter>
#include <QTimer>
#include <QLabel>

// KDE
#include <KIcon>
#include <KDialog>
#include <KWindowSystem>
#include <Plasma/Label>
#include <Plasma/Meter>
#include <Plasma/Theme>

BrightnessOSDWidget::BrightnessOSDWidget(QWidget * parent)
    : Plasma::Dialog(parent, Qt::ToolTip),
      m_scene(new QGraphicsScene(this)),
      m_container(new QGraphicsWidget),
      m_iconLabel(new Plasma::Label),
      m_volumeLabel(new Plasma::Label),
      m_meter(new Plasma::Meter),
      m_hideTimer(new QTimer(this))
{
    KWindowSystem::setState(winId(), NET::KeepAbove);
    KWindowSystem::setType(winId(), NET::Tooltip);
    setAttribute(Qt::WA_X11NetWmWindowTypeToolTip, true);
    m_meter->setMeterType(Plasma::Meter::BarMeterHorizontal);
    m_meter->setMaximum(100);

    m_volumeLabel->setAlignment(Qt::AlignCenter);

    //Setup the auto-hide timer
    m_hideTimer->setInterval(2000);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, SIGNAL(timeout()), this, SLOT(hide()));

    //Setup the OSD layout
    QGraphicsLinearLayout *layout = new QGraphicsLinearLayout(m_container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addItem(m_iconLabel);
    layout->addItem(m_meter);
    layout->addItem(m_volumeLabel);

    m_scene->addItem(m_container);
    setGraphicsWidget(m_container);

    themeUpdated();
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(themeUpdated())); // e.g. for updating font
}

void BrightnessOSDWidget::activateOSD()
{
    m_hideTimer->start();
}

void BrightnessOSDWidget::setCurrentBrightness(int brightnessLevel)
{
    m_meter->setValue(brightnessLevel);
    m_volumeLabel->setText(QString::number(brightnessLevel) + " %");
}

void BrightnessOSDWidget::themeUpdated()
{
    //Set a font which makes the text appear as big (height-wise) as the meter.
    //QFont font = QFont(m_volumeLabel->nativeWidget()->font());
    Plasma::Theme* theme = Plasma::Theme::defaultTheme();


    QPalette palette = m_volumeLabel->palette();
    palette.setColor(QPalette::WindowText, theme->color(Plasma::Theme::TextColor));
    m_volumeLabel->setPalette(palette);

    QFont font = theme->font(Plasma::Theme::DefaultFont);
    font.setPointSize(15);
    m_volumeLabel->setFont(font);
    QFontMetrics qfm(font);
    QRect textSize = qfm.boundingRect("100 %  ");

    int widthHint = textSize.width();
    int heightHint = textSize.height();
    //setCurrentVolume(100,false);
    m_volumeLabel->setMaximumHeight(heightHint);
    m_volumeLabel->setMinimumWidth(widthHint);
    m_volumeLabel->nativeWidget()->setFixedWidth(widthHint);

    //Cache the icon pixmaps
    QFontMetrics fm(m_volumeLabel->font());
    QSize iconSize = QSize(fm.height(), fm.height());

    m_brightnessPixmap = KIcon("video-display").pixmap(iconSize);

    m_iconLabel->nativeWidget()->setPixmap(m_brightnessPixmap);
    m_iconLabel->nativeWidget()->setFixedSize(iconSize);
    m_iconLabel->setMinimumSize(iconSize);
    m_iconLabel->setMaximumSize(iconSize);

    m_meter->setMaximumHeight(iconSize.height());
    m_container->setMinimumSize(iconSize.width() * 13 + m_volumeLabel->nativeWidget()->width(), iconSize.height());
    m_container->setMaximumSize(iconSize.width() * 13 + m_volumeLabel->nativeWidget()->width(), iconSize.height());

    syncToGraphicsWidget();
}


#include "brightnessosdwidget.moc"
