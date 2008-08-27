/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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

#include "ErrorView.h"
#include "IdleView.h"

#include <QtDBus/QDBusMessage>
#include <QGraphicsLinearLayout>
#include <QGraphicsProxyWidget>
#include <QPainter>
#include <QGraphicsSceneDragDropEvent>

#include <KIcon>
#include <KDebug>

#include <plasma/svg.h>
#include <plasma/applet.h>
#include <plasma/theme.h>
#include <plasma/dataengine.h>

#define TOP_MARGIN 0
#define MARGIN 20
#define SPACING 30

PowerDevilApplet::PowerDevilApplet(QObject *parent, const QVariantList &args)
        : Plasma::Applet(parent, args),
        m_dbus(QDBusConnection::systemBus())
{
    KGlobal::locale()->insertCatalog("powerdevil");

    setHasConfigurationInterface(false);
    setAspectRatioMode(Plasma::IgnoreAspectRatio);
    setBackgroundHints(Applet::DefaultBackground);

    //connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), this, SLOT(themeRefresh()));
}

PowerDevilApplet::~PowerDevilApplet()
{
    //m_engine->disconnectSource("Shaman", this);

    delete m_view;
}

void PowerDevilApplet::init()
{
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    m_layout = new QGraphicsLinearLayout();
    m_layout->setSpacing(SPACING);
    m_layout->setOrientation(Qt::Vertical);

    /*m_form = new QGraphicsWidget(this);
    m_form->setContentsMargins(MARGIN, TOP_MARGIN, MARGIN, MARGIN);
    m_form->setLayout(m_layout);*/

    if (formFactor() == Plasma::Vertical || formFactor() == Plasma::Horizontal) {
        m_layout->setContentsMargins(0, 0, 0, 0);
        setBackgroundHints(NoBackground);
    } else {
        m_layout->setContentsMargins(MARGIN, TOP_MARGIN, MARGIN, MARGIN);
        setMinimumSize(QSize(300, 300));
    }
    setLayout(m_layout);

    m_engine = dataEngine("powerdevil");

    if (m_engine) {
        m_engine->connectSource("PowerDevil", this);
        m_engine->setProperty("refreshTime", 2000);
    } else
        kDebug() << "PowerDevil Engine could not be loaded";

    setAcceptDrops(true);
}

/*QSizeF PowerDevilApplet::contentSizeHint() const
{
    return effectiveSizeHint(Qt::PreferredSize, geometry().size());
}

void PowerDevilApplet::constraintsEvent(Plasma::Constraints constraints)
{
    if (constraints & Plasma::SizeConstraint) {
        if (m_layout) {
            resize(geometry().size());
        }
    }
}*/

void PowerDevilApplet::dataUpdated(const QString &source, const Plasma::DataEngine::Data &data)
{
    kDebug() << "called";

    Q_UNUSED(source)

    m_error = data["error"].toBool();

    if (data["error"].toBool() && !m_error) {
        m_errorMessage = data["errorMessage"].toString();
        loadView(PowerDevilApplet::ErrorViewType);
    } else if (!data["error"].toBool()) {
        loadView(PowerDevilApplet::IdleType);
    }

    if (data["currentProfile"].toString() != m_currentProfile
            || data["availableProfiles"].toStringList() != m_availableProfiles) {
        m_currentProfile = data["currentProfile"].toString();
        m_availableProfiles = data["availableProfiles"].toStringList();

        emit profilesChanged(m_currentProfile, m_availableProfiles);
    }

    emit batteryStatusChanged(data["batteryChargePercent"].toInt(), data["ACPlugged"].toBool());
}

void PowerDevilApplet::loadView(uint type)
{
    if (type != m_viewType) {
        if (m_view) {
            kDebug() << "Deleting old view";
            delete m_view;
        }

        kDebug() << "Creating new View";

        switch (type) {
        case PowerDevilApplet::ErrorViewType :
            m_view = new ErrorView(this, m_errorMessage);
            break;
        case PowerDevilApplet::IdleType :

        default:
            m_view = new IdleView(this, m_dbus);

            m_currentProfile.clear();
            m_availableProfiles.clear();

            connect(this, SIGNAL(batteryStatusChanged(int, bool)), m_view, SLOT(updateBatteryIcon(int, bool)));
            connect(this, SIGNAL(profilesChanged(const QString&, const QStringList&)),
                    m_view, SLOT(updateProfiles(const QString&, const QStringList&)));
            break;
        }

        kDebug() << "New View Created";

        m_viewType = type;

        kDebug() << "Updating Geometry...";

        m_layout->updateGeometry();

        kDebug() << "Done.";
    }
}

void PowerDevilApplet::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)

    kDebug() << "Drop happened";
}

#include "PowerDevilApplet.moc"
