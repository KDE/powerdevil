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

#include "xrandrbrightness.h"

#include <config-X11.h>

#include <stdio.h>
#include <stdlib.h>

#include <ctype.h>
#include <string.h>
#include <unistd.h>

XRandrBrightness::XRandrBrightness()
    : m_backlight(None), m_resources(0)
{
    // init
    int major, minor;
    if (!XRRQueryVersion (QX11Info::display(), &major, &minor))
    {
        qWarning("RandR extension missing");
        return;
    }

    if (major < 1 || (major == 1 && minor < 2))
    {
        qWarning("RandR version %d.%d too old", major, minor);
        return;
    }

    m_backlight = XInternAtom(QX11Info::display(), "Backlight", True);
    if (m_backlight == None)
        m_backlight = XInternAtom(QX11Info::display(), "BACKLIGHT", True);  // try with legacy atom

    if (m_backlight == None)
    {
        qWarning("No outputs have backlight property");
        return;
    }

#ifdef HAS_RANDR_1_3
    if (minor > 2) {
        m_resources = XRRGetScreenResourcesCurrent(QX11Info::display(), QX11Info::appRootWindow()); // version 1.3, faster version
    } else
#endif
    {
        m_resources = XRRGetScreenResources(QX11Info::display(), QX11Info::appRootWindow());
    }

    if (!m_resources)
    {
        qWarning("No available Randr resources");
        return;
    }
}

XRandrBrightness::~XRandrBrightness()
{
    if (m_resources) {
        XRRFreeScreenResources(m_resources);
    }
}

bool XRandrBrightness::isSupported() const
{
    return (m_resources != 0);
}

float XRandrBrightness::brightness() const
{
    float result = 0;

    if (!m_resources)
        return result;

    for (int o = 0; o < m_resources->noutput; o++)
    {
        RROutput output = m_resources->outputs[o];
        double cur = backlight_get(output);
        if (cur != -1)
        {
            XRRPropertyInfo * info = XRRQueryOutputProperty(QX11Info::display(), output, m_backlight);
            if (info)
            {
                if (info->range && info->num_values == 2)
                {
                    double min = info->values[0];
                    double max = info->values[1];

                    // FIXME for now just return the first output's value
                    result = (cur - min) * 100 / (max - min);
                    break;
                }
                XFree(info);
            }
        }
    }

    return result;
}

void XRandrBrightness::setBrightness(float brightness)
{
    if (!m_resources)
        return;

    for (int o = 0; o < m_resources->noutput; o++)
    {
        RROutput output = m_resources->outputs[o];
        double cur = backlight_get(output);
        if (cur != -1)
        {
            XRRPropertyInfo * info = XRRQueryOutputProperty(QX11Info::display(), output, m_backlight);
            if (info)
            {
                if (info->range && info->num_values == 2)
                {
                    double min = info->values[0];
                    double max = info->values[1];

                    // FIXME for now just set the first output's value
                    double value = min + (brightness * (max - min) / 100);
                    backlight_set(output, (long) (value + 0.5));
                }
                XFree(info);
            }
        }
    }

    XSync(QX11Info::display(), False);
}

long XRandrBrightness::backlight_get(RROutput output) const
{
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop;
    Atom actual_type;
    int actual_format;
    long value;

    if (!m_backlight || XRRGetOutputProperty (QX11Info::display(), output, m_backlight,
                                              0, 4, False, False, None,
                                              &actual_type, &actual_format,
                                              &nitems, &bytes_after, &prop) != Success)
        return -1;

    if (actual_type != XA_INTEGER || nitems != 1 || actual_format != 32)
        value = -1;
    else
        value = *((long *) prop);
    XFree (prop);
    return value;
}

void XRandrBrightness::backlight_set(RROutput output, long value)
{
    XRRChangeOutputProperty (QX11Info::display(), output, m_backlight, XA_INTEGER, 32,
                             PropModeReplace, (unsigned char *) &value, 1);
}
