/*
�   Copyright (C) 2019 Istituto Italiano di Tecnologia (IIT)
�   All rights reserved.
�
�   This software may be modified and distributed under the terms of the
�   GPL-2+ license. See the accompanying LICENSE file for details.
*/

#include <yarp/os/Network.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Time.h>
#include <yarp/os/LogStream.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/Map2DLocation.h>
#include <yarp/dev/OdometryData.h>
#include <yarp/os/Bottle.h>
#include <yarp/sig/Vector.h>
#include <yarp/dev/ControlBoardInterfaces.h>
#include <yarp/dev/IFrameTransform.h>
#include <mutex>
#include <math.h>
#include <iCub/ctrl/adaptWinPolyEstimator.h>

using namespace yarp::os;

class localization_device_with_estimated_odometry
{
private:
    //velocity estimation
    yarp::sig::Vector            m_odom_vel;
    yarp::sig::Vector            m_robot_vel;
    iCub::ctrl::AWLinEstimator*  m_estimator;
    yarp::dev::OdometryData      m_current_odom;
    std::mutex                   m_current_odom_mutex;

public:
    localization_device_with_estimated_odometry();
    virtual ~localization_device_with_estimated_odometry();
    yarp::dev::OdometryData estimate(const yarp::dev::Nav2D::Map2DLocation& m_localization_data);
    yarp::dev::OdometryData getOdometry();
};
