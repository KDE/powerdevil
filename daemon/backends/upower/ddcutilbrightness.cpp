/*  This file is part of the KDE project
 *    Copyright (C) 2017 Dorian Vogel <dorianvogel@gmail.com>
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

#include <powerdevil_debug.h>
#include "ddcutilbrightness.h"


DDCutilBrightness::DDCutilBrightness()
{
    m_setBrightnessEventFilter.setInterval(100);
    m_setBrightnessEventFilter.setSingleShot(true);
    connect(&m_setBrightnessEventFilter, &QTimer::timeout, this, &DDCutilBrightness::setBrightnessAfterFilter);
}

void DDCutilBrightness::detect()
{
#ifndef WITH_DDCUTIL
    qCInfo(POWERDEVIL)  << "[DDCutilBrightness] compiled without DDC/CI support";
    return;
#else
    DDCA_Status rc;

    qCDebug(POWERDEVIL) << "Check for monitors using ddca_get_displays()...";
    // Inquire about detected monitors.
    DDCA_Display_Info_List * dlist;
    ddca_get_display_info_list2(false, &dlist);
    qCDebug(POWERDEVIL) << "ddca_get_display_info_list2() returned "<< dlist;
    qCInfo(POWERDEVIL)  << "[DDCutilBrightness] " << dlist->ct << "display(s) were detected";

    for (int iDisp=0;iDisp<dlist->ct;iDisp++) {
        DDCA_Display_Identifier did;
        DDCA_Display_Ref dref;
        DDCA_Display_Handle dh = nullptr;  // initialize to avoid clang analyzer warning

        qCDebug(POWERDEVIL) << "Create a Display Identifier for display"<<iDisp <<
        " : "<< dlist->info[iDisp].model_name;

        m_displayInfoList.append(dlist->info[iDisp]);

        rc = ddca_create_dispno_display_identifier(iDisp+1, &did); // ddcutil uses 1 paded indexing for displays

        char * did_repr = ddca_did_repr(did);

        qCDebug(POWERDEVIL) << "did="<<did_repr;

        qCDebug(POWERDEVIL) << "Create a display reference from the display identifier...";
        rc = ddca_create_display_ref(did, &dref);

        if (rc != 0) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness]: ddca_create_display_ref() returned "<<
            rc<< " ("<<ddca_rc_name(rc) <<
            "): "<< ddca_rc_name(rc);
            continue;
        }

        char * dref_repr = ddca_dref_repr(dref);
        qCDebug(POWERDEVIL) << "dref= " << dref_repr;

        qCDebug(POWERDEVIL) << "Open the display reference, creating a display handle...";
        rc = ddca_open_display(dref, &dh);
        if (rc != 0) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness]: ddca_open_display"<< rc;
            continue;
        }

        qCDebug(POWERDEVIL) << "adding handle to list";
        m_displayHandleList.append(dh);
        qCDebug(POWERDEVIL) << "handles nb: "<<m_displayHandleList.count();
        char* capabilityString;
        DDCA_Capabilities* parsedCapabilities;
        ddca_get_capabilities_string(dh, &capabilityString);
        ddca_parse_capabilities_string(capabilityString, &parsedCapabilities);
        qCInfo(POWERDEVIL) << "[DDCutilBrightness] " << parsedCapabilities->vcp_code_ct << "capabilities parsed";


        m_descrToVcp_perDisp.append(QMap<QString, int>());
        m_vcpTovcpValueWithDescr_perDisp.append(QMap<int, QMap<int, QString> >() );
        //fill the feature description to vcp LUT

        DDCA_Version_Feature_Info* featureInfo;
        for (int iVcp=0;iVcp<parsedCapabilities->vcp_code_ct;iVcp++) {

            int vcpCode=parsedCapabilities->vcp_codes[iVcp].feature_code;

            m_vcpTovcpValueWithDescr_perDisp[iDisp].insert(vcpCode, QMap<int, QString>());

            m_descrToVcp_perDisp[iDisp].insert(
                QString(ddca_get_feature_name(vcpCode)), vcpCode);


            ddca_get_feature_info_by_display(m_displayHandleList.at(iDisp), vcpCode, &featureInfo);
            if (featureInfo == nullptr) {
                continue;
            }
            qCDebug(POWERDEVIL) << featureInfo->feature_code<<":"<<featureInfo->desc;
            if ((featureInfo->feature_flags & DDCA_SIMPLE_NC) != DDCA_SIMPLE_NC) {
                continue;
            }
            for (int iVcpVal=0;featureInfo->sl_values[iVcpVal].value_code!=0;++iVcpVal) {

                qCDebug(POWERDEVIL) << "\t"<<featureInfo->sl_values[iVcpVal].value_code
                <<":"<< featureInfo->sl_values[iVcpVal].value_name;

                bool thisVcpValIsSupported=false;

                for (int iSupportedVcpVal=0; iSupportedVcpVal<parsedCapabilities->vcp_codes[iVcp].value_ct; iSupportedVcpVal++) {
                    if(parsedCapabilities->vcp_codes[iVcp].values[iSupportedVcpVal]
                        ==featureInfo->sl_values[iVcpVal].value_code) {
                        thisVcpValIsSupported=true;
                        }
                }

                if (thisVcpValIsSupported) {
                    (m_vcpTovcpValueWithDescr_perDisp[iDisp])[vcpCode].insert(
                        featureInfo->sl_values[iVcpVal].value_code,
                        featureInfo->sl_values[iVcpVal].value_name);
                }
            }
        }
        ddca_free_display_identifier(did);
        ddca_free_parsed_capabilities(parsedCapabilities);
    }
#endif
}


bool DDCutilBrightness::isSupported() const
{
#ifndef WITH_DDCUTIL
    return false;
#else
    return !m_displayHandleList.isEmpty();
#endif
}


long DDCutilBrightness::brightness()
{
#ifdef WITH_DDCUTIL
    //we check whether the timer is running, this means we received new values but did not send them yet to the monitor
    //not checking that results in the brightness slider jump to the previous vqlue when changing.
    if(m_setBrightnessEventFilter.isActive()) {
        m_lastBrightnessKnown = m_tmpCurrentBrightness;
    }
    else {  //FIXME: gets value for display 1
        DDCA_Status rc;
        DDCA_Any_Vcp_Value *returnValue;

        rc = ddca_get_any_vcp_value_using_explicit_type(m_displayHandleList.at(0),
                                m_descrToVcp_perDisp.at(0).value("Brightness"),
                                DDCA_NON_TABLE_VCP_VALUE, &returnValue);
        qCDebug(POWERDEVIL) << "[DDCutilBrightness::brightness]: ddca_get_any_vcp_value_using_explicit_type returned" << rc;

        //check rc to prevent crash on wake from idle and the monitor has gone to powersave mode
        if (rc == 0) {
            m_lastBrightnessKnown = (long)VALREC_CUR_VAL(returnValue);
        }
    }
    return m_lastBrightnessKnown;
#else
    return 0;
#endif
}

long DDCutilBrightness::brightnessMax()
{
#ifdef WITH_DDCUTIL
    DDCA_Status rc;
    DDCA_Any_Vcp_Value *returnValue;

    rc = ddca_get_any_vcp_value_using_explicit_type(m_displayHandleList.at(0),
                            m_descrToVcp_perDisp.at(0).value("Brightness"),
                            DDCA_NON_TABLE_VCP_VALUE, &returnValue);
    qCDebug(POWERDEVIL) << "[DDCutilBrightness::brightnessMax]: ddca_get_any_vcp_value_using_explicit_type returned" << rc;

    //check rc to prevent crash on wake from idle and the monitor has gone to powersave mode
    if (rc == 0) {
        m_lastMaxBrightnessKnown = (long)VALREC_MAX_VAL(returnValue);
    }

    return m_lastMaxBrightnessKnown;
#else
    return 100.0;
#endif
}

void DDCutilBrightness::setBrightness(long value)
{
    m_tmpCurrentBrightness = value;
    qCDebug(POWERDEVIL) << "[DDCutilBrightness]: saving brightness value: " << value;
    m_setBrightnessEventFilter.start();
}

void DDCutilBrightness::setBrightnessAfterFilter()
{
#ifdef WITH_DDCUTIL
    DDCA_Status rc;
    for (int iDisp=0;iDisp<m_displayHandleList.count();iDisp++) {          //FIXME: we set the same brightness to all monitors, plugin architecture needed
        qCDebug(POWERDEVIL) << "[DDCutilBrightness]: setting brightness "<<m_tmpCurrentBrightness;
        rc = ddca_set_continuous_vcp_value(m_displayHandleList.at(iDisp), 
                                        m_descrToVcp_perDisp.at(iDisp).value("Brightness"), 
                                        (int)m_tmpCurrentBrightness);

        if (rc != 0) {
            qCWarning(POWERDEVIL) << "[DDCutilBrightness::setBrightness] failed, trying to detect()";
            detect();
        } else {
            m_lastBrightnessKnown = m_tmpCurrentBrightness;
        }
    }
#else
    return;
#endif
}
