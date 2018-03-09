/*
    Copyright 2009 Benjamin K. Stuhl <bks24@cornell.edu>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UDEVQTDEVICE_H
#define UDEVQTDEVICE_H

#include <QObject>
#include <QList>
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
        Device &operator= (const Device &other);

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

#endif
