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

#include "PowerDevilRunner.h"

#include <KIcon>
#include <KLocale>
#include <KDebug>
#include <KRun>
#include <QDBusInterface>
#include <QDBusConnectionInterface>

PowerDevilRunner::PowerDevilRunner(QObject *parent, const QVariantList &args)
        : Plasma::AbstractRunner(parent),
        m_dbus(QDBusConnection::sessionBus())
{
    KGlobal::locale()->insertCatalog("powerdevil");

    Q_UNUSED(args)

    /* Let's define all the words here. m_words contains all the words that
     * will eventually trigger a match in the runner.
     *
     * FIXME: I made all the words translatable, though I don't know if that's
     * the right way to go.
     */

    m_words << i18n("set-profile") << i18n("change-profile") << i18n("switch-profile") <<
    i18n("set-governor") << i18n("change-governor") << i18n("switch-governor") <<
    i18n("set-scheme") << i18n("change-scheme") << i18n("switch-scheme") <<
    i18n("set-brightness") << i18n("change-brightness");

    setObjectName(i18n("PowerDevil"));
}

PowerDevilRunner::~PowerDevilRunner()
{
}

void PowerDevilRunner::match(Plasma::RunnerContext &context)
{
    QString term = context.query();

    foreach(const QString &word, m_words) {
        if (term.startsWith(word, Qt::CaseInsensitive)) {
            if (word == "set-profile" || word == "change-profile" ||
                    word == "switch-profile") {
                KConfig *m_profilesConfig = new KConfig("powerdevilprofilesrc", KConfig::SimpleConfig);

                foreach(const QString &profile, m_profilesConfig->groupList()) {
                    if (term.split(' ').count() == 2) {
                        if (!profile.startsWith(term.split(' ').at(1)))
                            continue;
                    }

                    Plasma::QueryMatch match(this);

                    match.setType(Plasma::QueryMatch::ExactMatch);

                    match.setIcon(KIcon("battery-charging-040"));
                    match.setText(i18n("Set Profile to '%1'", profile));
                    match.setData(profile);

                    match.setRelevance(1);
                    match.setId("ProfileChange");
                    context.addMatch(term, match);
                }
                delete m_profilesConfig;
            } else if (word == "set-governor" || word == "change-governor" ||
                       word == "switch-governor") {
                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kded",
                                   "/modules/powerdevil", "org.kde.PowerDevil", "getSupportedGovernors");
                QDBusReply<QStringList> govs = m_dbus.call(msg);

                foreach(const QString &ent, govs.value()) {
                    if (term.split(' ').count() == 2) {
                        if (!ent.startsWith(term.split(' ').at(1)))
                            continue;
                    }

                    Plasma::QueryMatch match(this);

                    match.setType(Plasma::QueryMatch::ExactMatch);

                    match.setIcon(KIcon("battery-charging-040"));
                    match.setText(i18n("Set CPU Governor to '%1'", ent));
                    match.setData(ent);

                    match.setRelevance(1);
                    match.setId("GovernorChange");
                    context.addMatch(term, match);
                }
            } else if (word == "set-scheme" || word == "change-scheme" ||
                       word == "switch-scheme") {
                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kded",
                                   "/modules/powerdevil", "org.kde.PowerDevil", "getSupportedSchemes");
                QDBusReply<QStringList> schemes = m_dbus.call(msg);

                foreach(const QString &ent, schemes.value()) {
                    if (term.split(' ').count() == 2) {
                        if (!ent.startsWith(term.split(' ').at(1)))
                            continue;
                    }

                    Plasma::QueryMatch match(this);

                    match.setType(Plasma::QueryMatch::ExactMatch);

                    match.setIcon(KIcon("battery-charging-040"));
                    match.setText(i18n("Set Powersaving Scheme to '%1'", ent));
                    match.setData(ent);

                    match.setRelevance(1);
                    match.setId("SchemeChange");
                    context.addMatch(term, match);
                }
            } else if (word == "set-brightness" || word == "change-brightness") {
                if (term.split(' ').count() == 2) {
                    bool test;

                    int b = term.split(' ').at(1).toInt(&test);

                    if (test) {
                        int brightness = qBound(0, b, 100);

                        Plasma::QueryMatch match(this);

                        match.setType(Plasma::QueryMatch::ExactMatch);

                        match.setIcon(KIcon("battery-charging-040"));
                        match.setText(i18n("Set Brightness to %1", brightness));
                        match.setData(brightness);

                        match.setRelevance(1);
                        match.setId("BrightnessChange");
                        context.addMatch(term, match);
                    }
                }
            }
        }
    }
}

void PowerDevilRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context)

    QDBusInterface iface("org.kde.kded", "/modules/powerdevil", "org.kde.PowerDevil", m_dbus);

    if (match.id() == "PowerDevil_ProfileChange") {
        iface.call("refreshStatus");
        iface.call("setProfile", match.data().toString());
    } else if (match.id() == "PowerDevil_GovernorChange") {
        iface.call("setGovernor", match.data().toString());
    } else if (match.id() == "PowerDevil_SchemeChange") {
        iface.call("setPowersavingScheme", match.data().toString());
    } else if (match.id() == "PowerDevil_BrightnessChange") {
        iface.call("setBrightness", match.data().toInt());
    }
}

#include "PowerDevilRunner.moc"
