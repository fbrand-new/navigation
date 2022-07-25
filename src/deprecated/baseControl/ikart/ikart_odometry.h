/* 
 * Copyright (C)2011  Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
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

#ifndef IKART_ODOMETRY_H
#define IKART_ODOMETRY_H

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
#include <math.h>
#include "../odometryHandler.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::math;

class iKart_Odometry: public OdometryHandler
{
private:
    //encoder variables
    double              encA_offset;
    double              encB_offset;
    double              encC_offset;

    double              encA;
    double              encB;
    double              encC;

    //measured motor velocity
    double              velA;
    double              velB;
    double              velC;

    //estimated motor velocity
    double              velA_est;
    double              velB_est;
    double              velC_est;
    iCub::ctrl::AWLinEstimator      *encvel_estimator;

    //robot geometry
    double              geom_r;
    double              geom_L;
    double              g_angle;

    yarp::sig::Vector enc;
    yarp::sig::Vector encv;

public:
    /**
    * Constructor
    * @param _driver is a pointer to a remoteControlBoard driver.
    */
    iKart_Odometry(PolyDriver* _driver);
    
    /**
    * Default destructor
    */
    virtual ~iKart_Odometry();

    //The following methods are documented in base class odometry.h
    bool   reset_odometry();
    bool   open(const Property &_options);
    void   compute();
    void   printStats();
};

#endif
