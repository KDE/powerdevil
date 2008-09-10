/***************************************************************************
 *   Copyright (C) 2007-2008 by Sebastian Kuegler <sebas@kde.org>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#ifndef BATTERY_H
#define BATTERY_H

#include <QLabel>
#include <QGraphicsSceneHoverEvent>
#include <QPair>
#include <QMap>

#include <plasma/applet.h>
#include <plasma/animator.h>
#include <plasma/dataengine.h>
#include "ui_batteryConfig.h"

namespace Plasma
{
class Svg;
}

class Battery : public Plasma::Applet
{
        Q_OBJECT
    public:
        Battery( QObject *parent, const QVariantList &args );
        ~Battery();

        void init();
        void paintInterface( QPainter *painter, const QStyleOptionGraphicsItem *option,
                             const QRect &contents );
        void setPath( const QString& );
        QSizeF sizeHint( const Qt::SizeHint which, const QSizeF& constraint ) const;
        Qt::Orientations expandingDirections() const;

        void constraintsEvent( Plasma::Constraints constraints );

    public slots:
        void dataUpdated( const QString &name, const Plasma::DataEngine::Data &data );

    protected Q_SLOTS:
        virtual void hoverEnterEvent( QGraphicsSceneHoverEvent *event );
        virtual void hoverLeaveEvent( QGraphicsSceneHoverEvent *event );
        void configAccepted();
        void readColors();

    protected:
        void createConfigurationInterface( KConfigDialog *parent );

    private slots:
        void animationUpdate( qreal progress );
        void acAnimationUpdate( qreal progress );
        void batteryAnimationUpdate( qreal progress );
        void sourceAdded( const QString &source );
        void sourceRemoved( const QString &source );

    signals:
        void sizeHintChanged( Qt::SizeHint hint );

    private:
        Q_ENUMS( m_batteryStyle )
        enum ClockStyle {
            // Keep the order of styles the same order as the items in the configdialog!
            OxygenBattery, ClassicBattery
        };
        void connectSources();
        void disconnectSources();
        int m_batteryStyle;
        /* Paint battery with proper charge level */
        void paintBattery( QPainter *p, const QRect &contentsRect, const int batteryPercent, const bool plugState );
        /* Paint a label on top of the battery */
        void paintLabel( QPainter *p, const QRect &contentsRect, const QString& labelText );
        /* Fade in/out the label above the battery. */
        void showLabel( bool show );
        /* Scale in/out Battery. */
        void showBattery( bool show );
        /* Scale in/out Ac Adapter. */
        void showAcAdapter( bool show );
        /* Scale in a QRectF */
        QRectF scaleRectF( qreal progress, QRectF rect );
        /* Show multiple batteries with individual icons and charge info? */
        bool m_showMultipleBatteries;
        /* Should the battery charge information be shown on top? */
        bool m_showBatteryString;
        QSizeF m_size;
        int m_pixelSize;
        Plasma::Svg* m_theme;
        bool m_acadapter_plugged;

        // Configuration dialog
        Ui::batteryConfig ui;

        int m_animId;
        qreal m_alpha;
        bool m_fadeIn;

        int m_acAnimId;
        qreal m_acAlpha;
        bool m_acFadeIn;

        int m_batteryAnimId;
        qreal m_batteryAlpha;
        bool m_batteryFadeIn;

        // Internal data
        QList<QVariant> batterylist, acadapterlist;
        QHash<QString, QHash<QString, QVariant> > m_batteries_data;
        QFont m_font;
        bool m_isHovered;
        QColor m_boxColor;
        QColor m_textColor;
        QRectF m_textRect;
        int m_boxAlpha;
        int m_boxHoverAlpha;
        int m_numOfBattery;
};

K_EXPORT_PLASMA_APPLET( battery, Battery )

#endif
