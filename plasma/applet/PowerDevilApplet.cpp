/***************************************************************************
 *   Copyright (C) 2007-2008 by Riccardo Iaconelli <riccardo@kde.org>      *
 *   Copyright (C) 2007-2008 by Sebastian Kuegler <sebas@kde.org>          *
 *   Copyright (C) 2007 by Luka Renko <lure@kubuntu.org>                   *
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

#include "PowerDevilApplet.h"

#include <QApplication>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QFont>
#include <QGraphicsSceneHoverEvent>
#include <QProcess>

#include <KDebug>
#include <KIcon>
#include <KSharedConfig>
#include <KDialog>
#include <KColorScheme>
#include <KConfigDialog>
#include <KGlobalSettings>

#include <plasma/svg.h>
#include <plasma/theme.h>
#include <plasma/animator.h>

PowerDevilApplet::PowerDevilApplet( QObject *parent, const QVariantList &args )
        : Plasma::Applet( parent, args ),
        m_batteryStyle( 0 ),
        m_theme( 0 ),
        m_animId( -1 ),
        m_alpha( 1 ),
        m_fadeIn( false ),
        m_acAnimId( -1 ),
        m_acAlpha( 1 ),
        m_acFadeIn( false ),
        m_batteryAnimId( -1 ),
        m_batteryAlpha( 1 ),
        m_batteryFadeIn( true ),
        m_isHovered( false ),
        m_numOfBattery( 0 )
{
    kDebug() << "Loading applet battery";
    setAcceptsHoverEvents( true );
    setHasConfigurationInterface( true );
    resize( 128, 128 );
    setAspectRatioMode( Plasma::ConstrainedSquare );
    m_textRect = QRect();
    m_theme = new Plasma::Svg( this );

    QAction *configurePowerdevil = new QAction( KIcon( "dialog-ok-apply" ), i18n( "Configure PowerDevil" ), 0 );
    connect( configurePowerdevil, SIGNAL( triggered() ), this, SLOT( openPowerDevilConfiguration() ) );
    addAction( i18n( "Configure PowerDevil" ), configurePowerdevil );
}

void PowerDevilApplet::init()
{
    KConfigGroup cg = config();
    m_showBatteryString = cg.readEntry( "showBatteryString", false );
    m_showMultipleBatteries = cg.readEntry( "showMultipleBatteries", true );

    QString svgFile;
    if ( cg.readEntry( "style", 0 ) == 0 ) {
        m_batteryStyle = OxygenBattery;
        svgFile = "widgets/battery-oxygen";
    } else {
        m_batteryStyle = ClassicBattery;
        svgFile = "widgets/battery";
    }
    m_theme->setImagePath( svgFile );
    m_theme->setContainsMultipleImages( false );

    m_theme->resize( contentsRect().size() );
    m_font = QApplication::font();
    m_font.setWeight( QFont::Bold );

    m_boxAlpha = 128;
    m_boxHoverAlpha = 192;

    readColors();
    connect( Plasma::Theme::defaultTheme(), SIGNAL( themeChanged() ), SLOT( readColors() ) );

    const QStringList& battery_sources = dataEngine( "powermanagement" )->query( I18N_NOOP( "Battery" ) )[I18N_NOOP( "sources" )].toStringList();

    //connect sources
    connectSources();

    foreach( const QString &battery_source, battery_sources ) {
        kDebug() << "BatterySource:" << battery_source;
        dataUpdated( battery_source, dataEngine( "powermanagement" )->query( battery_source ) );
    }
    dataUpdated( I18N_NOOP( "AC Adapter" ), dataEngine( "powermanagement" )->query( I18N_NOOP( "AC Adapter" ) ) );
    m_numOfBattery = battery_sources.size();
    kDebug() << battery_sources.size();
}

void PowerDevilApplet::constraintsEvent( Plasma::Constraints constraints )
{
    if ( constraints & ( Plasma::FormFactorConstraint | Plasma::SizeConstraint ) ) {
        if ( formFactor() == Plasma::Vertical ) {
            if ( !m_showMultipleBatteries ) {
                setMaximumSize( QWIDGETSIZE_MAX, qMax( m_textRect.height(), contentsRect().width() ) );
            } else {
                setMaximumSize( QWIDGETSIZE_MAX, qMax( m_textRect.height(), contentsRect().width() *m_numOfBattery ) );
            }
            //kDebug() << "Vertical FormFactor";
        } else if ( formFactor() == Plasma::Horizontal ) {
            if ( !m_showMultipleBatteries ) {
                setMaximumSize( qMax( m_textRect.width(), contentsRect().height() ), QWIDGETSIZE_MAX );
            } else {
                setMaximumSize( qMax( m_textRect.width(), contentsRect().height() *m_numOfBattery ), QWIDGETSIZE_MAX );
            }
            //kDebug() << "Horizontal FormFactor" << m_textRect.width() << contentsRect().height();
        }
    }
    if ( !m_showMultipleBatteries || m_numOfBattery < 2 ) {
        setAspectRatioMode( Plasma::Square );
    } else {
        setAspectRatioMode( Plasma::KeepAspectRatio );
    }

    if ( constraints & ( Plasma::SizeConstraint | Plasma::FormFactorConstraint ) && m_theme ) {
        m_theme->resize( contentsRect().size().toSize() );
        m_font.setPointSize( qMax( KGlobalSettings::smallestReadableFont().pointSize(),
                                   qRound( contentsRect().height() / 10 ) ) );
    }
}


QSizeF PowerDevilApplet::sizeHint( const Qt::SizeHint which, const QSizeF& constraint ) const
{
    Q_UNUSED( which );
    Q_UNUSED( constraint );
    QSizeF sizeHint = contentsRect().size();
    switch ( formFactor() ) {
    case Plasma::Vertical:
        sizeHint.setHeight( sizeHint.width() * qMax( 1, m_numOfBattery ) );
        break;

    default:
        sizeHint.setWidth( sizeHint.height() * qMax( 1, m_numOfBattery ) );
        break;
    }
    return sizeHint;
}


void PowerDevilApplet::dataUpdated( const QString& source, const Plasma::DataEngine::Data &data )
{
    if ( source.startsWith( I18N_NOOP( "Battery" ) ) ) {
        m_batteries_data[source] = data;
    } else if ( source == I18N_NOOP( "AC Adapter" ) ) {
        m_acadapter_plugged = data[I18N_NOOP( "Plugged in" )].toBool();
        showAcAdapter( m_acadapter_plugged );
    } else if ( source == I18N_NOOP( "PowerDevil" ) ) {
        m_availableProfiles = data[I18N_NOOP( "availableProfiles" )].toStringList();
        m_currentProfile = data[I18N_NOOP( "currentProfile" )].toString();
    } else {
        kDebug() << "Applet : Dunno what to do with " << source;
    }
    update();
}

void PowerDevilApplet::createConfigurationInterface( KConfigDialog *parent )
{
    QWidget *widget = new QWidget( parent );
    ui.setupUi( widget );
    parent->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply );
    parent->addPage( widget, parent->windowTitle(), icon() );
    connect( parent, SIGNAL( applyClicked() ), this, SLOT( configAccepted() ) );
    connect( parent, SIGNAL( okClicked() ), this, SLOT( configAccepted() ) );
    ui.styleGroup->setSelected( m_batteryStyle );
    ui.showBatteryStringCheckBox->setChecked( m_showBatteryString ? Qt::Checked : Qt::Unchecked );
    ui.showMultipleBatteriesCheckBox->setChecked( m_showMultipleBatteries ? Qt::Checked : Qt::Unchecked );
}

void PowerDevilApplet::configAccepted()
{
    KConfigGroup cg = config();

    if ( m_showBatteryString != ui.showBatteryStringCheckBox->isChecked() ) {
        m_showBatteryString = !m_showBatteryString;
        cg.writeEntry( "showBatteryString", m_showBatteryString );
        showLabel( m_showBatteryString );
    }

    if ( m_showMultipleBatteries != ui.showMultipleBatteriesCheckBox->isChecked() ) {
        m_showMultipleBatteries = !m_showMultipleBatteries;
        cg.writeEntry( "showMultipleBatteries", m_showMultipleBatteries );
        kDebug() << "Show multiple battery changed: " << m_showMultipleBatteries;
        emit sizeHintChanged( Qt::PreferredSize );
    }

    if ( ui.styleGroup->selected() != m_batteryStyle ) {
        QString svgFile;
        if ( ui.styleGroup->selected() == OxygenBattery ) {
            svgFile = "widgets/battery-oxygen";
        } else {
            svgFile = "widgets/battery";
        }
        if ( m_acadapter_plugged ) {
            showAcAdapter( false );
        }
        showBattery( false );
        m_batteryStyle = ui.styleGroup->selected();
        delete m_theme;
        m_theme = new Plasma::Svg( this );
        m_theme->setImagePath( svgFile );
        kDebug() << "Changing theme to " << svgFile;
        cg.writeEntry( "style", m_batteryStyle );
        m_theme->resize( contentsRect().size() );
        if ( m_acadapter_plugged ) {
            showAcAdapter( true );
        }
        showBattery( true );
    }

    //reconnect sources
    disconnectSources();
    connectSources();

    emit configNeedsSaving();
}

void PowerDevilApplet::readColors()
{
    m_textColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::TextColor );
    m_boxColor = Plasma::Theme::defaultTheme()->color( Plasma::Theme::BackgroundColor );
    m_boxColor.setAlpha( m_boxAlpha );
}

void PowerDevilApplet::hoverEnterEvent( QGraphicsSceneHoverEvent *event )
{
    showLabel( true );
    //showAcAdapter(false); // to test the animation without constant plugging
    //showBattery(false); // to test the animation without constant plugging
    m_isHovered = true;
    Applet::hoverEnterEvent( event );
}

void PowerDevilApplet::hoverLeaveEvent( QGraphicsSceneHoverEvent *event )
{
    if ( !m_showBatteryString ) {
        showLabel( false );
    }
    //showAcAdapter(true); // to test the animation without constant plugging
    //showBattery(true); // to test the animation without constant plugging
    //m_isHovered = false;
    Applet::hoverLeaveEvent( event );
}

PowerDevilApplet::~PowerDevilApplet()
{
}

void PowerDevilApplet::showLabel( bool show )
{
    if ( m_fadeIn == show ) {
        return;
    }
    m_fadeIn = show;
    const int FadeInDuration = 150;

    if ( m_animId != -1 ) {
        Plasma::Animator::self()->stopCustomAnimation( m_animId );
    }
    m_animId = Plasma::Animator::self()->customAnimation( 40 / ( 1000 / FadeInDuration ), FadeInDuration,
               Plasma::Animator::EaseOutCurve, this,
               "animationUpdate" );
}

QRectF PowerDevilApplet::scaleRectF( const qreal progress, QRectF rect )
{
    if ( progress == 1 ) {
        return rect;
    }
    // Scale
    qreal w = rect.width() * progress;
    qreal h = rect.width() * progress;

    // Position centered
    rect.setX(( rect.width() - w ) / 2 );
    rect.setY(( rect.height() - h ) / 2 );

    rect.setWidth( w );
    rect.setHeight( h );

    return rect;
}

void PowerDevilApplet::showAcAdapter( bool show )
{
    if ( m_acFadeIn == show ) {
        return;
    }
    m_acFadeIn = show;
    const int FadeInDuration = 300;
    // As long as the animation is running, we fake it's still plugged in so it gets
    // painted in paintInterface()
    m_acadapter_plugged = true;

    if ( m_acAnimId != -1 ) {
        Plasma::Animator::self()->stopCustomAnimation( m_acAnimId );
    }
    m_acAnimId = Plasma::Animator::self()->customAnimation( 40 / ( 1000 / FadeInDuration ), FadeInDuration,
                 Plasma::Animator::EaseOutCurve, this,
                 "acAnimationUpdate" );
}

void PowerDevilApplet::showBattery( bool show )
{
    if ( m_batteryFadeIn == show ) {
        return;
    }
    m_batteryFadeIn = show;
    const int FadeInDuration = 300;

    if ( m_batteryAnimId != -1 ) {
        Plasma::Animator::self()->stopCustomAnimation( m_batteryAnimId );
    }
    m_batteryAnimId = Plasma::Animator::self()->customAnimation( 40 / ( 1000 / FadeInDuration ), FadeInDuration,
                      Plasma::Animator::EaseOutCurve, this,
                      "batteryAnimationUpdate" );
}

void PowerDevilApplet::animationUpdate( qreal progress )
{
    if ( progress == 1 ) {
        m_animId = -1;
    }
    if ( !m_fadeIn ) {
        qreal new_alpha = m_fadeIn ? progress : 1 - progress;
        m_alpha = qMin( new_alpha, m_alpha );
    } else {
        m_alpha = m_fadeIn ? progress : 1 - progress;
    }
    m_alpha = qMax( qreal( 0.0 ), m_alpha );
    update();
}

void PowerDevilApplet::acAnimationUpdate( qreal progress )
{
    if ( progress == 1 ) {
        m_acAnimId = -1;
    }
    m_acAlpha = m_acFadeIn ? progress : 1 - progress;
    // During the fadeout animation, we had set it to true (and lie)
    // now the animation has ended, we _really_ set it to not show the adapter
    if ( !m_acFadeIn && ( progress == 1 ) ) {
        m_acadapter_plugged = false;
    }
    update();
}

void PowerDevilApplet::batteryAnimationUpdate( qreal progress )
{
    if ( progress == 1 ) {
        m_batteryAnimId = -1;
    }
    m_batteryAlpha = m_batteryFadeIn ? progress : 1 - progress;
    update();
}

void PowerDevilApplet::paintLabel( QPainter *p, const QRect &contentsRect, const QString& labelText )
{
    // Store font size, we want to restore it shortly
    int original_font_size = m_font.pointSize();

    // Fonts smaller than smallestReadableFont don't make sense.
    m_font.setPointSize( qMax( KGlobalSettings::smallestReadableFont().pointSize(), m_font.pointSize() ) );
    QFontMetrics fm( m_font );
    qreal text_width = fm.width( labelText );

    // Longer texts get smaller fonts
    if ( labelText.length() > 4 ) {
        if ( original_font_size / 1.5 < KGlobalSettings::smallestReadableFont().pointSize() ) {
            m_font.setPointSize(( KGlobalSettings::smallestReadableFont().pointSize() ) );
        } else {
            m_font.setPointSizeF( original_font_size / 1.5 );
        }
        fm = QFontMetrics( m_font );
        text_width = ( fm.width( labelText ) * 1.2 );
    } else {
        // Smaller texts get a wider box
        text_width = ( text_width * 1.4 );
    }
    if ( formFactor() == Plasma::Horizontal ||
            formFactor() == Plasma::Vertical ) {
        m_font = KGlobalSettings::smallestReadableFont();
        m_font.setWeight( QFont::Bold );
        fm = QFontMetrics( m_font );
        text_width = ( fm.width( labelText ) + 8 );
    }
    p->setFont( m_font );

    // Let's find a good position for painting the background
    m_textRect = QRectF( qMax( qreal( 0.0 ), contentsRect.left() + ( contentsRect.width() - text_width ) / 2 ),
                         contentsRect.top() + (( contentsRect.height() - ( int ) fm.height() ) / 2 * 0.9 ),
                         qMin( contentsRect.width(), ( int ) text_width ),
                         fm.height() * 1.2 );

    // Poor man's highlighting
    m_boxColor.setAlphaF( m_alpha );
    p->setPen( m_boxColor );
    m_boxColor.setAlphaF( m_alpha*0.5 );
    p->setBrush( m_boxColor );

    // Find sensible proportions for the rounded corners
    float round_prop = m_textRect.width() / m_textRect.height();

    // Tweak the rounding edge a bit with the proportions of the textbox
    qreal round_radius = 35.0;
    p->drawRoundedRect( m_textRect, round_radius / round_prop, round_radius, Qt::RelativeSize );

    m_textColor.setAlphaF( m_alpha );
    p->setPen( m_textColor );
    p->drawText( m_textRect, Qt::AlignCenter, labelText );

    // Reset font and box
    m_font.setPointSize( original_font_size );
    m_boxColor.setAlpha( m_boxAlpha );
}

void PowerDevilApplet::paintBattery( QPainter *p, const QRect &contentsRect, const int batteryPercent, const bool plugState )
{
    QString fill_element;
    if ( plugState && m_theme->hasElement( "Battery" ) ) {
        m_theme->paint( p, scaleRectF( m_batteryAlpha, contentsRect ), "Battery" );

        if ( m_batteryStyle == OxygenBattery ) {
            if ( batteryPercent > 95 ) {
                fill_element = "Fill100";
            } else if ( batteryPercent > 80 ) {
                fill_element = "Fill80";
            } else if ( batteryPercent > 50 ) {
                fill_element = "Fill60";
            } else if ( batteryPercent > 20 ) {
                fill_element = "Fill40";
            } else if ( batteryPercent > 10 ) {
                fill_element = "Fill20";
            } // Don't show a fillbar below 11% charged
        } else { // OxyenStyle
            if ( batteryPercent > 95 ) {
                fill_element = "Fill100";
            } else if ( batteryPercent > 90 ) {
                fill_element = "Fill90";
            } else if ( batteryPercent > 80 ) {
                fill_element = "Fill80";
            } else if ( batteryPercent > 70 ) {
                fill_element = "Fill70";
            } else if ( batteryPercent > 55 ) {
                fill_element = "Fill60";
            } else if ( batteryPercent > 40 ) {
                fill_element = "Fill50";
            } else if ( batteryPercent > 30 ) {
                fill_element = "Fill40";
            } else if ( batteryPercent > 20 ) {
                fill_element = "Fill30";
            } else if ( batteryPercent > 10 ) {
                fill_element = "Fill20";
            } else if ( batteryPercent >= 5 ) {
                fill_element = "Fill10";
            } // Lower than 5%? Show no fillbar.
        }
    }
    //kDebug() << "plugState:" << plugState;

    // Now let's find out which fillstate to show
    if ( plugState && !fill_element.isEmpty() ) {
        if ( m_theme->hasElement( fill_element ) ) {
            m_theme->paint( p, scaleRectF( m_batteryAlpha, contentsRect ), fill_element );
        } else {
            kDebug() << fill_element << " does not exist in svg";
        }
    }

    if ( m_acadapter_plugged ) {
        //QRectF ac_rect = QRectF(contentsRect.topLeft(), QSizeF(contentsRect.width()*m_acAlpha, contentsRect.height()*m_acAlpha));
        m_theme->paint( p, scaleRectF( m_acAlpha, contentsRect ), "AcAdapter" );
    }

    // For small FormFactors, we're drawing a shadow
    if ( formFactor() == Plasma::Vertical ||
            formFactor() == Plasma::Horizontal ) {
        if ( plugState ) {
            m_theme->paint( p, contentsRect, "Shadow" );
        }
    }
    if ( plugState && m_theme->hasElement( "Overlay" ) ) {
        m_theme->paint( p, scaleRectF( m_batteryAlpha, contentsRect ), "Overlay" );
    }
}

void PowerDevilApplet::paintInterface( QPainter *p, const QStyleOptionGraphicsItem *option, const QRect &contentsRect )
{
    Q_UNUSED( option );

    p->setRenderHint( QPainter::SmoothPixmapTransform );
    p->setRenderHint( QPainter::Antialiasing );

    if ( m_numOfBattery == 0 ) {
        QRectF ac_contentsRect( contentsRect.topLeft(), QSizeF( qMax( qreal( 0.0 ), contentsRect.width() * m_acAlpha ), qMax( qreal( 0.0 ), contentsRect.height() * m_acAlpha ) ) );
        if ( m_acadapter_plugged ) {
            m_theme->paint( p, ac_contentsRect, "AcAdapter" );
        }
        // Show that there's no battery
        paintLabel( p, contentsRect, I18N_NOOP( "n/a" ) );
        return;
    }

    if ( m_showMultipleBatteries ) {
        // paint each battery with own charge level
        int battery_num = 0;
        int width = contentsRect.width() / m_numOfBattery;
        QHashIterator<QString, QHash<QString, QVariant > > battery_data( m_batteries_data );
        while ( battery_data.hasNext() ) {
            battery_data.next();
            QRect corect = QRect( contentsRect.left() + battery_num * width,
                                  contentsRect.top(),
                                  width, contentsRect.height() );

            // paint battery with appropriate charge level
            paintBattery( p, corect, battery_data.value()[I18N_NOOP( "Percent" )].toInt(), battery_data.value()[I18N_NOOP( "Plugged in" )].toBool() );

            if ( m_showBatteryString || m_isHovered ) {
                // Show the charge percentage with a box on top of the battery
                QString batteryLabel;
                if ( battery_data.value()[I18N_NOOP( "Plugged in" )].toBool() ) {
                    batteryLabel = battery_data.value()[I18N_NOOP( "Percent" )].toString();
                    batteryLabel.append( "%" );
                } else {
                    batteryLabel = I18N_NOOP( "n/a" );
                }
                paintLabel( p, corect, batteryLabel );
            }
            ++battery_num;
        }
    } else {
        // paint only one battery and show cumulative charge level
        int battery_num = 0;
        int battery_charge = 0;
        bool has_battery = false;
        QHashIterator<QString, QHash<QString, QVariant > > battery_data( m_batteries_data );
        while ( battery_data.hasNext() ) {
            battery_data.next();
            if ( battery_data.value()[I18N_NOOP( "Plugged in" )].toBool() ) {
                battery_charge += battery_data.value()[I18N_NOOP( "Percent" )].toInt();
                has_battery = true;
                ++battery_num;
            }
        }
        if ( battery_num > 0 ) {
            battery_charge = battery_charge / battery_num;
        }
        // paint battery with appropriate charge level
        paintBattery( p, contentsRect,  battery_charge, has_battery );
        if ( m_showBatteryString || m_isHovered ) {
            // Show the charge percentage with a box on top of the battery
            QString batteryLabel;
            if ( has_battery ) {
                batteryLabel = QString::number( battery_charge );
                batteryLabel.append( "%" );
            } else {
                batteryLabel = I18N_NOOP( "n/a" );
            }
            paintLabel( p, contentsRect, batteryLabel );
        }
    }
}

void PowerDevilApplet::connectSources()
{
    const QStringList& battery_sources = dataEngine( "powermanagement" )->query( I18N_NOOP( "Battery" ) )[I18N_NOOP( "sources" )].toStringList();

    foreach( const QString &battery_source, battery_sources ) {
        dataEngine( "powermanagement" )->connectSource( battery_source, this );
    }

    dataEngine( "powermanagement" )->connectSource( I18N_NOOP( "AC Adapter" ), this );

    //dataEngine("powerdevil")->connectSource(I18N_NOOP("PowerDevil"), this);

    connect( dataEngine( "powermanagement" ), SIGNAL( sourceAdded( QString ) ),
             this,                          SLOT( sourceAdded( QString ) ) );
    connect( dataEngine( "powermanagement" ), SIGNAL( sourceRemoved( QString ) ),
             this,                          SLOT( sourceRemoved( QString ) ) );
}

void PowerDevilApplet::disconnectSources()
{
    const QStringList& battery_sources = dataEngine( "powermanagement" )->query( I18N_NOOP( "Battery" ) )[I18N_NOOP( "sources" )].toStringList();

    foreach( const QString &battery_source , battery_sources ) {
        dataEngine( "powermanagement" )->disconnectSource( battery_source, this );
    }

    dataEngine( "powermanagement" )->disconnectSource( I18N_NOOP( "AC Adapter" ), this );
    dataEngine( "powerdevil" )->disconnectSource( I18N_NOOP( "PowerDevil" ), this );

    disconnect( SLOT( sourceAdded( QString ) ) );
    disconnect( SLOT( sourceRemoved( QString ) ) );
}

void PowerDevilApplet::sourceAdded( const QString& source )
{
    if ( source.startsWith( "Battery" ) && source != "Battery" ) {
        dataEngine( "powermanagement" )->connectSource( source, this );
        m_numOfBattery++;
    }
}

void PowerDevilApplet::sourceRemoved( const QString& source )
{
    if ( m_batteries_data.contains( source ) ) {
        m_batteries_data.remove( source );
        m_numOfBattery--;
        update();
    }
}

void PowerDevilApplet::openPowerDevilConfiguration()
{
    QProcess::startDetached( "kcmshell4 powerdevilconfig" );
}

#include "PowerDevilApplet.moc"
