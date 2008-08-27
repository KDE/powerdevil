/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi                                    *
 *   drf54321@yahoo.it                                                     *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#include "IdleView.h"

#include <QAction>
#include <QtDBus/QDBusMessage>
#include <QGraphicsLinearLayout>
#include <QGraphicsProxyWidget>

#include <KPushButton>
#include <KComboBox>
#include <KIcon>
#include <KDebug>

#include <plasma/widgets/icon.h>

IdleView::IdleView(Plasma::Applet *parent, QDBusConnection dbs)
        : AbstractView(0),
        m_dbus(dbs)
{
    m_layout = static_cast <QGraphicsLinearLayout *>(parent->layout());

    if (m_layout) {

        m_batteryIcon = new Plasma::Icon(KIcon("battery"), i18n("Battery Status"), parent);

        m_layout->addItem(m_batteryIcon);

        m_lineLayout = new QGraphicsLinearLayout();

        m_comboBox = new QGraphicsProxyWidget(parent);
        m_profilesCombo = new KComboBox();

        m_comboBox->setWidget(m_profilesCombo);
        m_lineLayout->addItem(m_comboBox);

        m_button = new QGraphicsProxyWidget(parent);
        m_profilesButton = new KPushButton(0);

        m_profilesButton->setText(i18n("Set Selected Profile"));
        m_profilesButton->setIcon(KIcon("services"));

        /*QAction *m_removeAction = new QAction(KIcon("list-remove"), i18n("Uninstall Package"), this);
        connect(m_removeAction, SIGNAL(triggered()), SLOT(removePackage()));
        m_actionIcon->addIconAction(m_removeAction);
        m_contextMenu->addAction(m_removeAction);
        connect(m_actionIcon, SIGNAL(activated()), SLOT(showContextMenu()));*/

        m_lineLayout->addItem(m_button);

        m_layout->addItem(m_lineLayout);
    }

    QDBusMessage msg = QDBusMessage::createMethodCall("org.archlinux.shaman", "/Shaman", "org.archlinux.shaman", "doStreamPackages");
    m_dbus.call(msg);
}

IdleView::~IdleView()
{
    m_layout->removeItem(m_actionsLayout);
    m_layout->removeItem(m_lineLayout);

    m_comboBox->setWidget(0);
    m_button->setWidget(0);

    delete m_batteryIcon;
}

void IdleView::updateBatteryIcon(int percent, bool plugged)
{
    Q_UNUSED(percent)
    Q_UNUSED(plugged)

    kDebug() << "Update here!";
}

void IdleView::updateProfiles(const QString &current, const QStringList &available)
{
    m_profilesCombo->clear();
    m_profilesCombo->addItems(available);
    m_profilesCombo->setCurrentIndex(m_profilesCombo->findText(current));
}

void IdleView::setSelectedProfile()
{
    QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kded", "/modules/powerdevil",
                       "org.kde.PowerDevil", "setProfile");
    msg << m_profilesCombo->currentText();
}

#include "IdleView.moc"
