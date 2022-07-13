#include <ros/ros.h> 
#include <point_dist/PointDist.h> 
#include <sensor_msgs/LaserScan.h>
#include <algorithm>
#include <math.h> 

class PointDist
{
private: 
    ros::NodeHandle nh; 
    ros::Subscriber scan; 
    ros::Publisher max_pub, min_pub; 

public: 

    PointDist()
        : nh(ros::NodeHandle("~"))
    {   
        
        scan = nh.subscribe("/scan", 1, &PointDist::scan_cb, this); 
        max_pub = nh.advertise<point_dist::PointDist>("/farther_point", 1); 
        min_pub = nh.advertise<point_dist::PointDist>("/closest_point", 1); 
    }

    void scan_cb( const sensor_msgs::LaserScan & msg )
    {
        point_dist::PointDist max, min; 
        
        max.distance = msg.ranges[0];  
        min.distance = msg.ranges[0]; 
        max.angle = msg.angle_min;  //* (180.0/M_PI); 
        min.angle = msg.angle_min; //* (180.0/M_PI); 

        for( size_t i = 1; i < msg.ranges.size(); i ++ )
        {
            if( max.distance < msg.ranges[i] )
            {
                max.distance = msg.ranges[i];  
                max.angle = (msg.angle_min + (i*msg.angle_increment));
            }

            if( min.distance > msg.ranges[i] ) 
            {
                min.distance = msg.ranges[i]; 
                min.angle = (msg.angle_min + (i*msg.angle_increment));
            } 
        }

        max_pub.publish(max);
        min_pub.publish(min); 
    }

}; 

int main(int argc, char **argv) 
{
    ros::init(argc, argv, "point_dist");
    PointDist p; 
    ros::spin(); 
    return 0; 
}