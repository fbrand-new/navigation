/*
 * SPDX-FileCopyrightText: 2024 Istituto Italiano di Tecnologia (IIT)
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file Follower.h
 * @authors: Valentina Gaggero <valentina.gaggero@iit.it>
 */

#ifndef FOLLOWER_H
#define FOLLOWER_H

#include <string>
#include <mutex>
#include <tuple>

#include <yarp/os/ResourceFinder.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/Bottle.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/dev/IFrameTransform.h>

#include "TargetRetriever.h"
#include "SimFramePainter.h"
#include "GazeController.h"
#include "NavigationController.h"
#include "ObstacleAvoidance.h"
#include "navigation_defines.h"

namespace FollowerTarget
{

    class FollowerConfig
    {
    public:
        std::string inputPortName;
        std::string outputPortName;
        std::string targetType;
        int invalidTargetMax;
        bool startWithoutCommand;
        bool onSimulator;
        struct
        {
            double factorDist2Vel;
            double factorAng2Vel;
            double distanceThreshold;
            double angleThreshold;
            struct
            {
                double min;
                double max;
            }angleLimitsVelReduction;
            struct
            {
                double angular;
                double linear;
            }velocityLimits;
        } navigation;

        struct
        {
            bool enabled;
            bool paintGazeFrame;
            double period;
        }debug;


        FollowerConfig()
        {
            //init with default values
            navigation.factorDist2Vel = 0.8;
            navigation.factorAng2Vel = 0.8;
            navigation.distanceThreshold = 0.8;
            navigation.angleThreshold = 3.0;
            navigation.velocityLimits.angular = 30; //degree/sec
            navigation.velocityLimits.linear = 3; //m/s
            navigation.angleLimitsVelReduction.min = 20.0; //degree
            navigation.angleLimitsVelReduction.max = 40.0; //degree
            inputPortName = "targetPoint";
            outputPortName = "commands";
            targetType = "person";
            debug.enabled=false;
            debug.paintGazeFrame = false;
            debug.period = 0.5;
            startWithoutCommand = false;
            invalidTargetMax = 10;
            onSimulator=true;
        }

        void print(void);
    };

    enum class TargetType_t
    {
        redball, person, fakeHumanModel
    };

    enum class StateMachine
    {
        none       = 0, //at creation time or if configuration fails
        configured = 1, //if configuration has success or stop received
        running    = 2, //after start(tick) received,
        error      = 3
    };

    enum class RunningSubStMachine //these are the states of sub state machine when it is in running
    {
        unknown                  = 0,
        targetValid              = 1,
        maybeLostTarget          = 2,
        lostTarget_lookup        = 3,
        startAutoNav             = 4,
        waitAutoNav              = 5,
        autoNavOk                = 6,
        autoNavError             = 7,
        needHelp                 = 8
    };

    enum class SMEvents
    {
        validTargetRec=1,
        invalidTargetRec=2,
        lookupFinished=3,
        autoNavFinished=4,
        error=5
    };

    enum class Result_t
    {
        ok,
        notRunning,
        lostTarget,
        autoNavigation,
        error,
        needHelp,
        failed
    };

    enum class DebugLevel_t
    {
        general=1,
        targetRetriever=2,
        gazeController=3,
        navigationController=4,
        ObstacleVerifier=5,
        DurationStatisticsInfo=6
    };

    using FollowerSMTransition=std::tuple<RunningSubStMachine, RunningSubStMachine, SMEvents>;
    class Follower
    {
    public:

        Follower();
        ~Follower();
        bool configure(yarp::os::ResourceFinder &rf);
        Result_t followTarget(Target_t &target);
        bool start(void);
        bool stop(void);
        bool close(); // Close function, to perform cleanup.
        TargetType_t getTargetType(void);
        StateMachine getState(void);
        FollowerSMTransition getSmTransion(void);

        bool helpProvided(void); //for test purpose
        void printDebugInfo(Target_t &currenttarget);
        void setDebug(DebugLevel_t level, bool on);
        void setGazeTimeout_debug(double t) {m_gazeCtrl.setGazeTimeout_debug(t);} //TODO: to remove after debug
        std::string runStMachineState_2_string(RunningSubStMachine st);

    private:

        struct
        {
            yarp::dev::IFrameTransform* transformClient;
            yarp::dev::PolyDriver      driver;

            const std::string redBallFrameId = "head_leopard_left";
            const std::string personFrameId = "depth_center";
            const std::string baseFrameId = "mobile_base_body_link";
            std::string targetFrameId;
        }m_transformData;


        yarp::os::Port m_rpcPort;
        yarp::os::BufferedPort<yarp::os::Bottle>  m_outputPort2baseCtr; //I send commands to baseControl interruptModule

        FollowerConfig m_cfg;
        TargetType_t m_targetType;

        StateMachine m_stateMachine_st;
        RunningSubStMachine m_runStMachine_st;
        //yarp::sig::Vector m_lastValidPoint; //respect mobile_base_body_link frame here I save the point of m_lastValidTarget stransformed respect mobile_base_body_link
        //Target_t m_lastValidTarget;
        Target_t m_lastValidTargetOnBaseFrame;
        uint32_t m_lostTargetcounter;

        bool m_autoNavAlreadyDone;
        bool m_targetReached;

        std::mutex m_mutex;

        GazeController m_gazeCtrl;
        NavigationController m_navCtrl;
        SimManager * m_simmanager_ptr;
        Obstacle::ObstacleVerifier m_obsVer;
        Obstacle::Result m_obsVerResult;

        FollowerSMTransition m_transition;

        double m_debugTimePrints;
        bool transformPointInBaseFrame(Target_t &validTarget, yarp::sig::Vector &pointOutput);
        bool transformPointInHeadFrame(std::string frame_src, yarp::sig::Vector &pointInput, yarp::sig::Vector &pointOutput);
        bool transformPointInCamFrame(Target_t &validTarget, yarp::sig::Vector &pointOutput);

        bool initTransformClient(void);

        bool readConfig(yarp::os::ResourceFinder &rf, FollowerConfig &cfg);

        bool sendCommand2BaseControl(double linearDirection, double linearVelocity, double angularVelocity);

        bool isOnSimulator(void) {return((m_simmanager_ptr==nullptr) ? false :true);}

        bool isInRunningState(void);
        Result_t processValidTarget(Target_t &target);
        Result_t processTarget_core(Target_t &targetOnBaseFrame);
        void goto_targetValid_state();
       // bool checkTargetIsInThreshold(yarp::sig::Vector &target);//return true if the distance is smaller the thesholddistancePrame

        // ---- TEST STUFF
        bool moveRobot(void);
        yarp::os::BufferedPort<yarp::os::Bottle>  m_outputPortJoystick;//test only!!!used in sendOutput
        void sendOutputLikeJoystick(); //only for test. it simulates joystick
        void sendtargets4Debug(yarp::sig::Vector &VonCamFrame, yarp::sig::Vector &VonBaseFrame);
        //get transform matrix from left camera to mobile base. Pf3dtraker use the left camera.
        bool getMatrix(yarp::sig::Matrix &transform);
        void printStMachineDebufInfo(Target_t &currenttarget);
        std::string stateMachineState_2_string(StateMachine st);
    };
}
#endif

