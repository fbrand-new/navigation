/*
 * SPDX-FileCopyrightText: 2024 Istituto Italiano di Tecnologia (IIT)
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file BTMonitor.cpp
 * @authors: Valentina Gaggero <valentina.gaggero@iit.it>
 */
#include <BTMonitorMsg.h>
#include "BTMonitor.h"


#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>

using namespace FollowerTarget;

#define evt2str(e) #e

bool BTMonitor::Monitor::init(void)
{
    mapEvt2Str[Event::e_from_bt] ="e_from_bt";
    mapEvt2Str[Event::e_req_first_target] ="e_req_first_target";
    mapEvt2Str[Event::e_reply_first_target_invalid] ="e_reply_first_target_invalid";
    mapEvt2Str[Event::e_req_target] ="e_req_target";
    mapEvt2Str[Event::e_reply_N_target_invalid] ="e_reply_N_target_invalid";
    mapEvt2Str[Event::e_req_lookUp] ="e_req_lookUp";
    mapEvt2Str[Event::e_reply_lookup_failed] ="e_reply_lookup_failed";
    mapEvt2Str[Event::e_reply_lookup_succeed] ="e_reply_lookup_succeed";
    mapEvt2Str[Event::e_req_navig] ="e_req_navig";
    mapEvt2Str[Event::e_reply_human_lost] ="e_reply_human_lost";
    mapEvt2Str[Event::e_reply_target_found] ="e_reply_target_found";
    mapEvt2Str[Event::e_req_help] ="e_req_help";
    mapEvt2Str[Event::e_timeout] ="e_timeout";
    mapEvt2Str[Event::e_req_update_target] ="e_req_update_target";
    mapEvt2Str[Event::e_reply_update_failed] ="e_reply_update_failed";
    mapEvt2Str[Event::e_req] ="e_req";

    m_isRunning = m_toMonitorPort.open("/follower/monitor:o");
    return m_isRunning;
}


void BTMonitor::Monitor::sendEvent(BTMonitor::Event e)
{
    if(!m_isRunning)
        return;

    BTMonitorMsg msg;
    msg.skill = "follower";
    msg.event = mapEvt2Str[e];
//    yError() << "SEND EVT= " <<msg.event;
    m_toMonitorPort.write(msg);
}
