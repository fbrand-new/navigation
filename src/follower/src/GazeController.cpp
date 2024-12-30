/*
 * SPDX-FileCopyrightText: 2024 Istituto Italiano di Tecnologia (IIT)
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file GazeController.cpp
 * @authors: Valentina Gaggero <valentina.gaggero@iit.it>
 */

#include "GazeController.h"

#include <math.h>

#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Vocab.h>

using namespace yarp::os;
using namespace FollowerTarget;

YARP_LOG_COMPONENT(FOLLOWER_GAZE, "navigation.follower.gazeControl")

bool GazeController::init(GazeCtrlUsedCamera cam, yarp::os::ResourceFinder &rf, bool debugOn)
{
    Bottle gaze_group = rf.findGroup("GAZE");
    if (gaze_group.isNull())
    {
        yCWarning(FOLLOWER_GAZE) << "Missing GAZE group! the module uses default value!";
    }
    else
    {
        if(gaze_group.check("pixel_x_range"))
        {
            xpixelRange.first=gaze_group.find("pixel_x_range").asList()->get(0).asInt32();
            xpixelRange.second=gaze_group.find("pixel_x_range").asList()->get(1).asInt32();
        }

        if(gaze_group.check("pixel_y_range"))
        {
            ypixelRange.first=gaze_group.find("pixel_y_range").asList()->get(0).asInt32();
            ypixelRange.second=gaze_group.find("pixel_y_range").asList()->get(1).asInt32();
        }

        if(gaze_group.check("trajTimeInLookingup"))
        {
            m_trajectoryTime=gaze_group.find("trajTimeInLookingup").asFloat64();
        }
        else
            m_trajectoryTime = 10;//sec

        if(gaze_group.check("trajTime_timeout"))
        {
            m_lookup_timeout=gaze_group.find("trajTime_timeout").asFloat64();
        }
    }


    yCInfo(FOLLOWER_GAZE) << "GAZE=" << xpixelRange.first << xpixelRange.second << ypixelRange.first<< ypixelRange.second;
    yCInfo(FOLLOWER_GAZE) << "GAZE::TRAJECTORYtIME=" << m_trajectoryTime << "default=" <<m_trajectoryTimeDefault;



    if(!m_outputPort2gazeCtr.open("/follower/gazetargets:o"))
    {
        yCError(FOLLOWER_GAZE) << "Error opening output port for gaze control";
        return false;
    }
    //TODO: read frpm cam the size of image
    m_pLeft.u = 50;
    m_pLeft.v = 110;

    m_pCenter.u = 160;
    m_pCenter.v = 110;

    m_pRight.u = 270;
    m_pRight.v =110;

    if(GazeCtrlUsedCamera::left ==cam)
        m_camera_str_command = "left";
    else
        m_camera_str_command = "depth_rgb";

    m_debugOn=debugOn;

    m_rpcPort2gazeCtr.open("/follower/gazeController/rpc");

    return true;
}

bool GazeController::deinit(void)
{
    setTrajectoryTime(m_trajectoryTimeDefault);
    yarp::os::Time::delay(0.5);
    m_outputPort2gazeCtr.interrupt();
    m_outputPort2gazeCtr.close();

    m_rpcPort2gazeCtr.interrupt();
    m_rpcPort2gazeCtr.close();
    return true;
}

GazeCtrlLookupStates GazeController::lookup4Target(void)
{
    switch(m_lookupState)
    {
        case GazeCtrlLookupStates::none:
        {
            getTrajectoryTime(); //read the current trajectory time
            if(m_debugOn)
                yCDebug(FOLLOWER_GAZE) << "GazeCtrl in NONE state: move gaze to angle (35.0 10.0).";

            setTrajectoryTime(m_trajectoryTime);
            lookAtAngle(35.0, 10.0);
            m_lookupState = GazeCtrlLookupStates::nearest;
            m_stateMachineTimeOut.starttime = yarp::os::Time::now();
            m_stateMachineTimeOut.isstarted=true;
            m_stateMachineTimeOut.duration=m_lookup_timeout;
        }break;

        case GazeCtrlLookupStates::nearest:
        {
            if(checkMotionDone())
            {
                if(m_debugOn)
                    yCDebug(FOLLOWER_GAZE) << "GazeCtrl in NEAREST state: motion completed.(Duration=" << yarp::os::Time::now()-m_stateMachineTimeOut.starttime << "). Move gaze to angle (-35.0 10.0) ";
                lookAtAngle(-35.0, 10.0);
                m_lookupState = GazeCtrlLookupStates::otherside;
                m_stateMachineTimeOut.starttime = yarp::os::Time::now();

            }
            else
            {
                if(yarp::os::Time::now()-m_stateMachineTimeOut.starttime > m_stateMachineTimeOut.duration)
                {
                    stopLookup4Target();
                    setTrajectoryTime(m_trajectoryTimeDefault);
                    m_lookupState=GazeCtrlLookupStates::failed;
                    yCError(FOLLOWER_GAZE) << "GazeCtrl in NEAREST state: error reaching angle (35.0 10.0)";
                }
                //else do nothing...wait until gaze is in position or there is a timeout.
            }

        }break;


        case GazeCtrlLookupStates::otherside:
        {

            if(checkMotionDone())
            {
                if(m_debugOn)
                    yCDebug(FOLLOWER_GAZE) << "GazeCtrl in OTHERSIDE state: motion completed.(Duration=" << yarp::os::Time::now()-m_stateMachineTimeOut.starttime << "). Move gaze to angle (0.0 10.0) ";

                lookAtAngle(0, 10);
                m_lookupState = GazeCtrlLookupStates::infront;
                m_stateMachineTimeOut.starttime = yarp::os::Time::now();
           }
            else
            {
                if(yarp::os::Time::now()-m_stateMachineTimeOut.starttime > m_stateMachineTimeOut.duration)
                {
                    stopLookup4Target();
                    setTrajectoryTime(m_trajectoryTimeDefault);

                    m_lookupState=GazeCtrlLookupStates::failed;
                    yCError(FOLLOWER_GAZE) << "GazeCtrl in OTHERSIDE state: error reaching angle (35.0 10.0)";
                }
                //else do nothing...wait until gaze is in position or there is a timeout.
            }
         }break;

        case GazeCtrlLookupStates::infront:
        {

            if(checkMotionDone())
            {
                m_lookupState = GazeCtrlLookupStates::finished;
                if(m_debugOn)
                    yCDebug(FOLLOWER_GAZE) << "GazeCtrl in INFRONT state: motion completed. SUCCESS! (Duration=" << yarp::os::Time::now()-m_stateMachineTimeOut.starttime << ")";
            }
            else
            {
                if(yarp::os::Time::now()-m_stateMachineTimeOut.starttime > m_stateMachineTimeOut.duration)
                {
                    stopLookup4Target();
                    setTrajectoryTime(m_trajectoryTimeDefault);

                    m_lookupState=GazeCtrlLookupStates::failed;
                    yCError(FOLLOWER_GAZE) << "GazeCtrl in INFRONT state: error reaching angle (35.0 10.0)";
                }
                //else do nothing...wait until gaze is in position or there is a timeout.
            }
        }break;

        case GazeCtrlLookupStates::finished:
        {
            if(m_debugOn)
                yCDebug(FOLLOWER_GAZE) << "GazeCtrl in FINISHED state. ";
            stopLookup4Target();
            setTrajectoryTime(m_trajectoryTimeDefault);

            m_stateMachineTimeOut.isstarted=false;
        }break;
    };
    return m_lookupState;
}

std::string GazeController::stausToStrig()
{
    std::string str="";
    switch(m_lookupState)
    {
        case GazeCtrlLookupStates::none:
        {
            str="NONE state.";
            if(m_stateMachineTimeOut.isstarted=true)
                str+= " Gaze is moving toward to angle (35.0 10.0)";
        }break;

        case GazeCtrlLookupStates::nearest:
        {
            str="NEAREST state. Gaze is moving toward to angle (35.0 10.0)";
        }break;


        case GazeCtrlLookupStates::otherside:
        {
            str="OTHERSIDE state. Gaze is moving toward to angle (-35.0 10.0)";
        }break;

        case GazeCtrlLookupStates::infront:
        {

            str="INFRONT state. Gaze is moving toward to angle (0.0 10.0)";
        }break;

        case GazeCtrlLookupStates::finished:
        {
            str="FINISHED state. Lookup for target is finished!!";
        }break;
        case GazeCtrlLookupStates::failed:
        {
            str="FAILED state. An error occurred in looking up for target";
        }break;

        return str;
    };

}


void GazeController::resetLookupstateMachine(void)
{

    if(m_lookupState != GazeCtrlLookupStates::none)
    {
        stopLookup4Target();
        setTrajectoryTime(m_trajectoryTimeDefault);
    }
    m_lookupState = GazeCtrlLookupStates::none;
}

bool GazeController::lookInFront(void)
{
    return lookAtPixel(m_pCenter.u, m_pCenter.v);
}

bool GazeController::lookAtPixel(double u, double v)
{
    if (m_outputPort2gazeCtr.getOutputCount() == 0)
        return true;

    if(isnan(u) || isnan(v))
        return true;

    bool movegaze=false;
    if( (u<xpixelRange.first) || (u>xpixelRange.second))
        movegaze=true;
    if( (v<ypixelRange.first) || (v>ypixelRange.second) )
        movegaze=true;

    if(m_debugOn)
        yCDebug(FOLLOWER_GAZE) << "LookAtPixel(" <<u<<v << ") movegaze=" <<movegaze;

    if(!movegaze)
        return true;

    static double last_u=0, last_v=0;

    if((last_u==0) && (last_v==0))
    {
        last_u=u;
        last_v=v;
    }
    else
    {
        if((fabs(last_u-u)<10) || (fabs(last_v-v)<10))
        {
            //yCError() << "LookAtPixel(" <<u<<v << ") in threshold!!!";
            return true;
        }
        last_u=u;
        last_v=v;
    }

    Property &p = m_outputPort2gazeCtr.prepare();
    p.clear();
    p.put("control-frame",m_camera_str_command);
    p.put("target-type","image");
    p.put("image",m_camera_str_command);

    Bottle location = yarp::os::Bottle();
    Bottle &val = location.addList();
    val.addFloat64(u);
    val.addFloat64(v);
    p.put("target-location",location.get(0));
    m_outputPort2gazeCtr.write();

    return true;
}

bool GazeController::lookAtPoint(const  yarp::sig::Vector &x)
{
    static int count=0;
    if (m_outputPort2gazeCtr.getOutputCount() == 0)
        return true;

    count++;
    if(count<20)
        return true;

    count=0;

    Property &p = m_outputPort2gazeCtr.prepare();
    p.clear();

    //p.put("control-frame", m_camera_str_command);
    p.put("target-type","cartesian");

    Bottle target;
    target.addList().read(x);
    p.put("target-location",target.get(0));

    if(m_debugOn)
        yCDebug(FOLLOWER_GAZE) << "Command to gazectrl lookAtPoint: " << p.toString();

    m_outputPort2gazeCtr.write();

    return true;
}


bool GazeController::lookAtAngle(double a, double b)
{
    if (m_outputPort2gazeCtr.getOutputCount() == 0)
        return true;

    Property &p = m_outputPort2gazeCtr.prepare();
    p.clear();

    p.put("target-type","angular");

    Bottle target;
    Bottle &val = target.addList();
    val.addFloat64(a);
    val.addFloat64(b);
    p.put("target-location",target.get(0));

   if(m_debugOn)
       yCDebug(FOLLOWER_GAZE) << "Command to gazectrl lookAtAngle: " << p.toString();

    m_outputPort2gazeCtr.write();

    return true;
}


bool GazeController::stopLookup4Target(void)
{
    if (m_rpcPort2gazeCtr.asPort().getOutputCount() == 0)
        return true;

    Bottle cmd, ans;
    cmd.clear();
    ans.clear();

    cmd.addString("stop");

    m_rpcPort2gazeCtr.write(cmd, ans);

   if(m_debugOn)
       yCDebug(FOLLOWER_GAZE) << "GazeController::stopLookup4Target rpc_cmd=" << cmd.toString() << "Ans=" << ans.toString();

    if(ans.toString() == "ack")
        return true;
    else
        return false;
}

bool GazeController::setTrajectoryTime(double T)
{
    if (m_rpcPort2gazeCtr.asPort().getOutputCount() == 0)
        return true;

    Bottle cmd, ans;
    cmd.clear();
    ans.clear();

    cmd.addString("set");
    cmd.addString("T");
    cmd.addFloat64(T);

    m_rpcPort2gazeCtr.write(cmd, ans);

   if(m_debugOn)
       yCDebug(FOLLOWER_GAZE) << "GazeController: rpc_cmd=" << cmd.toString() << "Ans=" << ans.toString();

    if(ans.toString() == "ack")
        return true;
    else
        return false;

}

bool GazeController::getTrajectoryTime(void)
{
    if (m_rpcPort2gazeCtr.asPort().getOutputCount() == 0)
        return true;

    Bottle cmd, ans;
    cmd.clear();
    ans.clear();

    cmd.addString("get");
    cmd.addString("T");

    m_rpcPort2gazeCtr.write(cmd, ans);

    if(m_debugOn)
        yCDebug(FOLLOWER_GAZE) << "GazeController: rpc_cmd=" << cmd.toString() << "Ans=" << ans.toString();

    if(ans.get(0).toString() == "ack")
    {
        m_trajectoryTimeDefault=ans.get(1).asFloat64();
        return true;
    }
    else
        return false;

}

bool GazeController::checkMotionDone(void)
{
    if(m_rpcPort2gazeCtr.asPort().getOutputCount() == 0)
        return true;

    Bottle cmd, ans;
    cmd.clear();
    ans.clear();

    cmd.addString("get");
    cmd.addString("done");

    m_rpcPort2gazeCtr.write(cmd, ans);

   if(m_debugOn)
       yCDebug(FOLLOWER_GAZE) << "GazeController: rpc_cmd=" << cmd.toString() << "Ans=" << ans.toString();

    if(ans.get(0).asVocab32() == yarp::os::Vocab32::encode("ack"))
    {
        if(ans.get(1).asInt32() == 1)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
        return false;


}




bool GazeController::lookAtPointRPC(const  yarp::sig::Vector &x)
{
    if(m_rpcPort2gazeCtr.asPort().getOutputCount() == 0)
        return true;

static int count=0;

count++;
if(count <10)
    return true;

count =0;

    Bottle cmd, ans;
    cmd.clear();
    ans.clear();

    cmd.addString("look");
    Property &p=cmd.addDict();



    p.put("target-type","cartesian");

    Bottle target;
    target.addList().read(x);
    p.put("target-location",target.get(0));


    if(m_debugOn)
        yCDebug(FOLLOWER_GAZE) << "Command to gazectrl lookAtPointRPC: " << cmd.toString();



    m_rpcPort2gazeCtr.write(cmd, ans);

    if(ans.get(0).asVocab32() == yarp::os::Vocab32::encode("ack"))
    {
        return true;
    }
    else
        return false;

}


void GazeController::setGazeTimeout_debug(double t)
{
    m_lookup_timeout=t;
    yCError(FOLLOWER_GAZE) << "LOOKUP TIMEOUT=" << m_lookup_timeout;
}

