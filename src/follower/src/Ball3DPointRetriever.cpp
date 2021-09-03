
/******************************************************************************
 *                                                                            *
 * Copyright (C) 2019 Fondazione Istituto Italiano di Tecnologia (IIT)        *
 * All Rights Reserved.                                                       *
 *                                                                            *
 ******************************************************************************/

/**
 * @file Ball3DPointRetriever.cpp
 * @authors: Valentina Gaggero <valentina.gaggero@iit.it>
 */

#include "Ball3DPointRetriever.h"

#include <yarp/os/Time.h>
#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>

using namespace yarp::os;
using namespace FollowerTarget;

YARP_LOG_COMPONENT(FOLLOWER_BALLRET, "navigation.follower.ballRetriever")

Target_t Ball3DPointRetriever::getTarget(void)
{
    Target_t t(m_refFrame); //it is initialized as invalid target

    Bottle *b = m_inputPort.read(false);
    if(nullptr == b)
    {
        if(m_debugOn)
            yCDebug(FOLLOWER_BALLRET) <<" Ball3DPointRetriever::getTarget: I received nothing!";
        return t;
    }

    bool ballIsTracked = (b->get(6).asFloat64() == 1.0) ? true : false;

    if(!ballIsTracked)
    {
        if(m_debugOn)
            yCDebug(FOLLOWER_BALLRET) << "Ball3DPointRetriever: I can't see the redBall";
        return t;
    }


    t.point3D[0] = b->get(0).asFloat64();
    t.point3D[1] = b->get(1).asFloat64();
    t.point3D[2] = b->get(2).asFloat64();

    t.pixel[0] = b->get(4).asFloat64(); //u and V are the the coordinate x any of image.
    t.pixel[1] = b->get(5).asFloat64();

    t.isValid=true;

    return t;
}

bool Ball3DPointRetriever::init(yarp::os::ResourceFinder &rf)
{
    // 1) set my reference frame
    m_refFrame=ReferenceFrameOfTarget_t::head_leopard_left;

    // 2) read name of input port from config file and open it
    std::string inputPortName="targets";
    Bottle config_group = rf.findGroup("FOLLOWER_GENERAL");
    if (config_group.isNull())
    {
        yCError(FOLLOWER_BALLRET) << "Missing FOLLOWER_GENERAL group! the module uses default value!";
    }
    else
    {
        if (config_group.check("inputPort"))  {inputPortName = config_group.find("inputPort").asString(); }
    }

    bool ret = TargetRetriever::initInputPort("/follower/" + inputPortName +":i");

    return ret;
}


bool Ball3DPointRetriever::deinit(void)
{
    return(TargetRetriever::deinitInputPort());
}

