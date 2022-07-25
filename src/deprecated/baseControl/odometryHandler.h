/*
* Copyright (C)2015  iCub Facility - Istituto Italiano di Tecnologia
* Author: Marco Randazzo
* email:  marco.randazzo@iit.it
* website: www.robotcub.org
* Permission is granted to copy, distribute, and/or modify this program
* under the terms of the GNU General Public License, version 2 or any
* later version published by the Free Software Foundation.
*
* A copy of the license can be found at
* http://www.robotcub.org/icub/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

#ifndef ODOMETRY_H
#define ODOMETRY_H

#include <yarp/os/Network.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Bottle.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/ResourceFinder.h>
#include <yarp/os/Os.h>
#include <yarp/os/Time.h>
#include <yarp/sig/Vector.h>
#include <yarp/dev/Drivers.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/os/PeriodicThread.h>
#include <yarp/os/Semaphore.h>
#include <yarp/dev/ControlBoardInterfaces.h>
#include <iCub/ctrl/adaptWinPolyEstimator.h>
#include <yarp/math/Math.h>
#include <yarp/os/Stamp.h>
#include <string>
#include <yarp/os/Node.h>
#include <yarp/os/Publisher.h>
#include <yarp/rosmsg/nav_msgs/Odometry.h>
#include <yarp/rosmsg/geometry_msgs/PolygonStamped.h>
#include <yarp/rosmsg/geometry_msgs/TransformStamped.h>
#include <yarp/rosmsg/tf2_msgs/TFMessage.h>
#include <yarp/dev/OdometryData.h>

#define _USE_MATH_DEFINES
#include <math.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::math;

#ifndef RAD2DEG
#define RAD2DEG 180.0/M_PI
#endif 

#ifndef DEG2RAD
#define DEG2RAD M_PI/180.0
#endif

class OdometryHandler
{
protected:
    Property              ctrl_options;
    mutable std::mutex    data_mutex;
    yarp::os::Stamp       timeStamp;
    double                last_time;

protected:
    //estimated cartesian velocity in the fixed odometry reference frame (world)
    double              odom_vel_x;
    double              odom_vel_y;
    double              odom_vel_lin;
    double              odom_vel_theta;

    //estimated cartesian velocity in the base relative reference frame
    double              base_vel_x;
    double              base_vel_y;
    double              base_vel_lin;
    double              base_vel_theta;

    //estimated odometer 
    double              traveled_distance;
    double              traveled_angle;

    //estimated cartesian pos in the fixed odometry reference frame
    double              m_odom_x;
    double              m_odom_y;
    double              m_odom_theta;

protected:
    //motor control interfaces 
    PolyDriver                      *control_board_driver;
    IEncoders                       *ienc;

public:
    /**
    * Constructor
    * @param _driver is a pointer to a remoteControlBoard driver.
    */
    OdometryHandler(PolyDriver* _driver);

    /**
    * Default destructor
    */
    ~OdometryHandler();
    
    /**
    * Resets the robot odometry, meaning the current robot pose becomes 0,0,0.
    * @return true/false if the command is accepted.
    */
    virtual bool   reset_odometry() = 0;

    /**
    * Initializes the odometry module.
    * @param options the options to be passed to odometry module.
    * @return true/false if the odometry module is opened successfully.
    */
    virtual bool   open(const Property &options) = 0;

    /**
    * Performs the odometry computation.
    */
    virtual void   compute() = 0;

    /**
    * Print some stats about the computed odometry estimation
    */
    virtual void   printStats() = 0;

    /**
    * Terminates the execution of the odometry module
    */
    virtual void   close();

    /**
    * Get the current robot linear velocity
    * @return the current linear velocity
    */
    virtual double get_base_vel_lin();

    /**
    * Get the current robot angular velocity
    * @return the current angular velocity
    */
    virtual double get_base_vel_theta();

    bool getOdometry(yarp::dev::OdometryData& odomData) const;
};

#endif
