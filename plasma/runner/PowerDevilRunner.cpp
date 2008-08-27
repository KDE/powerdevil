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
        : Plasma::AbstractRunner(parent)
{
    KGlobal::locale()->insertCatalog("powerdevil");

    Q_UNUSED(args)

    m_words << "set-profile" << "change-profile" << "switch-profile";

    setObjectName(i18n("PowerDevil"));
}

PowerDevilRunner::~PowerDevilRunner()
{
}

void PowerDevilRunner::match(Plasma::RunnerContext &context)
{
    kDebug() << "Match search context?";
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
            }

        }
    }
}

void PowerDevilRunner::run(const Plasma::RunnerContext &context, const Plasma::QueryMatch &match)
{
    Q_UNUSED(context)

    QDBusInterface iface("org.kde.kded", "/modules/powerdevil", "org.kde.PowerDevil", QDBusConnection::sessionBus());

    /*iface.call("emitWarningNotification", "joberror",
            QString("match name: %1, context: %2, match id: %3").arg(match.data().toString()).arg(context.query()).arg(match.id()));*/

    if (match.id() == "PowerDevil_ProfileChange") {
        iface.call("refreshStatus");
        iface.call("setProfile", match.data().toString());
    }
}

#include "PowerDevilRunner.moc"
