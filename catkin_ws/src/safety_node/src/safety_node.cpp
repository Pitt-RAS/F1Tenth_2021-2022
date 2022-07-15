#include <ros/ros.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/LaserScan.h>
// TODO: include ROS msg type headers and libraries
#include <ackermann_msgs/AckermannDriveStamped.h>
#include <std_msgs/Bool.h>
#include <cmath> 

#define PI M_PI

// (TODO) [nmm] Add a library that has predifined macros and structs
struct car_intrinsics
{ 
        float width, wheelbase, base_link;  
};

struct lidar_intrinsics 
{ 
    double num_scans,
           scan_inc,
           min_angle,
           max_angle; 
}; 

std::vector<double> compute_car_perim(
    const car_intrinsics& car_data, const lidar_intrinsics& lidar_data)
{
    std::vector<double> car_perim; 
    car_perim.reserve(lidar_data.num_scans); 
    auto angle = lidar_data.min_angle; 

    for( size_t i = lidar_data.min_angle; i < lidar_data.num_scans; i++ ) 
    { 
        if(angle > 0.0) // top half
            if(angle < PI/2) // First Quadrant 
            {
                auto right_side = (car_data.width/2)/std::cos(angle);
                auto top_right = (car_data.wheelbase - car_data.base_link)/std::cos((PI/2)-angle); 
                car_perim[i] = std::min(right_side, top_right);  
            }
            else // Second Quadrant
            {
                auto top_left = (car_data.wheelbase - car_data.base_link)/std::cos(angle - (PI/2));
                auto left_side = (car_data.width/2)/std::cos(PI - angle);  
                car_perim[i] = std::min(left_side, top_left); 
            }
        else // bottom half
            if(angle < -PI/2) // Third Quadrant
            {
                auto left_side = (car_data.width/2)/std::cos(PI + angle);
                auto bottom_left = (car_data.base_link)/std::cos(-angle - (PI/2)); 
                car_perim[i] = std::min(left_side, bottom_left); 
            }
            else // Fourth Quadrant
            {
                auto bottom_right = (car_data.base_link)/std::cos((PI/2)-angle); 
                auto right_side = (car_data.width/2)/std::cos(-angle);
                car_perim[i] = std::min(bottom_right, right_side); 
            }
    }
    return car_perim; 
} 

class Safety {
// The class that handles emergency braking
private:
    ros::NodeHandle n;

    ros::Subscriber scan_sub, odom_sub; 
    ros::Publisher brake_pub, speed_pub; 

    // Info to perform emergency braking 
    std::vector<double> car_perimeter; 
    lidar_intrinsics lidar; 
    car_intrinsics car; 
    double ttc_threshold = 0.01; 
    double speed;

    // Data to publish
    struct {
        std_msgs::Bool brake;
        ackermann_msgs::AckermannDriveStamped speed;    
    } brake_msg; 

public:
    Safety() 
    {
        n = ros::NodeHandle("~");
        speed = 0.0; 

        // Initialize brake message
        brake_msg.brake.data = true; 
        brake_msg.speed.drive.speed = 0.0; 
        
        // Listening to one scan message to grab LIDAR instrinsics 
        boost::shared_ptr<const sensor_msgs::LaserScan> 
            shared = ros::topic::waitForMessage<sensor_msgs::LaserScan>("/scan", n);
        
        if( shared != NULL )
        {
            lidar.scan_inc = shared->angle_increment;
            lidar.max_angle = shared->angle_max; 
            lidar.min_angle = shared->angle_max; 
            lidar.num_scans = (lidar.max_angle - lidar.min_angle)/lidar.scan_inc; 
        } 

        /*
        One publisher should publish to the /brake topic with an
        ackermann_msgs/AckermannDriveStamped brake message.

        One publisher should publish to the /brake_bool topic with a
        std_msgs/Bool message.

        You should also subscribe to the /scan topic to get the
        sensor_msgs/LaserScan messages and the /odom topic to get
        the nav_msgs/Odometry messages

        The subscribers should use the provided odom_callback and 
        scan_callback as callback methods

        NOTE that the x component of the linear velocity in odom is the speed
        */

        // [ Pubs ]
            /* Brake-bool Publisher */
        brake_pub = n.advertise<std_msgs::Bool>("/brake_bool", 1); 
            /* Brake-speed Publisher */
        speed_pub = n.advertise<ackermann_msgs::AckermannDriveStamped>("/brake", 1); 

        // [ Subs ]
            /* Scan Subscriber*/
        scan_sub = n.subscribe("/scan", 1, &Safety::scan_callback, this);
            /* Odom Subscriber */ 
        odom_sub = n.subscribe("/scan", 1, &Safety::odom_callback, this); 

        n.getParam("width", car.width); 
        n.getParam("scan_distance_to_base_link", car.base_link); 
        n.getParam("wheelbase", car.wheelbase);
        n.getParam("scan_beams", lidar.num_scans);

        // Compute the perimeter of the car
        compute_car_perim(car, lidar); 
    }   

    void odom_callback(const nav_msgs::Odometry::ConstPtr &odom_msg) 
    {
        speed = odom_msg->twist.twist.linear.x; // Update current speed. 
    }

    void scan_callback(const sensor_msgs::LaserScan::ConstPtr &scan_msg) 
    {
        // TODO: calculate TTC
        if(scan_msg->ranges.size() != car_perimeter.size()) 
        {
            ROS_INFO("Scan size does match precomputed size."); 
            return; 
        }
        
        // TODO: publish drive/brake message
        for( size_t i = 0 ; i < scan_msg->ranges.size(); i++ ) 
        {
            auto r_hat = speed*std::cos(i*scan_msg->angle_increment + scan_msg->angle_min); 
            if(r_hat <= 0) { continue; }
            auto ttc = (scan_msg->ranges[i] - car_perimeter[i])/r_hat; 

            if( ttc < ttc_threshold ) 
            { 
                brake_pub.publish(brake_msg.brake); 
                speed_pub.publish(brake_msg.speed); 
            }
        }
    }
};

int main(int argc, char ** argv) {
    ros::init(argc, argv, "safety_node");
    Safety sn;
    ros::spin();
    return 0;
}