/*  This file is part of the KDE project
 *    Copyright (C) 2010 Lukas Tinkl <ltinkl@redhat.com>
 * 
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License version 2 as published by the Free Software Foundation.
 * 
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 * 
 *    You should have received a copy of the GNU Library General Public License
 *    along with this library; see the file COPYING.LIB.  If not, write to
 *    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *    Boston, MA 02110-1301, USA.
 * 
 */

#ifndef XRANDRBRIGHTNESS_H
#define XRANDRBRIGHTNESS_H

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>

class XRandrBrightness
{
public:
    XRandrBrightness();
    ~XRandrBrightness();
    bool isSupported() const;
    float brightness() const;
    void setBrightness(float brightness);

private:
    long backlight_get(RROutput output) const;
    void backlight_set(RROutput output, long value);

    Atom m_backlight;
    XRRScreenResources  *m_resources;
};

#endif // XRANDRBRIGHTNESS_H
