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

#include "input.h"
#include "filters.h"
#include <yarp/os/Log.h>
#include <yarp/os/LogStream.h>
#include <navigation_defines.h>

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef RAD2DEG
#define RAD2DEG 180.0/M_PI
#endif 

YARP_LOG_COMPONENT(INPUT_HND, "navigation.baseControl.input")

void Input::printStats()
{
    yCInfo(INPUT_HND, "* Input thread:\n");
    if (rosInputEnabled)
    {
       yCInfo(INPUT_HND, "timeouts: %d joy1: %d joy2: %d aux: %d cmd: %d ros: %d\n", thread_timeout_counter, joy_timeout_counter[0], joy_timeout_counter[1], aux_timeout_counter, mov_timeout_counter, ros_timeout_counter);
    }
    else
    {
        yCInfo(INPUT_HND, "timeouts: %d joy1: %d joy2: %d aux: %d cmd: %d\n", thread_timeout_counter, joy_timeout_counter[0], joy_timeout_counter[1], aux_timeout_counter, mov_timeout_counter);
    }

    if (joystick_received[0]>0) 
        yCInfo(INPUT_HND, "Under joystick1 control (%d)\n", joystick_received[0]);
    if (joystick_received[1]>0) 
        yCInfo(INPUT_HND, "Under joystick2 control (%d)\n", joystick_received[1]);
}

void Input::close()
{
    port_movement_control.interrupt();
    port_movement_control.close();
    port_auxiliary_control.interrupt();
    port_auxiliary_control.close();
    if (port_joystick_control[0])
    {
        port_joystick_control[0]->interrupt();
        port_joystick_control[0]->close();
        delete port_joystick_control[0];
        port_joystick_control[0]=0;
    }
    if (port_joystick_control[1])
    {
        port_joystick_control[1]->interrupt();
        port_joystick_control[1]->close();
        delete port_joystick_control[1];
        port_joystick_control[1]=0;
    }
}

Input::~Input()
{
    close();
}

bool Input::configureJoypdad(int n, const Bottle& joypad_group)
{
    if (joypad_group.check("JoypadDevice"))
    {
        Value joydevicename = joypad_group.find("JoypadDevice");
        if (!joydevicename.isString())
        {
            yCError(INPUT_HND) << "baseControl: JoypadDevice param is not a string";
            return false;
        }
        Value joylocal = joypad_group.find("local");
        if (!joylocal.isString())
        {
            yCError(INPUT_HND) << "baseControl: JoypadDevice param is not a string";
            return false;
        }
        Value joyremote = joypad_group.find("remote");
        if (!joyremote.isString())
        {
            yCError(INPUT_HND) << "baseControl: JoypadDevice param is not a string";
            return false;
        }

        typedef Input::InputDescription::InputType inputType;
        vector<tuple<string, unsigned int*, inputType*, float*> > paramlist;
        paramlist.push_back(make_tuple("x",    &jDescr[n].xAxis.Id, &jDescr[n].xAxis.type, &jDescr[n].xAxis.Factor));
        paramlist.push_back(make_tuple("y",    &jDescr[n].yAxis.Id, &jDescr[n].yAxis.type, &jDescr[n].yAxis.Factor));
        paramlist.push_back(make_tuple("t",    &jDescr[n].tAxis.Id, &jDescr[n].tAxis.type, &jDescr[n].tAxis.Factor));
        paramlist.push_back(make_tuple("gain", &jDescr[n].gain.Id,  &jDescr[n].gain.type,  &jDescr[n].gain.Factor));

        for(auto p : paramlist)
        {
            bool                   idFound(false), facFound(false);
            string                 idPar;
            string                 factorPar;
            string                 Error("Axis/Button");
            inputType              type;
            map<string, inputType> str2types{make_pair("Axis",   Input::InputDescription::AXIS),
                                                make_pair("Button", Input::InputDescription::BUTTON)};
            for(auto t : str2types)
            {
                idPar     = std::get<0>(p)+t.first+"_id";
                factorPar = std::get<0>(p)+t.first+"_factor";
                if(joypad_group.check(idPar) && joypad_group.find(idPar).isInt32())
                {
                    idFound = true;
                    if(joypad_group.check(factorPar) && joypad_group.find(factorPar).isFloat64())
                    {
                        facFound = true;
                        type = str2types[t.first];
                        break;
                    }
                }
            }

            if(!idFound)
            {
                yCError(INPUT_HND) << "baseControl: param" << std::get<0>(p)+Error+"_id" << "not found or not a int in configuration file";
                return false;
            }

            if(!facFound)
            {
                yCError(INPUT_HND) << "baseControl: param" << std::get<0>(p)+Error+"_factor" << "not found or not a int in configuration file";
                return false;
            }

            if(type == Input::InputDescription::BUTTON &&
                    (std::get<0>(p) == "x" ||
                        std::get<0>(p) == "y" ||
                        std::get<0>(p) == "t"))
            {
                yCError(INPUT_HND) << "at the moment xAxis, yAxis and tAxis cannot be mapped to buttons";
                return false;
            }

            *std::get<1>(p) = joypad_group.find(idPar).asInt32();
            *std::get<2>(p) = type;
            *std::get<3>(p) = joypad_group.find(factorPar).asFloat64();
        }

        yCInfo(INPUT_HND) << "opening" << joydevicename.asString() << "device";
        Property joycfg;
        joycfg.put("device", joydevicename.asString());
        joycfg.put("local", joylocal.toString());
        joycfg.put("remote",joyremote.toString());

        if (!joyPolyDriver[n].open(joycfg))
        {
            yCError(INPUT_HND) << "baseControl: could not open the joypad device";
            return false;
        }

        joyPolyDriver[n].view(iJoy[n]);
        if (!iJoy[n])
        {
            yCError(INPUT_HND) << "joypad Device must implement the IJoypadController interface";
            return false;
        }
        unsigned int count;
        iJoy[n]->getAxisCount(count);
        if(count < 4)
        {
            yCError(INPUT_HND) << "joypad must have at least 3 axes";
            return false;
        }
    }
    return true;
}

bool Input::open(Property &_options)
{
    ctrl_options = _options;
    localName    = ctrl_options.find("local").asString();

    // open control input ports
    port_movement_control.open((localName+"/control:i").c_str());
    port_auxiliary_control.open((localName+"/aux_control:i").c_str());

    if (!ctrl_options.check("BASECTRL_GENERAL"))
    {
        yCError(INPUT_HND) << "Missing [BASECTRL_GENERAL] section";
        return false;
    }
    yarp::os::Bottle& general_options = ctrl_options.findGroup("BASECTRL_GENERAL");
    yarp::os::Bottle& joystick_options = ctrl_options.findGroup("JOYSTICK");

    //ROS section
    useRos = general_options.check("use_ROS", Value(false), "enable ROS communication").asBool();
    if (useRos)
    {
        if (ctrl_options.check("ROS_INPUT"))
        {
            yarp::os::Bottle rin_group = ctrl_options.findGroup("ROS_INPUT");
            if (rin_group.check("topic_name") == false)  { yCError(INPUT_HND) << "Missing topic_name parameter"; return false; }
            rosTopicName_twist = rin_group.find("topic_name").asString();
            rosInputEnabled = true;
        }
        else
        {
            rosInputEnabled = false;
        }
        
        if (!rosSubscriberPort_twist.topic(rosTopicName_twist))
        {
            yCError(INPUT_HND) << " opening " << rosTopicName_twist << " Topic, check your yarp-ROS network configuration\n";
            return false;
        }
    }
    

    double tmp = 0;
    tmp = (joystick_options.check("linear_vel_at_full_control", Value(0), "linear velocity at 100% of the joystick control [m/s]")).asFloat64();
    if (tmp>0) linear_vel_at_100_joy = tmp;
    tmp = (joystick_options.check("angular_vel_at_full_control", Value(0), "angular velocity at 100% of the joystick control [deg/s]")).asFloat64();
    if (tmp>0) angular_vel_at_100_joy = tmp;

    localName = ctrl_options.find("local").asString();

    //Joystick1
    if (general_options.check("joypad1_configuration")==false)
    {
        yCError(INPUT_HND) << "joypad1 device not configured";
        return false;
    }
    else
    {
        string joypad_group_name = general_options.find("joypad1_configuration").toString();
        if (joypad_group_name == "<none>")
        {
            yCInfo(INPUT_HND) << "No Joystick1 selected";
        }
        else if (joypad_group_name=="<joystick_port>")
        {
             port_joystick_control[0]=new BufferedPort<Bottle>;
             port_joystick_control[0]->open((localName+"/joystick1:i").c_str());
        }
        else
        {
            Bottle joypad_group = ctrl_options.findGroup(joypad_group_name);
            if (joypad_group.isNull())
            {
                yCError(INPUT_HND) << "Unable to find joypad section" << joypad_group_name;
                return false;
            }
            if (configureJoypdad(0,joypad_group)==false)
            {
                yCError(INPUT_HND,"Unable to configure joystick: JoypadDevice option not set.");
                return false;
            }
        }
    }

    //Joystick2
    if (general_options.check("joypad2_configuration")==false)
    {
        yCError(INPUT_HND) << "joypad2 device not configured";
        return false;
    }
    else
    {
        string joypad_group_name = general_options.find("joypad2_configuration").toString();
        if (joypad_group_name == "<none>")
        {
            yCInfo(INPUT_HND) << "No Joystick2 selected";
        }
        else if (joypad_group_name=="<joystick_port>")
        {
             port_joystick_control[1]=new BufferedPort<Bottle>;
             port_joystick_control[1]->open((localName+"/joystick2:i").c_str());
        }
        else
        {
            Bottle joypad_group = ctrl_options.findGroup(joypad_group_name);
            if (joypad_group.isNull())
            {
                yCError(INPUT_HND) << "Unable to find joypad section" << joypad_group_name;
                return false;
            }
            if (configureJoypdad(1,joypad_group)==false)
            {
                yCError(INPUT_HND,"Unable to configure joystick: JoypadDevice option not set.");
                return false;
            }
         }
    }

    return true;
}

Input::Input()
{
    useRos                 = false;

    thread_timeout_counter = 0;

    command_received       = 0;
    rosInput_received      = 0;
    joystick_received[0]   = 0;
    joystick_received[1]   = 0;
    auxiliary_received     = 0;
                           
    port_joystick_control[0] =0;
    port_joystick_control[1] =0;

    mov_timeout_counter    = 0;
    joy_timeout_counter[0] = 0;
    joy_timeout_counter[1] = 0;
    aux_timeout_counter    = 0;
    ros_timeout_counter    = 0;

    joy_linear_speed[0]      = 0;
    joy_angular_speed[0]     = 0;
    joy_desired_direction[0] = 0;
    joy_pwm_gain[0]          = 0;

    joy_linear_speed[1]      = 0;
    joy_angular_speed[1]     = 0;
    joy_desired_direction[1] = 0;
    joy_pwm_gain[1]          = 0;

    cmd_linear_speed       = 0;
    cmd_angular_speed      = 0;
    cmd_desired_direction  = 0;
    cmd_pwm_gain           = 0;

    aux_linear_speed       = 0;
    aux_angular_speed      = 0;
    aux_desired_direction  = 0;
    aux_pwm_gain           = 0;
    
    ros_linear_speed       = 0;
    ros_angular_speed      = 0;
    ros_desired_direction  = 0;
    ros_pwm_gain           = 0;

    linear_vel_at_100_joy  = 0;
    angular_vel_at_100_joy = 0;

    iJoy[0]                = 0;
    iJoy[1]                = 0;
}

void Input::read_percent_polar(const Bottle *b, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
    des_dir  = b->get(1).asFloat64();
    lin_spd  = b->get(2).asFloat64();
    ang_spd  = b->get(3).asFloat64();
    pwm_gain = b->get(4).asFloat64();
    pwm_gain = (pwm_gain<+100) ? pwm_gain : +100; 
    pwm_gain = (pwm_gain>0)    ? pwm_gain : 0;
}

void Input::read_percent_cart(const Bottle *b, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
    double x_speed = b->get(1).asFloat64();
    double y_speed = b->get(2).asFloat64();
    double t_speed = b->get(3).asFloat64();
    pwm_gain       = b->get(4).asFloat64();
    des_dir        = atan2(y_speed, x_speed) * RAD2DEG;
    lin_spd        = sqrt (x_speed*x_speed+y_speed*y_speed);
    ang_spd        = t_speed;
    pwm_gain       = (pwm_gain<+100) ? pwm_gain : +100;
    pwm_gain       = (pwm_gain>0) ? pwm_gain : 0;
}

void Input::read_speed_polar(const Bottle *b, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
    des_dir  = b->get(1).asFloat64();
    lin_spd  = b->get(2).asFloat64();
    ang_spd  = b->get(3).asFloat64();
    pwm_gain = b->get(4).asFloat64();
    pwm_gain = (pwm_gain<+100) ? pwm_gain : +100;
    pwm_gain = (pwm_gain>0) ? pwm_gain : 0;
}

void Input::read_speed_cart(const Bottle *b, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
     double x_speed = b->get(1).asFloat64();
     double y_speed = b->get(2).asFloat64();
     double t_speed = b->get(3).asFloat64();
     des_dir        = atan2(y_speed, x_speed) * RAD2DEG;
     lin_spd        = sqrt (x_speed*x_speed+y_speed*y_speed);
     ang_spd        = t_speed;
     pwm_gain = b->get(4).asFloat64();
}

void Input::read_joystick_data(JoyDescription *jDescr, IJoypadController* iJoy, double& des_dir, double& lin_spd, double& ang_spd, double& pwm_gain)
{
    //received a joystick command.
    double x_speed;
    double y_speed;
    iJoy->getAxis(jDescr->xAxis.Id, x_speed);
    iJoy->getAxis(jDescr->yAxis.Id, y_speed);
    iJoy->getAxis(jDescr->tAxis.Id, ang_spd);

    if(jDescr->gain.type == InputDescription::AXIS)
    {
        iJoy->getAxis(jDescr->gain.Id, pwm_gain);
    }
    else
    {
        float r;
        iJoy->getButton(jDescr->gain.Id, r);
        pwm_gain = r;
    }

    x_speed       *= jDescr->xAxis.Factor;
    y_speed       *= jDescr->yAxis.Factor;
    ang_spd       *= jDescr->tAxis.Factor;
    pwm_gain      *= jDescr->gain.Factor;

    des_dir  = atan2(y_speed, x_speed) * RAD2DEG;
    lin_spd  = sqrt (x_speed*x_speed+y_speed*y_speed);
    pwm_gain = (pwm_gain<+100) ? pwm_gain : +100;
    pwm_gain = (pwm_gain>0) ? pwm_gain : 0;
}

void Input::read_inputs(double& linear_speed,double& angular_speed,double& desired_direction, double& pwm_gain)
{
    static double wdt_old_mov_cmd = Time::now();
    static double wdt_old_ros_cmd = Time::now();
    static double wdt_old_joy_cmd = Time::now();
    static double wdt_old_aux_cmd = Time::now();
    static double wdt_mov_cmd     = Time::now();
    static double wdt_ros_cmd     = Time::now();
    static double wdt_joy_cmd[2]  = {Time::now(), Time::now()};
    static double wdt_aux_cmd     = Time::now();

    //- - -read joysticks - - -
    for (int id=0; id<2; id++)
    {
        //port joystick
        if (port_joystick_control[id])
        {
            if (Bottle* b = port_joystick_control[id]->read(false))
            {                
                if (b->get(0).asInt32()== BASECONTROL_COMMAND_PERCENT_POLAR)
                {
                    //received a joystick command.
                    read_percent_polar(b, joy_desired_direction[id],joy_linear_speed[id],joy_angular_speed[id],joy_pwm_gain[id]);
                    joy_linear_speed[id] = (joy_linear_speed[id] > 100) ? 100 : joy_linear_speed[id];
                    joy_angular_speed[id] = (joy_angular_speed[id] > 100) ? 100 : joy_angular_speed[id];
                    joy_linear_speed[id] = (joy_linear_speed[id] < -100) ? -100 : joy_linear_speed[id];
                    joy_angular_speed[id] = (joy_angular_speed[id] < -100) ? -100 : joy_angular_speed[id];
                    joy_linear_speed[id] = joy_linear_speed[id] / 100 * linear_vel_at_100_joy;
                    joy_angular_speed[id] = joy_angular_speed[id] / 100 * angular_vel_at_100_joy;
                    wdt_old_joy_cmd = wdt_joy_cmd[id];
                    wdt_joy_cmd[id] = Time::now();

                    //Joystick commands have higher priority respect to movement commands.
                    //this make the joystick to take control for 100*20 ms
                    if (joy_pwm_gain[id]>10) joystick_received[id] = 100;
                }
                else if (b->get(0).asInt32() == BASECONTROL_COMMAND_VELOCIY_POLAR)
                {
                    //received a joystick command.
                    read_speed_polar(b, joy_desired_direction[id], joy_linear_speed[id], joy_angular_speed[id], joy_pwm_gain[id]);
                    joy_linear_speed[id] = (joy_linear_speed[id] > 100) ? 100 : joy_linear_speed[id];
                    joy_angular_speed[id] = (joy_angular_speed[id] > 100) ? 100 : joy_angular_speed[id];
                    joy_linear_speed[id] = (joy_linear_speed[id] < -100) ? -100 : joy_linear_speed[id];
                    joy_angular_speed[id] = (joy_angular_speed[id] < -100) ? -100 : joy_angular_speed[id];
                    joy_linear_speed[id] = joy_linear_speed[id] / 100 * linear_vel_at_100_joy;
                    joy_angular_speed[id] = joy_angular_speed[id] / 100 * angular_vel_at_100_joy;
                    wdt_old_joy_cmd = wdt_joy_cmd[id];
                    wdt_joy_cmd[id] = Time::now();

                    //Joystick commands have higher priority respect to movement commands.
                    //this make the joystick to take control for 100*20 ms
                    if (joy_pwm_gain[id]>10) joystick_received[id] = 100;
                }
                else if (b->get(0).asInt32() == BASECONTROL_COMMAND_VELOCIY_CARTESIAN)
                {
                    //received a joystick command.
                    read_speed_cart(b, joy_desired_direction[id], joy_linear_speed[id], joy_angular_speed[id], joy_pwm_gain[id]);
                    joy_linear_speed[id] = (joy_linear_speed[id] > 100) ? 100 : joy_linear_speed[id];
                    joy_angular_speed[id] = (joy_angular_speed[id] > 100) ? 100 : joy_angular_speed[id];
                    joy_linear_speed[id] = (joy_linear_speed[id] < -100) ? -100 : joy_linear_speed[id];
                    joy_angular_speed[id] = (joy_angular_speed[id] < -100) ? -100 : joy_angular_speed[id];
                    joy_linear_speed[id] = joy_linear_speed[id] / 100 * linear_vel_at_100_joy;
                    joy_angular_speed[id] = joy_angular_speed[id] / 100 * angular_vel_at_100_joy;
                    wdt_old_joy_cmd = wdt_joy_cmd[id];
                    wdt_joy_cmd[id] = Time::now();

                    //Joystick commands have higher priority respect to movement commands.
                    //this make the joystick to take control for 100*20 ms
                    if (joy_pwm_gain[id]>10) joystick_received[id] = 100;
                }
                else
                {
                    yCError(INPUT_HND) << "Invalid format received on port_joystick_control";
                }
            }
        }
        //- device joystick
        else if(iJoy[id])
        {
            read_joystick_data(&jDescr[id], iJoy[id],joy_desired_direction[id], joy_linear_speed[id], joy_angular_speed[id], joy_pwm_gain[id]);
            joy_linear_speed[id] = (joy_linear_speed[id] > 100) ? 100 : joy_linear_speed[id];
            joy_angular_speed[id] = (joy_angular_speed[id] > 100) ? 100 : joy_angular_speed[id];
            joy_linear_speed[id] = (joy_linear_speed[id] < -100) ? -100 : joy_linear_speed[id];
            joy_angular_speed[id] = (joy_angular_speed[id] < -100) ? -100 : joy_angular_speed[id];
            joy_linear_speed[id] = joy_linear_speed[id] / 100 * linear_vel_at_100_joy;
            joy_angular_speed[id] = joy_angular_speed[id] / 100 * angular_vel_at_100_joy;
            wdt_old_joy_cmd = wdt_joy_cmd[id];
            wdt_joy_cmd[id] = Time::now();

            //Joystick commands have higher priority respect to movement commands.
            //this make the joystick to take control for 100*20 ms
            if (joy_pwm_gain[id]>10) joystick_received[id] = 100;
        }
    }

    //- - - read command port - - -
    if (Bottle *b = port_movement_control.read(false))
    {
        if (b->get(0).asInt32()== BASECONTROL_COMMAND_PERCENT_POLAR)
        {
            read_percent_polar(b, cmd_desired_direction,cmd_linear_speed,cmd_angular_speed,cmd_pwm_gain);
            wdt_old_mov_cmd = wdt_mov_cmd;
            wdt_mov_cmd = Time::now();
            command_received = 100;
        }
        else if (b->get(0).asInt32()== BASECONTROL_COMMAND_VELOCIY_POLAR)
        {
            read_speed_polar(b, cmd_desired_direction,cmd_linear_speed,cmd_angular_speed,cmd_pwm_gain);
            wdt_old_mov_cmd = wdt_mov_cmd;
            wdt_mov_cmd = Time::now();
            command_received = 100;
        }
        else if (b->get(0).asInt32()== BASECONTROL_COMMAND_VELOCIY_CARTESIAN)
        {
            read_speed_cart(b, cmd_desired_direction,cmd_linear_speed,cmd_angular_speed,cmd_pwm_gain);
            wdt_old_mov_cmd = wdt_mov_cmd;
            wdt_mov_cmd = Time::now();
            command_received = 100;
        }
        else
        {
            yCError(INPUT_HND) << "Invalid format received on port_movement_control";
        }
    }

    //- - -read aux port - - -
    if (yarp::dev::MobileBaseVelocity *b = port_auxiliary_control.read(false))
    {
        aux_desired_direction = atan2(b->vel_y, b->vel_x)*RAD2DEG;
        aux_linear_speed  = sqrt((b->vel_x*b->vel_x)+ (b->vel_y * b->vel_y));
        aux_angular_speed = b->vel_theta;
        aux_pwm_gain = 100;
        auxiliary_received = 100;
        wdt_aux_cmd = Time::now();
    }
    
    //- - - read ROS commands - - -
    if (rosInputEnabled)
    {
        if (yarp::rosmsg::geometry_msgs::Twist* rosTwist = rosSubscriberPort_twist.read(false))
        {
            Bottle b;
            b.addInt32(3);
            b.addFloat64(rosTwist->linear.x);
            b.addFloat64(rosTwist->linear.y);
            b.addFloat64(rosTwist->angular.z * RAD2DEG);
            b.addFloat64(100);
            read_speed_cart(&b, ros_desired_direction, ros_linear_speed, ros_angular_speed, ros_pwm_gain);
            wdt_old_ros_cmd   = wdt_ros_cmd;
            wdt_ros_cmd       = Time::now();
            rosInput_received = 100;
        }
    }

    //- - - priority test - - -
    if (joystick_received[0]>0)
    {
        desired_direction  = joy_desired_direction[0];
        linear_speed       = joy_linear_speed[0];
        angular_speed      = joy_angular_speed[0];
        pwm_gain           = joy_pwm_gain[0];
    }
    else if (joystick_received[1]>0)
    {
        desired_direction  = joy_desired_direction[1];
        linear_speed       = joy_linear_speed[1];
        angular_speed      = joy_angular_speed[1];
        pwm_gain           = joy_pwm_gain[1];
    }
    else if (auxiliary_received>0)
    {
        desired_direction  = aux_desired_direction;
        linear_speed       = aux_linear_speed;
        angular_speed      = aux_angular_speed;
        pwm_gain           = aux_pwm_gain;
    }
    else if (rosInput_received>0)
    {
        desired_direction  = ros_desired_direction;
        linear_speed       = ros_linear_speed;
        angular_speed      = ros_angular_speed;
        pwm_gain           = ros_pwm_gain;
    }
    else //if (command_received>0)
    {
        desired_direction  = cmd_desired_direction;
        linear_speed       = cmd_linear_speed;
        angular_speed      = cmd_angular_speed;
        pwm_gain           = cmd_pwm_gain;
    }

    //watchdog on received commands
    static double wdt_old=Time::now();
    double wdt=Time::now();
    //yCDebug("period: %f\n", wdt-wdt_old);
    if (wdt-wdt_mov_cmd > 0.200)
    {
        cmd_desired_direction=0;
        cmd_linear_speed=0;
        cmd_angular_speed=0;
        cmd_pwm_gain=0;
        mov_timeout_counter++; 
    }
    if (wdt-wdt_joy_cmd[0] > 0.200)
    {
        joy_desired_direction[0]=0;
        joy_linear_speed[0]=0;
        joy_angular_speed[0]=0;
        joy_pwm_gain[0]=0;
        joy_timeout_counter[0]++;
    }
    if (wdt-wdt_joy_cmd[1] > 0.200)
    {
        joy_desired_direction[1]=0;
        joy_linear_speed[1]=0;
        joy_angular_speed[1]=0;
        joy_pwm_gain[1]=0;
        joy_timeout_counter[1]++;
    }
    if (wdt-wdt_aux_cmd > 0.200)
    {
        aux_desired_direction=0;
        aux_linear_speed=0;
        aux_angular_speed=0;
        aux_pwm_gain=0;
        aux_timeout_counter++;
    }
    if (wdt-wdt_ros_cmd > 0.500)
    {
        ros_desired_direction=0;
        ros_linear_speed=0;
        ros_angular_speed=0;
        ros_pwm_gain=0;
        ros_timeout_counter++;
    }

    if (wdt-wdt_old > 0.040) { thread_timeout_counter++;  }
    wdt_old=wdt;

    if (joystick_received[0]>0)  { joystick_received[0]--;  }
    if (joystick_received[1]>0)  { joystick_received[1]--;  }
    if (command_received>0)    { command_received--;   }
    if (auxiliary_received>0)  { auxiliary_received--; }
    if (rosInput_received>0)   { rosInput_received--; }
}
