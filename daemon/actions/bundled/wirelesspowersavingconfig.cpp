/***************************************************************************
 *   Copyright (C) 2016 by Jan Grulich <jgrulich@redhat.com>               *
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

#include "wirelesspowersavingconfig.h"
#include "wirelesspowersaving.h"

#include <QComboBox>
#include <QVBoxLayout>

#include <KConfig>
#include <KPluginFactory>
#include <KSharedConfig>
#include <KLocalizedString>

#include <NetworkManagerQt/Manager>

K_PLUGIN_FACTORY(PowerDevilWirelessPowerSavingConfigFactory, registerPlugin<PowerDevil::BundledActions::WirelessPowerSavingConfig>(); )

namespace PowerDevil {
namespace BundledActions {

WirelessPowerSavingConfig::WirelessPowerSavingConfig(QObject *parent, const QVariantList& )
    : ActionConfig(parent)
    , m_btCombobox(nullptr)
    , m_wifiCombobox(nullptr)
    , m_wwanCombobox(nullptr)
{

}

WirelessPowerSavingConfig::~WirelessPowerSavingConfig()
{

}

void WirelessPowerSavingConfig::save()
{
    configGroup().writeEntry<uint>("wifiOption", m_wifiCombobox->itemData(m_wifiCombobox->currentIndex()).toUInt());
    configGroup().writeEntry<uint>("wwanOption", m_wwanCombobox->itemData(m_wwanCombobox->currentIndex()).toUInt());
    configGroup().writeEntry<uint>("btOption", m_btCombobox->itemData(m_btCombobox->currentIndex()).toUInt());
}

void WirelessPowerSavingConfig::load()
{
    configGroup().config()->reparseConfiguration();

    const uint wifiOptionType = configGroup().readEntry<uint>("wifiOption", 0);
    const uint wwanOptionType = configGroup().readEntry<uint>("wwanOption", 0);
    const uint btOptionType = configGroup().readEntry<uint>("btOption", 0);

    m_wifiCombobox->setCurrentIndex(m_wifiCombobox->findData(wifiOptionType));
    m_wwanCombobox->setCurrentIndex(m_wwanCombobox->findData(wwanOptionType));
    m_btCombobox->setCurrentIndex(m_btCombobox->findData(btOptionType));
}

QList< QPair< QString, QWidget* > > WirelessPowerSavingConfig::buildUi()
{
    m_wifiCombobox = new QComboBox();
    m_wifiCombobox->addItem(i18n("Leave unchanged"), (uint)WirelessPowerSaving::NoAction);
    m_wifiCombobox->addItem(i18n("Turn off"), (uint)WirelessPowerSaving::TurnOff);
    m_wifiCombobox->addItem(i18n("Turn on"), (uint)WirelessPowerSaving::TurnOn);

    m_wwanCombobox = new QComboBox();
    m_wwanCombobox->addItem(i18n("Leave unchanged"), (uint)WirelessPowerSaving::NoAction);
    m_wwanCombobox->addItem(i18n("Turn off"), (uint)WirelessPowerSaving::TurnOff);
    m_wwanCombobox->addItem(i18n("Turn on"), (uint)WirelessPowerSaving::TurnOn);

    m_btCombobox = new QComboBox();
    m_btCombobox->addItem(i18n("Leave unchanged"), (uint)WirelessPowerSaving::NoAction);
    m_btCombobox->addItem(i18n("Turn off"), (uint)WirelessPowerSaving::TurnOff);
    m_btCombobox->addItem(i18n("Turn on"), (uint)WirelessPowerSaving::TurnOn);

    // unified width for the comboboxes
    int comboBoxMaxWidth = 300;
    comboBoxMaxWidth = qMax(comboBoxMaxWidth, m_wifiCombobox->sizeHint().width());
    m_wifiCombobox->setMinimumWidth(300);
    m_wifiCombobox->setMaximumWidth(comboBoxMaxWidth);
    m_wwanCombobox->setMinimumWidth(300);
    m_wwanCombobox->setMaximumWidth(comboBoxMaxWidth);
    m_btCombobox->setMinimumWidth(300);
    m_btCombobox->setMaximumWidth(comboBoxMaxWidth);

    // Disable comboboxes for actions which are not allowed
    NMStringMap permissions = NetworkManager::permissions();
    for (auto it = permissions.constBegin(); it != permissions.constEnd(); ++it) {
        if (it.key() == QLatin1String("org.freedesktop.NetworkManager.enable-disable-wifi")) {
            m_wifiCombobox->setEnabled(it.value() == QLatin1String("yes"));
        } else if (it.key() == QLatin1String("org.freedesktop.NetworkManager.enable-disable-wwan")) {
            m_wwanCombobox->setEnabled(it.value() == QLatin1String("yes"));
        }
    }

    QList< QPair< QString, QWidget* > > retlist;
    retlist.append(qMakePair< QString, QWidget* >(i18n("Wi-Fi"), m_wifiCombobox));
    retlist.append(qMakePair< QString, QWidget* >(i18n("Mobile broadband"), m_wwanCombobox));
    retlist.append(qMakePair< QString, QWidget* >(i18n("Bluetooth"), m_btCombobox));

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    const auto comboIndexChanged = QOverload<int, const QString&>::of(&QComboBox::currentIndexChanged);
#else
    const auto comboIndexChanged = QOverload<int>::of(&QComboBox::currentIndexChanged);
#endif
    connect(m_wifiCombobox, comboIndexChanged, this, &WirelessPowerSavingConfig::setChanged);
    connect(m_wwanCombobox, comboIndexChanged, this, &WirelessPowerSavingConfig::setChanged);
    connect(m_btCombobox, comboIndexChanged, this, &WirelessPowerSavingConfig::setChanged);

    return retlist;
}

}
}

#include "wirelesspowersavingconfig.moc"
