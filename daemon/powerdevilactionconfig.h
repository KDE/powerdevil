/*
 *   SPDX-FileCopyrightText: 2010 Dario Freddi <drf@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QWidget>

#include "powerdevilcore_export.h"

namespace PowerDevil
{
class ProfileSettings;

/**
 * @brief The base class for Action's config interfaces
 *
 * This class should be reimplemented when creating a new Action. It is used to generate
 * a configuration UI for your action and integrate it into KDE Power Management System's
 * config module seamlessly.
 *
 * @par Creating an ActionConfig
 *
 * If you already have been through creating an Action, you have seen that Action's desktop
 * file contains already all the needed information for loading its respective ActionConfig. For this
 * reason, despite ActionConfig being a plugin, you do not need to create a desktop file for it -
 * you just have to write the correct info into the Action's one.
 *
 * @par The key/value pair rationale
 *
 * The config UI works in a key/value fashion, where value is a generic QWidget. This is done
 * to keep the UI consistent without hurting the flexibility of the implementation.
 * Please remember to keep the configuration as easy and essential as possible: if you have gone
 * beyond 2 rows, you might be doing something wrong.
 *
 * @par Loading and saving configuration
 *
 * Of course, you should also be providing the logic for saving and loading your action's configuration.
 * This is done through PowerDevil::ProfileSettings: your action should have corresponding entries in
 * PowerDevil::ProfileSettings which is provided by the KDE Power Management System and accessible
 * to an ActionConfig via profileSettings(). The very same settings data you are loading/saving here
 * will be the one passed to Action::loadAction.
 *
 * @see Action
 *
 * @since 4.6
 */
class POWERDEVILCORE_EXPORT ActionConfig : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ActionConfig)

public:
    /**
     * Default constructor
     */
    explicit ActionConfig(QObject *parent);
    /**
     * Default destructor
     */
    ~ActionConfig() override;

    /**
     * @returns The ProfileSettings associated with this profile, where data should be saved (and loaded from)
     *
     * @note You should not track this ProfileSettings, as it might change from profile to profile.
     *       Always use this function to access it.
     */
    PowerDevil::ProfileSettings *profileSettings() const;
    /**
     * Overwrites the profile settings object associated with this profile with @c settings.
     *
     * @param settings A ProfileSettings carrying the settings for this profile. (No ownership transfer.)
     */
    void setProfileSettings(PowerDevil::ProfileSettings *settings);

    /**
     * This function should return the key/value pairs containing your Action's configuration UI.
     *
     * @returns A list of QString, QWidget pairs representing your Action's configuration UI
     */
    virtual QList<QPair<QString, QWidget *>> buildUi() = 0;

    /**
     * This function gets called whenever the parent module requires to load a specific configuration. This
     * usually occurs when @c profileSettings is updated, for example due to the fact that the user wants to modify
     * a different profile. In this function, you should update the info contained in the widgets generated
     * by @c buildUi accordingly.
     */
    virtual void load() = 0;
    /**
     * This function gets called whenever the parent module requires to save the configuration. Usually, you
     * want to read values from the widgets generated with @c buildUi and call setter functions on @c profileSettings
     * to update the Action's configuration. The Action does not need to call save() on @c profileSettings by itself,
     * that is the responsibility of the parent module.
     */
    virtual void save() = 0;

    /**
     * @returns The action-specific value from @c profileSettings that tells whether the action is enabled for this profile.
     *
     * @note The parent module can use this to update the checkbox associated with the action's config section.
     */
    virtual bool enabledInProfileSettings() const = 0;

    /**
     * This function gets called by the parent module to let the ActionConfig set in @c profileSettings
     * whether or not the action is enabled for this profile. This may be called in the same go with save(),
     * make sure that there is no conflict between values set here and there.
     */
    virtual void setEnabledInProfileSettings(bool enabled) = 0;

protected Q_SLOTS:
    /**
     * Call this slot whenever the user makes some changes to the widgets.
     */
    void setChanged();

Q_SIGNALS:
    /**
     * This signal gets emitted every time the user changes the (unsaved) configuration. Do not emit this signal
     * directly: call setChanged instead.
     *
     * @see setChanged
     */
    void changed();

private:
    class Private;
    Private *const d;
};

}
