/*
 * SPDX-FileCopyrightText: 2025 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <qqmlregistration.h>

#include <KConfigWatcher>

class BrightnessGlobalsSettings;

class DarkModeControl : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)

    Q_PROPERTY(QString currentTheme READ currentTheme NOTIFY currentThemeChanged)
    Q_PROPERTY(QString otherTheme READ otherTheme NOTIFY otherThemeChanged)

public:
    explicit DarkModeControl(QObject *parent = nullptr);
    ~DarkModeControl() override;

    bool darkMode() const;
    void setDarkMode(bool enable);
    Q_SIGNAL void darkModeChanged();

    QString currentTheme() const;
    Q_SIGNAL void currentThemeChanged();

    QString otherTheme() const;
    Q_SIGNAL void otherThemeChanged();

private:
    void updateCurrentTheme();
    void updateOtherTheme();
    void updateDarkMode();

    BrightnessGlobalsSettings *const m_settings;
    KConfigWatcher::Ptr m_settingsWatcher;

    bool m_darkMode = false;

    QString m_currentTheme;
    QString m_otherTheme;
};
