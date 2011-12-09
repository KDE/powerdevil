/***************************************************************************
 *   Copyright (C) 2010 by Dario Freddi <drf@kde.org>                      *
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


#ifndef POWERDEVIL_POWERDEVILACTION_H
#define POWERDEVIL_POWERDEVILACTION_H

#include "powerdevilpolicyagent.h"

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

#include <kdemacros.h>

class KConfigGroup;

namespace PowerDevil
{
class BackendInterface;
class Core;

/**
 * @brief The base class for Power Management Actions
 *
 * The Action class is the very base class for writing a new Power Management action.
 * Developers wishing to implement their own action are supposed to subclass Action.
 *
 * @par Creating a brand new Action
 *
 * If you are already familiar with KDE's plugin system, you have to know that actions are
 * nothing but a KService plugin which will be loaded on demand. Each action has an ID associated to it
 * which represents it in the config file and uniquely identifies it in the loading phase.
 *
 * In addition to standard parameters, the .desktop file representing your action should contain the following
 * entries:
 *
 * @code
 * X-KDE-ServiceTypes=PowerDevil/Action
 * X-KDE-PowerDevil-Action-ID=YourActionID
 * X-KDE-PowerDevil-Action-IsBundled=false
 * X-KDE-PowerDevil-Action-UIComponentLibrary=myactionplugin_config
 * X-KDE-PowerDevil-Action-ConfigPriority=98
 * @endcode
 *
 * The @c UIComponentLibrary field refers to the library which contains the configuration UI
 * for your action. Please see ActionConfig documentation for more details.
 *
 * The @c ConfigPriority is relevant to the configuration UI as well, and determines where your config UI will appear.
 * The higher the number, the higher your config UI will be in the list of actions. Choose this value wisely: usually
 * only very basic power management functions should have a value > 50.
 *
 * The most important functions you need to reimplement are loadAction and triggerImpl. The first is called
 * whenever an action is loaded, carrying the action's configuration. The other is called whenever
 * the action is triggered. You usually want to process the action here as triggerImpl is guaranteed
 * to be called just when policies are satisfied.
 *
 * @par Runtime requirements
 *
 * Some actions might be available only when the system satisfies certain hardware or software runtime requirements.
 * In this case, powerdevil provides a way for the action to advertise to the outside whether it is supported or
 * not. This can be done by reimplementing @c isSupported and adding to the .desktop file the field
 *
 * @code
 * X-KDE-PowerDevil-Action-HasRuntimeRequirement=true
 * @endcode
 *
 * Done that, powerdevil will take care of exposing your action only if support for it is advertised. In addition,
 * the UI will expose the configuration of your action only if its support is advertised. Of course, this means the
 * action will be temporarily loaded by the config UI to verify its support. If your action is performing some tasks in
 * the constructor besides setting policies, first of all revise your design since you probably don't need or want to
 * do that. If you really cannot avoid that, you MUST check for an OPTIONAL parameter in the QVariantList coming
 * from the plugin's constructor. If it exists, it's a boolean and it is true, the action is being loaded just for
 * a support check, and you should refrain from doing any actions which would affect the system. This parameter is also
 * used in tests.
 *
 * @par Handling policies from within the action
 *
 * As you might know, the KDE Power Management system features a very efficient policy handler, which
 * prevents Power Management actions when certain condition occurs. The integration with Actions is very easy:
 * in your Action's constructor, you want to call setRequiredPolicies, stating which policies have to be
 * satisfied to perform the action. For example, if your action should not be performed whenever the session
 * cannot be interrupted, you would pass @c InterruptSession.
 *
 * Done that, your action is already obeying to the policy. trigger, in fact, calls triggerImpl just when
 * the policies are allowing your action to be performed.
 *
 * @since 4.6
 */
class KDE_EXPORT Action : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Action)

public:
    /**
     * Default constructor
     */
    explicit Action(QObject *parent);
    /**
     * Default destructor
     */
    virtual ~Action();

    /**
     * Reimplement this function when creating a new Action. This function is called whenever the action is loaded or
     * its configuration changes. It carries the KConfigGroup associated with your action and generated from your
     * config interface.
     *
     * @param config The action's configuration which should be loaded.
     * @returns Whether the action has been successfully loaded.
     *
     * @see ActionConfig
     */
    virtual bool loadAction(const KConfigGroup &config) = 0;
    /**
     * Unloads the action. You usually shouldn't reimplement this function: reimplement onUnloadAction instead.
     *
     * @returns Whether the action has been successfully unloaded
     */
    virtual bool unloadAction();

    /**
     * This function is meant to find out if this action is available on this system. Actions
     * CAN reimplement this function if they are dependent on specific hardware/software requirements.
     * By default, this function will always return true.
     *
     * Should this function return false, the core will delete and ignore the action right after creation.
     *
     * @returns Whether this action is supported or not by the current system
     */
    virtual bool isSupported();

    /**
     * Triggers the action with the given argument. This function is meant to be used by the caller only -
     * if you are implementing your own action, reimplement triggerImpl instead.
     *
     * @param args The arguments for triggering the action
     */
    void trigger(const QVariantMap &args);

protected:
    /**
     * Registers an idle timeout for this action. Call this function and not KIdleTime directly to take advantage
     * of Action's automated handling of idle timeouts. Also, please reimplement onIdleTimeout instead of listening
     * to KIdleTime's signals to catch idle timeout events.
     *
     * @param msec The idle timeout to be registered in milliseconds.
     */
    void registerIdleTimeout(int msec);
    /**
     * Sets the required policies needed for this Action to run. Usually, you want to call this function in your
     * Action's constructor.
     *
     * @param requiredPolicies A set of policies which are required to run this action. It can be empty if your
     *                         Action does not rely on policies.
     */
    void setRequiredPolicies(PowerDevil::PolicyAgent::RequiredPolicies requiredPolicies);

    /**
     * This function's body should undertake the Action's execution. It has to be reimplemented in a new Action.
     *
     * @param args The arguments for triggering the action
     */
    virtual void triggerImpl(const QVariantMap &args) = 0;

    /**
     * @returns The BackendInterface
     */
    PowerDevil::BackendInterface *backend();
    /**
     * @returns The PowerDevil Core
     */
    PowerDevil::Core *core();

protected Q_SLOTS:
    /**
     * This function is called whenever a profile is loaded. Please note that this is slightly different from
     * loadAction: in fact a profile can be reloaded without having the action change its configuration.
     * If your action should do something as soon as a profile switches, it should be done inside this function.
     */
    virtual void onProfileLoad() = 0;
    /**
     * This slot is triggered whenever an idle timeout registered with registerIdleTimeout is reached.
     *
     * @param msec The idle timeout reached in milliseconds
     */
    virtual void onIdleTimeout(int msec) = 0;
    /**
     * This slot is triggered whenever the PC wakes up from an Idle state. It is @b always called after a registered
     * idle timeout has been reached.
     */
    virtual void onWakeupFromIdle() = 0;
    /**
     * This function is called when the profile is unloaded.
     */
    virtual void onProfileUnload() = 0;
    /**
     * This function is called when the action is unloaded. You usually want to put what would have gone in your
     * destructor here.
     *
     * @returns Whether the action was unloaded successfully.
     */
    virtual bool onUnloadAction();

Q_SIGNALS:
    void actionTriggered(bool result, const QString &error = QString());

private:
    class Private;
    Private * const d;

    friend class Core;
    friend class ActionPool;
};

}

#endif // POWERDEVIL_POWERDEVILACTION_H
