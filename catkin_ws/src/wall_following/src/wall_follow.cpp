/**
 * @file wall_follow.cpp
 * @author Nathaniel Mallick (nmm109@pitt.edu)
 * @brief This file follows lab 3 of the F1Tenth lab modules 
 *          (https://f1tenth-coursekit.readthedocs.io/en/stable/assignments/labs/lab3.html)
 * @version 0.1
 * @date 2022-07-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <ros/ros.h> 

#include <sensor_msgs/Image.h>
#include <sensor_msgs/LaserScan.h>
#include <ackermann_msgs/AckermannDriveStamped.h>
#include <ackermann_msgs/AckermannDrive.h>

#include <std_msgs/Int32MultiArray.h>
#include <nav_msgs/Odometry.h>

#include <cmath>
#define pi M_PI // lazily avoiding uppercase variables for science 


class WallFollow 
{ 
    private: 
        ros::NodeHandle n; 
        ros::Publisher drive_pub; 
        ros::Subscriber scan_sub, mux_sub; 

        ros::Time curr_time; 

        std::string drive_topic; 

        int mux_idx;
        bool done;
        double rate = 60.0;   

        struct {
            double kp, ki, kd; 
        } gains; 

        struct {
            int num_scans; 
            double min_angle, max_angle,
                    scan_inc; 
        } lidar_data;

        struct {
            ros::Time time; 
            double speed; 
        } odom_data; 
        
        double err, prev_err; 
        double vel;
        double p,i,d; 

        int a_idx, b_idx; 
        double L, theta = pi/4.0; // [theta = 45 deg] (0 < theta < 70deg)

    public: 
        WallFollow(): 
            err(0.0), prev_err(0.0),   
            p(0.0), i(0.0), d(0.0), 
            n(ros::NodeHandle("~"))            
        {
            // Extract  lidar info from one message
            boost::shared_ptr<const sensor_msgs::LaserScan>
                tmp_scan = ros::topic::waitForMessage<sensor_msgs::LaserScan>("/scan", n, ros::Duration(10.0)); 
            
            if(tmp_scan != NULL) 
            { 
                lidar_data.scan_inc = tmp_scan->angle_increment;
                lidar_data.min_angle = tmp_scan->angle_min; 
                lidar_data.max_angle = tmp_scan->angle_max; 
                lidar_data.num_scans = 
                    (int)ceil((lidar_data.max_angle - lidar_data.min_angle)/lidar_data.scan_inc); 

                ROS_INFO(""); 
                ROS_INFO("Min Angle:\t%f", lidar_data.min_angle);
                ROS_INFO("Max Andgle:\t%f", lidar_data.max_angle); 
                ROS_INFO("Scan Incr:\t%f", lidar_data.scan_inc);  
                ROS_INFO("Num scans:\t%d", lidar_data.num_scans); 
                ROS_INFO("");
            } else 
            {
                ROS_INFO_ONCE("Couldn't extract lidar instrinsics... \nEXITING");
                exit(-1); 
            }

            n.getParam("wall_follow_idx", mux_idx); 
            n.getParam("wall_follow_topic", drive_topic);

            // pubs
            drive_pub = n.advertise<ackermann_msgs::AckermannDriveStamped>(drive_topic, 1); 

            // subs 
            scan_sub = n.subscribe("/scan", 1, &WallFollow::lidar_cb, this); 
            mux_sub = n.subscribe("/mux", 1, &WallFollow::mux_cb, this); 

            // We want this index the angle thats orthogonally
            // to the left of the front of the car _| 
            b_idx = (int)round((pi/2.0-lidar_data.min_angle)/lidar_data.scan_inc); 
            a_idx = (int)round((((pi/2.0)-theta)-lidar_data.min_angle)/lidar_data.scan_inc); 

            // Update theta to be MORE accurate due to rounding errors in finding our idx
            theta = lidar_data.scan_inc*(a_idx - b_idx); 
            ROS_INFO("Angle Difference: %f", theta); 
        } 

        void mux_cb(const std_msgs::Int32MultiArray &msg) 
        {
            // Set the mux idx to verify wether to 
            //  turn the PID controller on/off. 
            done = msg.data[mux_idx]; 
        }

        void lidar_cb(const sensor_msgs::LaserScan &msg)
        {
            /////!!!!! NEED TO FILTER FOR BAD DISTANCES (inf &&&& <0)
            auto a = msg.ranges[a_idx]; 
            auto b = msg.ranges[b_idx]; 
            
            auto alpha = std::atan((a*std::cos(theta)-b)/(a*std::sin(theta))); 
            auto dt = b*std::cos(alpha); 

            
            // auto dt_1 = dt + L*std::sin(alpha) // L needs to be defined using odometry!!!
            // ROS_INFO_ONCE("D_t = %f", dt); 
            // ROS_INFO_ONCE("b = %f", b); 
        }

        void pid_control(const double &err, const double &vel);
        double getRange(const sensor_msgs::LaserScan &data, const double &angle);
        double followLeft(); // need params

        bool getStatus() const 
        {
            return done; 
        }

        double getRate() const 
        {
            return rate; 
        }
}; 

int main(int argc, char **argv) 
{
    ros::init(argc, argv, "wall_follow");
    WallFollow w; 
    ros::spin(); 
    return 0; 
}