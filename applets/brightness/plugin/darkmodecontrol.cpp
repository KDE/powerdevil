/*
 * SPDX-FileCopyrightText: 2025 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "darkmodecontrol.h"

#include <QLoggingCategory>
#include <QProcess>

#include <KPackage/PackageLoader>

#include "brightness_kdeglobalssettings.h"

Q_LOGGING_CATEGORY(DARKMODE_CONTROL, "org.kde.plasma.darkmodecontrol")

using namespace Qt::Literals::StringLiterals;

DarkModeControl::DarkModeControl(QObject *parent)
    : QObject(parent)
    , m_settings(new BrightnessGlobalsSettings(this))
    , m_settingsWatcher(KConfigWatcher::create(m_settings->sharedConfig()))
{
    connect(m_settingsWatcher.get(), &KConfigWatcher::configChanged, this, [this](const auto &group, const auto &keys) {
        Q_UNUSED(keys);
        if (group.name() == "KDE"_L1) {
            m_settings->load();
        }
    });

    connect(m_settings, &BrightnessGlobalsSettings::lookAndFeelPackageChanged, this, &DarkModeControl::updateCurrentTheme);

    connect(m_settings, &BrightnessGlobalsSettings::defaultLightLookAndFeelChanged, this, &DarkModeControl::updateOtherTheme);
    connect(m_settings, &BrightnessGlobalsSettings::defaultDarkLookAndFeelChanged, this, [this] {
        updateDarkMode();
        updateOtherTheme();
    });

    updateDarkMode();
    updateCurrentTheme();
    updateOtherTheme();
}

DarkModeControl::~DarkModeControl()
{
}

bool DarkModeControl::darkMode() const
{
    return m_darkMode;
}

void DarkModeControl::setDarkMode(bool enable)
{
    if (darkMode() == enable) {
        return;
    }

    if (enable) {
        m_settings->setLookAndFeelPackage(m_settings->defaultDarkLookAndFeel());
    } else {
        m_settings->setLookAndFeelPackage(m_settings->defaultLightLookAndFeel());
    }

    QProcess::startDetached(u"plasma-apply-lookandfeel"_s, QStringList({u"-a"_s, m_settings->lookAndFeelPackage()}));
}

void DarkModeControl::updateDarkMode()
{
    const bool oldDarkMode = m_darkMode;

    m_darkMode = m_settings->lookAndFeelPackage() == m_settings->defaultDarkLookAndFeel();

    if (m_darkMode != oldDarkMode) {
        Q_EMIT darkModeChanged();
    }

    updateOtherTheme();
}

QString DarkModeControl::currentTheme() const
{
    return m_currentTheme;
}

static QString lookAndFeelName(const QString &packageId)
{
    auto package = KPackage::PackageLoader::self()->loadPackage(u"Plasma/LookAndFeel"_s);
    package.setPath(packageId);

    if (package.metadata().isValid()) {
        return package.metadata().name();
    }

    // Fallback.
    return packageId;
}

void DarkModeControl::updateCurrentTheme()
{
    const QString themeName = lookAndFeelName(m_settings->lookAndFeelPackage());
    if (m_currentTheme != themeName) {
        m_currentTheme = themeName;
        Q_EMIT currentThemeChanged();
    }

    updateDarkMode();
}

QString DarkModeControl::otherTheme() const
{
    return m_otherTheme;
}

void DarkModeControl::updateOtherTheme()
{
    // The theme it will switch to when toggling.
    const QString otherThemeId = darkMode() ? m_settings->defaultLightLookAndFeel() : m_settings->defaultDarkLookAndFeel();
    const QString otherThemeName = lookAndFeelName(otherThemeId);

    if (m_otherTheme != otherThemeName) {
        m_otherTheme = otherThemeName;
        Q_EMIT otherThemeChanged();
    }
}

#include "moc_darkmodecontrol.cpp"
