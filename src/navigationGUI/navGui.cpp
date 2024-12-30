/*
 * SPDX-FileCopyrightText: 2024 Istituto Italiano di Tecnologia (IIT)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <yarp/os/Network.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Bottle.h>
#include <yarp/os/BufferedPort.h>
#include <yarp/os/ResourceFinder.h>
#include <yarp/os/Os.h>
#include <yarp/os/Time.h>
#include <yarp/sig/Vector.h>
#include <yarp/sig/LaserMeasurementData.h>
#include <yarp/dev/Drivers.h>
#include <yarp/dev/PolyDriver.h>
#include <yarp/os/PeriodicThread.h>
#include <yarp/dev/IRangefinder2D.h>
#include <yarp/dev/INavigation2D.h>
#include <string>
#include <algorithm>
#include <yarp/cv/Cv.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include "navGui.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::dev::Nav2D;

#ifndef DEG2RAD
#define DEG2RAD M_PI/180
#endif

YARP_LOG_COMPONENT(NAVIGATION_GUI, "navigation.navigationGui")

NavGuiThread::~NavGuiThread()
{
  /* { m_ptf.close(); };
   {m_pLoc.close(); };
   {m_pLas.close(); };
   {m_pMap.close(); };
   {m_pNav.close(); };
   cvReleaseImage(&i1_map);
   cvReleaseImage(&i2_map_menu);
   cvReleaseImage(&i3_map_menu_scan);
   cvReleaseImage(&i4_map_with_path);
   m_estimated_poses->push_back(Map2DLocation());
   m_estimated_poses->clear();
   delete m_estimated_poses;
   yCDebug() << "";*/
}

bool NavGuiThread::click_in_menu(yarp::os::Bottle *gui_targ, yarp::math::Vec2D<int>& click_p)
{
    int yoff = i1_map->height;
    int xw = i1_map->width;
    int yh = i1_map->height;
    click_p.x = (*gui_targ).get(0).asInt32();
    click_p.y = (*gui_targ).get(1).asInt32();
    if (click_p.x > 0 && click_p.x < xw &&
        click_p.y > button1_t && click_p.y < button1_b)
    {
        click_p.x -= 0;
        click_p.y -= 0;
        return true;
    }
    else
    {
        return false;
    }
}

void NavGuiThread::readTargetFromYarpView()
{
    yarp::os::Bottle *gui_targ = m_port_yarpview_target_input.read(false);
    if (gui_targ)
    {
        yarp::math::Vec2D<int> click_p;
        if (click_in_menu(gui_targ, click_p))
        {
            if (click_p.x > button1_l && click_p.x < button1_r &&
                click_p.y > button1_t && click_p.y < button1_b)
                {
                    m_iNav->stopNavigation();
                }
            else
                if (click_p.x > button2_l && click_p.x < button2_r &&
                    click_p.y > button2_t && click_p.y < button2_b)
                {
                    if (m_navigation_status== navigation_status_moving)
                    {
                        m_iNav->suspendNavigation();
                    }
                    else if (m_navigation_status == navigation_status_paused)
                    {
                        m_iNav->resumeNavigation();
                    }
                }
            else
                if (click_p.x > button3_l && click_p.x < button3_r &&
                    click_p.y > button3_t && click_p.y < button3_b)
                {
                    if (button3_status == button_status_localize)
                    {
                        button3_status = button_status_goto;
                    }
                    else
                    {
                        button3_status = button_status_localize;
                    }
                }
                else
                    if (click_p.x > button4_l && click_p.x < button4_r &&
                        click_p.y > button4_t && click_p.y < button4_b)
                    {
                        m_iNav->recomputeCurrentNavigationPath();
                    }
            return;
        }

        if (gui_targ->size() == 2)
        {
            XYCell c_end_gui;
            c_end_gui.x = (*gui_targ).get(0).asInt32();
            c_end_gui.y = (*gui_targ).get(1).asInt32();
            yarp::sig::Vector v = static_cast<yarp::sig::Vector>(m_current_map.cell2World(c_end_gui));
            yCInfo(NAVIGATION_GUI,"selected point is located at (%6.3f, %6.3f)", v[0], v[1]);
            Map2DLocation loc;
            loc.map_id = m_localization_data.map_id;
            loc.x = v[0];
            loc.y = v[1];
            loc.theta = 0; //@@@@TO BE IMPROVED
            if (button3_status == button_status_goto)
            {
                m_iNav->gotoTargetByAbsoluteLocation(loc);
            }
            else if (button3_status == button_status_localize)
            {
                m_iNav->setInitialPose(loc);
            }
            else
            {
                yCError(NAVIGATION_GUI) << "Invalid button state";
            }
        }
        else if (gui_targ->size() == 4)
        {
            XYCell c_start_gui;
            XYCell c_end_gui;
            XYWorld c_start_world;
            XYWorld c_end_world;
            c_start_gui.x = (*gui_targ).get(0).asInt32();
            c_start_gui.y = (*gui_targ).get(1).asInt32();
            c_end_gui.x = (*gui_targ).get(2).asInt32();
            c_end_gui.y = (*gui_targ).get(3).asInt32();
            c_start_world = (m_current_map.cell2World(c_start_gui));
            c_end_world = (m_current_map.cell2World(c_end_gui));
            double angle = atan2(c_end_world.y - c_start_world.y, c_end_world.x - c_start_world.x) * 180.0 / M_PI;
            yarp::sig::Vector v = static_cast<yarp::sig::Vector>(c_start_world);
            yCInfo(NAVIGATION_GUI,"selected point is located at (%6.3f, %6.3f), angle: %6.3f", v[0], v[1], angle);
            Map2DLocation loc;
            loc.map_id = m_localization_data.map_id;
            loc.x = v[0];
            loc.y = v[1];
            loc.theta = angle;
            if (button3_status == button_status_goto)
            {
                m_iNav->gotoTargetByAbsoluteLocation(loc);
            }
            else if (button3_status == button_status_localize)
            {
                m_iNav->setInitialPose(loc);
            }
            else
            {
                yCError(NAVIGATION_GUI) << "Invalid button state";
            }
        }
        else
        {
            yCError(NAVIGATION_GUI) << "Received data with an invalid format.";
        }
    }
}

bool  NavGuiThread::updateLocations()
{
    std::vector<std::string> all_locations;
    m_iMap->getLocationsList(all_locations);
    Map2DLocation tmp_loc;
    m_locations_list.clear();
    for (size_t i=0; i<all_locations.size(); i++)
    {
        m_iMap->getLocation(all_locations[i],tmp_loc);
        if (tmp_loc.map_id == m_current_map.m_map_name)
            {m_locations_list.push_back(tmp_loc);}
    }
    return true;
}

bool  NavGuiThread::updateAreas()
{
    std::vector<std::string> all_areas;
    m_iMap->getAreasList(all_areas);
    Map2DArea tmp_area;
    m_areas_list.clear();
    for (size_t i = 0; i<all_areas.size(); i++)
    {
        m_iMap->getArea(all_areas[i], tmp_area);
        if (tmp_area.map_id == m_current_map.m_map_name)
            {m_areas_list.push_back(tmp_area);}
    }
    return true;
}

bool  NavGuiThread::readMaps()
{
    bool ret = true;
    ret &= m_iNav->getCurrentNavigationMap(yarp::dev::Nav2D::global_map, m_current_map);
    ret &= m_iNav->getCurrentNavigationMap(yarp::dev::Nav2D::NavigationMapTypeEnum::local_map, m_temporary_obstacles_map);
    return ret;
}

bool  NavGuiThread::readLocalizationData()
{
    bool ret = m_iLoc->getCurrentPosition(m_localization_data);
    if (ret)
    {
        m_loc_timeout_counter = 0;
    }
    else
    {
        m_loc_timeout_counter++;
        if (m_loc_timeout_counter>TIMEOUT_MAX) m_loc_timeout_counter = TIMEOUT_MAX;
        return false;
    }

    return true;
}

bool  NavGuiThread::readNavigationStatus(bool& changed)
{
    static double last_print_time = 0;

    //read the navigation status
    m_iNav->getNavigationStatus(m_navigation_status);
    if (m_navigation_status != m_previous_navigation_status)
    {
        changed = true;
    }
    else
    {
        changed = false;
    }
    m_previous_navigation_status = m_navigation_status;

    if (m_navigation_status == navigation_status_error)
    {
        if (yarp::os::Time::now() - last_print_time > 1.0)
        {
            yCError(NAVIGATION_GUI) << "Navigation status = error";
            last_print_time = yarp::os::Time::now();
        }
        return false;
    }
    return true;
}

void  NavGuiThread::readLaserData()
{
    std::vector<yarp::sig::LaserMeasurementData> scan;
    bool ret = m_iLaser->getLaserMeasurement(scan);

    if (ret)
    {
        m_laser_map_cells.clear();
        size_t scansize = scan.size();
        for (size_t i = 0; i<scansize; i++)
        {
            double las_x = 0;
            double las_y = 0;
            scan[i].get_cartesian(las_x, las_y);
            //performs a rotation from the robot to the world reference frame
            XYWorld world;
            double ss = sin(m_localization_data.theta * DEG2RAD);
            double cs = cos(m_localization_data.theta * DEG2RAD);
            world.x = las_x*cs - las_y*ss + m_localization_data.x;
            world.y = las_x*ss + las_y*cs + m_localization_data.y;
        //    if (!std::isinf(world.x) &&  !std::isinf(world.y))
            if (std::isfinite(world.x) && std::isfinite(world.y))
               { m_laser_map_cells.push_back(m_current_map.world2Cell(world));}
        }
        m_laser_timeout_counter = 0;
    }
    else
    {
        m_laser_timeout_counter++;
    }
}

bool prepare_image(IplImage* & image_to_be_prepared, const IplImage* template_image)
{
    if (template_image == 0)
    {
        yCError(NAVIGATION_GUI) << "PlannerThread::draw_map cannot copy an empty image!";
        return false;
    }
    if (image_to_be_prepared == 0)
    {
        image_to_be_prepared = cvCloneImage(template_image);
    }
    else if (image_to_be_prepared->width != template_image->width ||
             image_to_be_prepared->height != template_image->height)
    {
        cvResize(template_image, image_to_be_prepared);
        cvCopy(template_image, image_to_be_prepared);
    }
    else
    {
        cvCopy(template_image, image_to_be_prepared);
    }
    return true;
}

void NavGuiThread::addMenu(CvFont& font)
{
    button1_t = i1_map->height;
    button1_b = button1_t + 20;
    button2_t = i1_map->height;
    button2_b = button2_t + 20;
    button3_t = i1_map->height;
    button3_b = button3_t + 20;
    button4_t = i1_map->height;
    button4_b = button4_t + 20;
    cvRectangle(i2_map_menu, cvPoint(button1_l, button1_t), cvPoint(button1_r, button1_b), cvScalar(200, 50, 50), -1);
    cvRectangle(i2_map_menu, cvPoint(button2_l, button2_t), cvPoint(button2_r, button2_b), cvScalar(50, 200, 50), -1);
    cvRectangle(i2_map_menu, cvPoint(button3_l, button3_t), cvPoint(button3_r, button3_b), cvScalar(50, 50, 200), -1);
    cvRectangle(i2_map_menu, cvPoint(button4_l, button4_t), cvPoint(button4_r, button4_b), cvScalar(50, 150, 200), -1);
    cvRectangle(i2_map_menu, cvPoint(button1_l + 2, button1_t + 2), cvPoint(button1_r - 2, button1_b - 2), cvScalar(0, 0, 0));
    cvRectangle(i2_map_menu, cvPoint(button2_l + 2, button2_t + 2), cvPoint(button2_r - 2, button2_b - 2), cvScalar(0, 0, 0));
    cvRectangle(i2_map_menu, cvPoint(button3_l + 2, button3_t + 2), cvPoint(button3_r - 2, button3_b - 2), cvScalar(0, 0, 0));
    cvRectangle(i2_map_menu, cvPoint(button4_l + 2, button4_t + 2), cvPoint(button4_r - 2, button4_b - 2), cvScalar(0, 0, 0));
    char txt[255];

    //button 1
    if (m_navigation_status == navigation_status_moving ||
        m_navigation_status == navigation_status_paused )
    {
        snprintf(txt, 255, "Stop");
    }
    else
    {
        snprintf(txt, 255, "- - -");
    }
    cvPutText(i2_map_menu, txt, cvPoint(button1_l + 5, button1_t + 12), &font, cvScalar(0, 0, 0));

    //button 4
    if (m_navigation_status == navigation_status_moving ||
        m_navigation_status == navigation_status_paused ||
        m_navigation_status == navigation_status_failing ||
        m_navigation_status == navigation_status_waiting_obstacle)
    {
        snprintf(txt, 255, "Recompute");
    }
    else
    {
        snprintf(txt, 255, "- - -");
    }
    cvPutText(i2_map_menu, txt, cvPoint(button4_l + 5, button4_t + 12), &font, cvScalar(0, 0, 0));

    //button 2
    if (m_navigation_status == navigation_status_moving)
    {
        snprintf(txt, 255, "Suspend");
    }
    else if (m_navigation_status == navigation_status_paused)
    {
        snprintf(txt, 255, "Resume");
    }
    else
    {
        snprintf(txt, 255, "- - -");
    }
    cvPutText(i2_map_menu, txt, cvPoint(button2_l + 5, button2_t + 12), &font, cvScalar(0, 0, 0));

    //button 3
    if (button3_status == button_status_goto)
    {
        snprintf(txt, 255, "Goto");
    }
    else if (button3_status == button_status_localize)
    {
        snprintf(txt, 255, "Localize");
    }
    cvPutText(i2_map_menu, txt, cvPoint(button3_l + 5, button3_t + 12), &font, cvScalar(0, 0, 0));
}

void NavGuiThread::draw_and_send()
{
    CvFont font;
    cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 0.28, 0.28);
    CvFont font2;
    cvInitFont(&font2, CV_FONT_HERSHEY_SIMPLEX, 0.33, 0.33);
    static CvScalar red_color   = cvScalar(200, 80, 80);
    static CvScalar green_color = cvScalar(80, 200, 80);
    static CvScalar blue_color  = cvScalar(0, 0, 200);
    static CvScalar azure_color = cvScalar(80, 80, 200);
    static CvScalar azure_color2 = cvScalar(130, 130, 200);
    XYCell current_position = m_current_map.world2Cell(XYWorld(m_localization_data.x, m_localization_data.y));
    XYCell final_goal = m_current_map.world2Cell(XYWorld(m_curr_goal.x, m_curr_goal.y));

    if (i1_map == nullptr)
    {
#if 1
        yarp::sig::ImageOf<yarp::sig::PixelRgb> map_image;
        m_current_map.getMapImage(map_image);
        IplImage* tmp = (IplImage*)map_image.getRawImage();
        int w = tmp->width;
        int h = tmp->height;
        if (w < 320) w = 320;
        i1_map = cvCreateImage(cvSize(w, h), 8, 3);
        cvCopyMakeBorder(tmp, i1_map, cvPoint(0, 0), cv::BORDER_ISOLATED);
#else
        yarp::sig::ImageOf<yarp::sig::PixelRgb> map_image;
        m_current_map.getMapImage(map_image);
        auto tmp = yarp::cv::toCvMat(map_image);
        int w = tmp.cols;
        int h = tmp.rows;
        if (w < 320) w = 320;
        i1_map = cvCreateImage(CvSize(w, h), 8, 3);
        cvCopyMakeBorder(&tmp, i1_map, CvPoint(0, 0), cv::BORDER_ISOLATED);
#endif

    }
    if (i2_map_menu == nullptr)
    {
        //i2_map_menu = cvCreateImage(CvSize(i1_map->width, i1_map->height), 8, 3);
        //cvCopy(i1_map, i2_map_menu);
        i2_map_menu = cvCreateImage(cvSize(i1_map->width, i1_map->height+40), 8, 3);
        cvCopyMakeBorder(i1_map, i2_map_menu, cvPoint(0, 0), cv::BORDER_ISOLATED);
    }
    addMenu(font);

    if (i3_map_menu_scan == nullptr)
    {
        i3_map_menu_scan = cvCreateImage(cvSize(i2_map_menu->width, i2_map_menu->height), 8, 3);
    }
    cvCopy(i2_map_menu, i3_map_menu_scan);

    //############### draw laser
    if (m_laser_timeout_counter<TIMEOUT_MAX)
    {
        if (m_enable_draw_enlarged_scans)
        {
            map_utilites::drawLaserMap(i3_map_menu_scan, m_temporary_obstacles_map, azure_color);
        }
        if (m_enable_draw_laser_scans)
        {
            map_utilites::drawLaserScan(i3_map_menu_scan, m_laser_map_cells, blue_color);
        }
    }

    //############### draw goal
    switch (m_navigation_status)
    {
        case navigation_status_preparing_before_move:
        case navigation_status_moving:
        case navigation_status_waiting_obstacle:
        case navigation_status_aborted:
        case navigation_status_failing:
        case navigation_status_paused:
        case navigation_status_thinking:
            map_utilites::drawGoal(i3_map_menu_scan, m_current_map, m_curr_goal, red_color);
        break;
        case navigation_status_goal_reached:
            map_utilites::drawGoal(i3_map_menu_scan, m_current_map, m_curr_goal, green_color);
        break;
        case navigation_status_idle:
        default:
            //do nothing
        break;
    }

    //############### draw localization particles
    int particles_to_be_drawn = std::min((int)m_enable_estimated_particles, (int)m_estimated_poses.size());
    for (size_t i = 0; i < particles_to_be_drawn; i++)
    {
         map_utilites::drawPose(i3_map_menu_scan, m_current_map, m_estimated_poses[i], green_color);
    }


    //############### draw locations
    if (m_enable_draw_all_locations)
    {
        for (size_t i=0; i<m_locations_list.size(); i++)
        {
            map_utilites::drawGoal(i3_map_menu_scan, m_current_map, m_locations_list[i], blue_color);
        }
        for (size_t i = 0; i<m_areas_list.size(); i++)
        {
            std::vector<XYCell> area;
            for (size_t j = 0; j < m_areas_list[i].points.size(); j++)
            {
                area.push_back(m_current_map.world2Cell(XYWorld(m_areas_list[i].points[j].x, m_areas_list[i].points[j].y)));
            }
            map_utilites::drawArea(i3_map_menu_scan, area, blue_color);
        }

    }

    //############### draw Current Position
    map_utilites::drawCurrentPosition(i3_map_menu_scan, m_current_map, m_localization_data, azure_color);

    //############### draw Infos
    if (m_enable_draw_infos)
    {
        XYWorld w_x_axis; w_x_axis.x = 2; w_x_axis.y = 0;
        XYWorld w_y_axis; w_y_axis.x = 0; w_y_axis.y = 2;
        XYWorld w_orig; w_orig.x = 0; w_orig.y = 0;
        XYCell x_axis = m_current_map.world2Cell(w_x_axis);
        XYCell y_axis = m_current_map.world2Cell(w_y_axis);
        XYCell orig = m_current_map.world2Cell(w_orig);
//        map_utilites::drawInfo(i3_map_menu_scan, current_position, orig, x_axis, y_axis, getNavigationStatusAsString(), m_localization_data, font, blue_color);

        XYCell whereToDraw(10, i1_map->height+32);
        map_utilites::drawInfoFixed(i3_map_menu_scan, whereToDraw, orig, x_axis, y_axis, getNavigationStatusAsString(), m_localization_data, font2, azure_color2);
    }

    //############### draw path
    prepare_image(i4_map_with_path, i3_map_menu_scan);

    CvScalar color = cvScalar(0, 200, 0);
    CvScalar color2 = cvScalar(0, 200, 100);
    CvScalar color3 = cvScalar(0, 50, 0);

    if (m_navigation_status != navigation_status_idle &&
        m_navigation_status != navigation_status_goal_reached &&
        m_navigation_status != navigation_status_aborted &&
        m_navigation_status != navigation_status_error &&
        m_navigation_status != navigation_status_failing)
        {
            if (m_enable_draw_global_path)
            {
                std::queue <XYCell> all_waypoints_cell;
                for (int i = 0; i < m_global_waypoints.size(); i++)
                {
                    XYWorld curr_waypoint_world(m_global_waypoints[i].x, m_global_waypoints[i].y);
                    XYCell curr_waypoint_cell = m_current_map.world2Cell(curr_waypoint_world);
                    all_waypoints_cell.push(curr_waypoint_cell);
                }

                XYWorld curr_waypoint_world(m_curr_waypoint.x, m_curr_waypoint.y);
                XYCell curr_waypoint_cell = m_current_map.world2Cell(curr_waypoint_world);
                map_utilites::drawPath(i4_map_with_path, current_position, curr_waypoint_cell, all_waypoints_cell, color3, color2);
            }
            if (m_enable_draw_local_path)
            {
                std::queue <XYCell> all_waypoints_cell;
                for (int i = 0; i < m_local_waypoints.size(); i++)
                {
                    XYWorld curr_waypoint_world(m_local_waypoints[i].x, m_local_waypoints[i].y);
                    XYCell curr_waypoint_cell = m_current_map.world2Cell(curr_waypoint_world);
                    all_waypoints_cell.push(curr_waypoint_cell);
                }

                XYWorld curr_waypoint_world(m_curr_waypoint.x, m_curr_waypoint.y);
                XYCell curr_waypoint_cell = m_current_map.world2Cell(curr_waypoint_world);
                map_utilites::drawPath(i4_map_with_path, current_position, curr_waypoint_cell, all_waypoints_cell, color3, color2);
            }
        }

    //############### finished, send to port
    map_utilites::sendToPort(&m_port_map_output, i4_map_with_path);
}

void NavGuiThread::run()
{
    std::lock_guard<std::mutex> lock(m_guithread_mutex);
    //double check1 = yarp::os::Time::now();

    bool navstatus_changed = false;
    readNavigationStatus(navstatus_changed);
    readTargetFromYarpView();
    readLocalizationData();

    static double last_drawn_laser = yarp::os::Time::now();
    if (yarp::os::Time::now() - last_drawn_laser > m_period_update_laser_data)
    {
        readLaserData();
        last_drawn_laser = yarp::os::Time::now();
    }

    static double last_updated_enlarged_obstacles = yarp::os::Time::now();
    if (yarp::os::Time::now() - last_updated_enlarged_obstacles > m_period_update_enlarged_obstacles)
    {
        m_iNav->getCurrentNavigationMap(yarp::dev::Nav2D::NavigationMapTypeEnum::local_map, m_temporary_obstacles_map);
        last_updated_enlarged_obstacles = yarp::os::Time::now();
    }

    static double last_updated_global_map = yarp::os::Time::now();
    if (yarp::os::Time::now() - last_updated_global_map > m_period_update_global_map)
    {
        //@@@check this, untested
        m_iNav->getCurrentNavigationMap(yarp::dev::Nav2D::NavigationMapTypeEnum::global_map, m_current_map);
        last_updated_global_map = yarp::os::Time::now();
    }

    static double last_updated_estimated_poses = yarp::os::Time::now();
    if (yarp::os::Time::now() - last_updated_estimated_poses > m_period_update_estimated_poses)
    {
        m_iNav->getEstimatedPoses(m_estimated_poses);
        last_updated_estimated_poses = yarp::os::Time::now();
    }

    static double last_updated_map_locations = yarp::os::Time::now();
    if (yarp::os::Time::now() - last_updated_map_locations > m_period_update_map_locations)
    {
        updateLocations();
        updateAreas();
        last_updated_map_locations = yarp::os::Time::now();
    }

    //double check2 = yarp::os::Time::now();
    //yCDebug() << check2-check1;

    switch (m_navigation_status)
    {
        case navigation_status_moving:
             readWaypointsAndGoal();
        break;

        case navigation_status_goal_reached:
        case navigation_status_idle:
        case navigation_status_thinking:
        case navigation_status_aborted:
        case navigation_status_failing:
        case navigation_status_error:
        case navigation_status_paused:
        case navigation_status_waiting_obstacle:
        case navigation_status_preparing_before_move:
            //do nothing, just wait
        break;

        default:
              //unknown status
              yCError(NAVIGATION_GUI,"unknown status:%d", m_navigation_status);
              m_navigation_status = navigation_status_error;
        break;
    }

    static double last_drawn = yarp::os::Time::now();
    double elapsed_time = yarp::os::Time::now() - last_drawn;
    if ( elapsed_time > m_imagemap_draw_and_send_period)
    {
        //double check3 = yarp::os::Time::now();
        draw_and_send();
        //double check4 = yarp::os::Time::now();
        //yCDebug() << check4-check3;
        last_drawn = yarp::os::Time::now();
    }
}

bool NavGuiThread::readWaypointsAndGoal()
{
    if (m_iNav)
    {
        m_iNav->getCurrentNavigationWaypoint(m_curr_waypoint);
        m_iNav->getAllNavigationWaypoints(yarp::dev::Nav2D::global_trajectory,m_global_waypoints);
        m_iNav->getAllNavigationWaypoints(yarp::dev::Nav2D::local_trajectory, m_local_waypoints);
        m_iNav->getAbsoluteLocationOfCurrentTarget(m_curr_goal);
    }
    return true;
}

void NavGuiThread::sendTargetFromYarpView()
{
}


