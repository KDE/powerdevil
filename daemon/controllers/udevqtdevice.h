/*
    SPDX-FileCopyrightText: 2009 Benjamin K. Stuhl <bks24@cornell.edu>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace UdevQt
{
class DevicePrivate;
class Device
{
public:
    Device();
    Device(const Device &other);
    ~Device();
    Device &operator=(const Device &other);

    bool isValid() const;
    QString subsystem() const;
    QString devType() const;
    QString name() const;
    QString sysfsPath() const;
    int sysfsNumber() const;
    QString driver() const;
    QString primaryDeviceFile() const;
    QStringList alternateDeviceSymlinks() const;
    QStringList deviceProperties() const;
    QStringList sysfsProperties() const;
    Device parent() const;

    // ### should this really be a QVariant? as far as udev knows, everything is a string...
    // see also Client::devicesByProperty
    QVariant deviceProperty(const QString &name) const;
    QString decodedDeviceProperty(const QString &name) const;
    QVariant sysfsProperty(const QString &name) const;
    Device ancestorOfType(const QString &subsys, const QString &devtype) const;

private:
    explicit Device(DevicePrivate *devPrivate);
    friend class Client;
    friend class ClientPrivate;

    DevicePrivate *d;
};

typedef QList<Device> DeviceList;

}
