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

    m_words << i18nc("Note this is a KRunner keyword", "power profile") <<
    i18nc("Note this is a KRunner keyword", "power governor") <<
    i18nc("Note this is a KRunner keyword", "power scheme") <<
    i18nc("Note this is a KRunner keyword", "screen brightness");

    setObjectName("PowerDevil");
}

PowerDevilRunner::~PowerDevilRunner()
{
}

void PowerDevilRunner::match(Plasma::RunnerContext &context)
{
    QString term = context.query();

    foreach(const QString &word, m_words) {
        if (term.startsWith(word, Qt::CaseInsensitive)) {
            if (word == i18nc("Note this is a KRunner keyword", "power profile")) {
                KConfig *m_profilesConfig = new KConfig("powerdevilprofilesrc", KConfig::SimpleConfig);

                foreach(const QString &profile, m_profilesConfig->groupList()) {
                    if (term.split(' ').count() == 3) {
                        if (!profile.startsWith(term.split(' ').at(2)))
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
            } else if (word == i18nc("Note this is a KRunner keyword", "power governor")) {
                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kded",
                                   "/modules/powerdevil", "org.kde.PowerDevil", "getSupportedGovernors");
                QDBusReply<QStringList> govs = m_dbus.call(msg);

                foreach(const QString &ent, govs.value()) {
                    if (term.split(' ').count() == 3) {
                        if (!ent.startsWith(term.split(' ').at(2)))
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
            } else if (word == i18nc("Note this is a KRunner keyword", "power scheme")) {
                QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.kded",
                                   "/modules/powerdevil", "org.kde.PowerDevil", "getSupportedSchemes");
                QDBusReply<QStringList> schemes = m_dbus.call(msg);

                foreach(const QString &ent, schemes.value()) {
                    if (term.split(' ').count() == 3) {
                        if (!ent.startsWith(term.split(' ').at(2)))
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
            } else if (word == i18nc("Note this is a KRunner keyword", "screen brightness")) {
                if (term.split(' ').count() == 3) {
                    bool test;

                    int b = term.split(' ').at(2).toInt(&test);

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
                } else if (term.split(' ').count() == 2) {
                    Plasma::QueryMatch match1(this);

                    match1.setType(Plasma::QueryMatch::ExactMatch);

                    match1.setIcon(KIcon("battery-charging-040"));
                    match1.setText(i18n("Dim screen totally"));

                    match1.setRelevance(1);
                    match1.setId("DimTotal");
                    context.addMatch(term, match1);

                    Plasma::QueryMatch match2(this);

                    match2.setType(Plasma::QueryMatch::ExactMatch);

                    match2.setIcon(KIcon("battery-charging-040"));
                    match2.setText(i18n("Dim screen by half"));

                    match2.setRelevance(1);
                    match2.setId("DimHalf");
                    context.addMatch(term, match2);

                    Plasma::QueryMatch match3(this);

                    match3.setType(Plasma::QueryMatch::ExactMatch);

                    match3.setIcon(KIcon("battery-charging-040"));
                    match3.setText(i18n("Turn off screen"));

                    match3.setRelevance(1);
                    match3.setId("TurnOffScreen");
                    context.addMatch(term, match3);
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
    } else if (match.id() == "PowerDevil_DimTotal") {
        iface.call("setBrightness", 0);
    } else if (match.id() == "PowerDevil_DimHalf") {
        iface.call("setBrightness", -2);
    } else if (match.id() == "PowerDevil_TurnOffScreen") {
        iface.call("turnOffScreen");
    }
}

#include "PowerDevilRunner.moc"
