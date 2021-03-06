//
// Created by lucasyu on 18-11-29.
//
#include <ros/ros.h>
#include <tf/transform_datatypes.h>
#include <pcl/common/transforms.h>
#include <pcl/io/pcd_io.h>
#include <pcl_conversions/pcl_conversions.h>
#include <sensor_msgs/PointCloud2.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/PointStamped.h>
#include <std_msgs/Float64.h>
#include <opencv2/opencv.hpp> 
#include <opencv2/imgproc/imgproc.hpp>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <sensor_msgs/image_encodings.h>
#include "/home/ubuntu/catkin_ws/devel/include/darknet_ros_msgs/BoundingBoxes.h"
#include <random>
#include <cmath>

#include <iostream>
#include <string>
#include <fstream>
#include <array>
#include <time.h>
#include <numeric>


using namespace std;
std::vector< std::vector<float> > data_pcl;
std::vector< std::vector<float> > data_uav;
std::vector< std::vector<float> > data_label;

ros::Time timestamp;
float vel_odom = 0.0f;
float angular_odom = 0.0f;
float position_odom_x = 0.0f;
float position_odom_y = 0.0f;
float position_odom_z = 0.0f;
float position_radar_x = 0.0f;
float position_radar_y = 0.0f;
float vel_smoother = 0.0f;
float angular_smoother = 0.0f;
float vel_teleop = 0.0f;
float angular_teleop = 0.0f;
float pos_target_x = 0.0f;
float pos_target_y = 0.0f;
float yaw_target = 0.0f;
float yaw_current = 0.0f;
float yaw_current_x = 0.0f;
float yaw_current_y = 0.0f;
float yaw_delt = 0.0f;
float yaw_forward = 0.0f;
float yaw_backward = 0.0f;
float yaw_leftward = 0.0f;
float yaw_rightward = 0.0f;
std::vector<int> v_rgb_one_line;
std::vector<float> v_depth_one_line;

const int num_uavdata = 17;
const int num_label = 4;
const int img_width = 64;
//const int img_height = 64;
const int img_height_uplimit = 20;
const int img_height_downlimit = 4;
const int info_dim = 1;
const int resize_rgb_width = 256;
const int resize_rgb_height = 192;

//width and height before resize:
const int IMGWIDTH = 640;
const int IMGHEIGHT = 480;

// average theta for the gaussian noise 
const float theta_mean = 3.1415926 / 6;

unsigned char semantic_labels[IMGHEIGHT][IMGWIDTH];
unsigned char semantic_labels_resized[resize_rgb_height][resize_rgb_width];

ofstream outFile_rgbdata;
ofstream outFIle_depthdata;
ofstream outFile_uavdata;
ofstream outFile_labels;
ofstream outFile_semantics;

void rotate_x_y(const float& x_origin, const float& y_origin, float& x_rotate, float& y_rotate, const float& yaw_angle)
{
    x_rotate = x_origin * cos(yaw_angle) + y_origin * sin(yaw_angle);
    y_rotate = y_origin * cos(yaw_angle) - x_origin * sin(yaw_angle);
}

void writeCsv(const std::vector<std::vector<float>> vec, const string& filename)
{
    // 写文件
    ofstream outFile;
    outFile.open(filename, ios::out); // 打开模式可省略
    for(int i_vec=0; i_vec<vec.size(); ++i_vec) {
        for (int j=0; j<vec[i_vec].size(); ++j)
        {
            char c[20];
            sprintf(c, "%f", vec[i_vec][j]);
            outFile << c << ",";
        }
        outFile << endl;
    }

    outFile.close();
}

void writeLabelData(const std::vector<float> labels)
{
    char c[50];
    for (int j=0; j<labels.size(); ++j)
    {
        sprintf(c, "%f", labels[j]);
        outFile_labels << c << ",";
    }
    outFile_labels << endl;
}

void writeCsvOneLine(const std::vector<float> uav_data, const std::vector<float> labels)
{
    char c[20];
    for (int j=0; j<uav_data.size(); ++j)
    {
        sprintf(c, "%f", uav_data[j]);
        outFile_uavdata << c << ",";
    }

    outFile_uavdata << endl;

    for (int j=0; j<labels.size(); ++j)
    {
        sprintf(c, "%f", labels[j]);
        outFile_labels << c << ",";
    }
    outFile_labels << endl;
}

void imageCb_d(const sensor_msgs::ImageConstPtr& depth_msg)
{
    cv_bridge::CvImagePtr depth_ptr;
    v_depth_one_line.clear();
    try
    {
        depth_ptr = cv_bridge::toCvCopy(depth_msg);//, sensor_msgs::image_encodings::TYPE_64FC1);
        cv::Mat depth_img = depth_ptr->image;
        
        int nr = depth_img.rows;
        int nc = depth_img.cols;
        cv::Mat depth_int(nr, nc, CV_8UC1, cv::Scalar::all(0));

        // cout << "depth_image size before: " << nr << ", " << nc << endl;

        for(int i=0; i<nr ;i++)
        {
            float *inDepth = depth_img.ptr<float>(i); // float
            uchar* inRgb = depth_int.ptr<uchar>(i);
            for(int j=0; j<nc; j++)
            {
                // int depth_normalized = inDepth[j];
                // cout << "value: " << inDepth[j] << endl;
                if (inDepth[j] > 6.3 || inDepth[j] != inDepth[j])
                {
                    inDepth[j] = 6.3;
                }
                inRgb[j] = (uchar)floor(inDepth[j] * 40.0); // 40 = 256/6.4
                // cout << "value 0: " << (int)inRgb[j] << endl;
            }
        }
        cv::Mat dst_depth;
        cv::resize(depth_int, dst_depth, cv::Size(resize_rgb_width, resize_rgb_height));

        for(int i=0; i<dst_depth.rows; i++)
        {
            const uchar* inDepth = dst_depth.ptr<uchar>(i); // float
            for(int j=0; j<dst_depth.cols; j++)
            {
                // cout << "value: " << (int)inDepth[j] << endl;
                v_depth_one_line.push_back(inDepth[j]);
            }
        }
        
        // writeCsvOneLine(data_uav_tmp, data_label_tmp);
        if (v_rgb_one_line.empty() || v_depth_one_line.empty())
        {
            return;
        }
        else{
            //write semantics into csv files:
            for (int i=0; i<resize_rgb_height; i++)
            {
                for (int j=0; j<resize_rgb_width; j++)
                {
                    char c_semantics[100];
                    sprintf(c_semantics, "%d", semantic_labels_resized[i][j]);
                    outFile_semantics << c_semantics << ",";
                }
            }
            outFile_semantics << endl;

            //write rgb data into csv files:
            char c_bgr[100];
            // cout << "size v_rgb: " << v_rgb_one_line.size() << endl; 
            for (auto & item_rgb : v_rgb_one_line)
            {
                sprintf(c_bgr, "%d", item_rgb);
                outFile_rgbdata << c_bgr << ",";
            }
            outFile_rgbdata << endl;

            //write depth data into csv files:
            char c_depth[100];
            // cout << "size v_depth_one_line: " << v_depth_one_line.size() << endl; 
            for (auto & item_depth : v_depth_one_line)
            {
                sprintf(c_depth, "%d", (int)item_depth);
                outFIle_depthdata << c_depth << ",";
            }
            outFIle_depthdata << endl;

            //write label data into csv files:
            float tmp_uav[num_uavdata] = {position_odom_x, position_odom_y, vel_odom, angular_odom, position_radar_x, 
                                    position_radar_y, pos_target_x, pos_target_y, yaw_target, yaw_current, yaw_current_x, 
                                    yaw_current_y, yaw_delt, yaw_forward, yaw_backward, yaw_leftward, yaw_rightward};
            std::vector<float>data_uav_tmp;
            data_uav_tmp.insert(data_uav_tmp.begin(), tmp_uav, tmp_uav + num_uavdata);

            float tmp_label[num_label] = {vel_smoother, angular_smoother, vel_teleop, angular_teleop};
            std::vector<float>data_label_tmp;
            data_label_tmp.insert(data_label_tmp.begin(), tmp_label, tmp_label+num_label);
            writeCsvOneLine(data_uav_tmp, data_label_tmp);

            // v_depth_one_line.clear();
            // v_rgb_one_line.clear();
            return;
        }
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
    
}


void objectsCallback(const darknet_ros_msgs::BoundingBoxes& objects)
{
    /*initialize labels*/
    for(int i = 0; i < IMGHEIGHT; i++)
    {
        for(int j = 0; j < IMGWIDTH; j++)
        {
            semantic_labels[i][j] = 0; // Set all points to common obstacles when initialzing, including NAN. NAN will not be inserted into map, so it doesn't matter.
        }
    }

    for (int i=0; i<resize_rgb_height;i++){
        for (int j=0; j<resize_rgb_width;j++){
            semantic_labels_resized[i][j] = 0;
        }
    }

    /* Set labels of each roi. If there are overlaps, only take the last input label.*/
    for (int m = 0; m < objects.bounding_boxes.size(); m++)
    {
        int label;
        /*
        0: free space
        1: unknown
        2: possible way
        3: obstacle
        4: none
        5: furniture
        6: other dynamic objects
        7: person
        */
        if(objects.bounding_boxes[m].Class == "person")
            label = 7;
        else if(objects.bounding_boxes[m].Class == "cat")
            label = 6;
        else if(objects.bounding_boxes[m].Class == "dog")
            label = 6;
        else if(objects.bounding_boxes[m].Class == "laptop")
            label = 5;
        else if(objects.bounding_boxes[m].Class == "bed")
            label = 5;
        else if(objects.bounding_boxes[m].Class == "tvmonitor")
            label = 5;
        else if(objects.bounding_boxes[m].Class == "chair")
            label = 5;
        else if(objects.bounding_boxes[m].Class == "diningtable")
            label = 5;
        else if(objects.bounding_boxes[m].Class == "sofa")
            label = 5;
        else if(objects.bounding_boxes[m].Class == "window")
            label = 2;
        else if(objects.bounding_boxes[m].Class == "")
            label = 2;
        else
            label = 3;

        unsigned int range_x_min = objects.bounding_boxes[m].xmin;
        unsigned int range_x_max = objects.bounding_boxes[m].xmax;
        unsigned int range_y_min = objects.bounding_boxes[m].ymin;
        unsigned int range_y_max = objects.bounding_boxes[m].ymax;

        if(range_x_min > IMGWIDTH - 1) range_x_min = IMGWIDTH - 1;
        if(range_x_min < 0) range_x_min = 0;
        if(range_x_max > IMGWIDTH - 1) range_x_max = IMGWIDTH - 1;
        if(range_x_max < 0) range_x_max = 0;
    
        // For object labels
        for(int i = range_y_min - 1; i < range_y_max ; i++)
        {
            for(int j = range_x_min - 1; j < range_x_max; j++)
            {
                semantic_labels[i][j] = floor(label * 32);
            }
        }
    }
    //resize process from C++ array to cv Mat
    cv::Mat semantic_img(IMGHEIGHT, IMGWIDTH, CV_8UC1, (unsigned char*)semantic_labels);
    cv::Mat dst_semantics;
    cv::resize(semantic_img, dst_semantics, cv::Size(resize_rgb_width, resize_rgb_height));
    int nr = dst_semantics.rows;
    int nc = dst_semantics.cols;
    // cout << "semantic resize size: " << nr << ", " << nc << endl;
    for (int i=0; i<nr; i++)
    {
        unsigned char* inSemantic = dst_semantics.ptr<unsigned char>(i);
        for(int j=0; j<nc; j++)
        {
            semantic_labels_resized[i][j] = inSemantic[j];
        }
    }
}


void callbackRGB(const sensor_msgs::ImageConstPtr& msg)
{
    cv_bridge::CvImagePtr cv_ptr;
    v_rgb_one_line.clear();
    try 
    {
        cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
        cv::Mat dst;
        cv::resize(cv_ptr->image, dst, cv::Size(resize_rgb_width, resize_rgb_height));
        // cout<< dst.at<float>(0,0)<<endl;
        int img_h = dst.rows;
        int img_w = dst.cols; // total number of elements per line

        // cout << "nr: "<< img_h << endl <<"nc: "<< img_w << endl;
        for (int i=0; i<img_h; i++) {
            for (int j=0; j<img_w; j++) {
                int color_b = dst.at<cv::Vec3b>(i, j)[0]; 
                int color_g = dst.at<cv::Vec3b>(i, j)[1]; 
                int color_r = dst.at<cv::Vec3b>(i, j)[2]; 
                v_rgb_one_line.push_back(color_b);
                v_rgb_one_line.push_back(color_g);
                v_rgb_one_line.push_back(color_r);
                // sprintf(c_b, "%d", color_b);
                // sprintf(c_g, "%d", color_g);
                // sprintf(c_r, "%d", color_r);

                // outFile_rgbdata << c_b << "," << c_g << "," << c_r << ",";
            }
        }

        // outFile_rgbdata << endl;
        return;
    }
    catch (cv_bridge::Exception& e)
    {   
         ROS_ERROR("cv_bridge exception: %s", e.what());
         return;
    }

}

void callBackOdom(const nav_msgs::OdometryConstPtr& odom)
{
    position_odom_x = odom->pose.pose.position.x;
    position_odom_y = odom->pose.pose.position.y;
    vel_odom = odom->twist.twist.linear.x / 0.8f;
    angular_odom = odom->twist.twist.angular.z / 0.8f;

}

void callBackRadar(const geometry_msgs::Point::ConstPtr& data)
{
    pos_target_x = data->x;
    pos_target_y = data->y;
}

void callBackTeleop(const geometry_msgs::Twist::ConstPtr& data)
{
    vel_teleop = data->linear.x / 0.8f;
    angular_teleop = data->angular.z / 0.8f;
}

void callBackCmdMobileBase(const geometry_msgs::Twist::ConstPtr& data)
{
    vel_smoother = data->linear.x / 0.8f;
    angular_smoother = data->angular.z / 0.8f;
}

void callBackTargetPos(const geometry_msgs::Point::ConstPtr& data)
{
    pos_target_x = data->x;
    pos_target_y = data->y;
}

void callBackTargetYaw(const std_msgs::Float64::ConstPtr& data)
{
    yaw_target = data->data / 3.15f;
}

void callBackCurrentYaw(const std_msgs::Float64::ConstPtr& data)
{
    yaw_current = data->data / 3.15f;
    yaw_current_x = cos(data->data);
    yaw_current_y = sin(data->data);
}

void callBackDeltYaw(const std_msgs::Float64::ConstPtr& data)
{
    yaw_delt = data->data;
    if (-3.15f/4.f < yaw_delt && yaw_delt < 3.15f/4.f){
        yaw_forward = 1.0f;
        yaw_backward = 0.0f;
        yaw_leftward = 0.0f;
        yaw_rightward = 0.0f;
    }
    else if ( 3.15f/4.f*3.f < yaw_delt || yaw_delt < -3.15f/4.f*3.f )
    {
        yaw_forward = 0.0f;
        yaw_backward = 1.0f;
        yaw_leftward = 0.0f;
        yaw_rightward = 0.0f;
    }
    else if ( 3.15f/4.f < yaw_delt && yaw_delt < 3.15f/4.f*3.f )
    {
        yaw_forward = 0.0f;
        yaw_backward = 0.0f;
        yaw_leftward = 1.0f;
        yaw_rightward = 0.0f;
    }
    else
    {
        yaw_forward = 0.0f;
        yaw_backward = 0.0f;
        yaw_leftward = 0.0f;
        yaw_rightward = 1.0f;
    }
    yaw_delt = data->data / 3.15f;

}


int main(int argc, char** argv)
{
    time_t t = time(0);
    char tmp[64];
    strftime( tmp, sizeof(tmp), "%Y_%m_%d_%X",localtime(&t) );
    
    char c_rgb_data[100];
    sprintf(c_rgb_data,"rgb_data_%s.csv",tmp);
    cout<<"----- file rgb data: "<< c_rgb_data<<endl;
    char c_depth_data[100];
    sprintf(c_depth_data,"depth_data_%s.csv",tmp);
    cout<<"----- file depth data: "<< c_depth_data<<endl;
    char c_uav_data[100];
    sprintf(c_uav_data,"uav_data_%s.csv",tmp);
    cout<<"----- file uav data: "<< c_uav_data<<endl;
    char c_label_data[100];
    sprintf(c_label_data,"label_data_%s.csv",tmp);
    cout<<"----- file label data: "<< c_label_data<<endl;
    char c_semantics[100];
    sprintf(c_semantics, "semantics_%s.csv",tmp);
    cout<<"----- file semantics data: "<< c_semantics<<endl;

    outFile_rgbdata.open(c_rgb_data, ios::out);
    outFIle_depthdata.open(c_depth_data, ios::out);
    outFile_labels.open(c_label_data, ios::out);
    outFile_semantics.open(c_semantics, ios::out);
    outFile_uavdata.open(c_uav_data, ios::out);

    ros::init(argc, argv, "uavdataprocessorigin");
    ros::NodeHandle nh;
    ros::Subscriber Odom_sub = nh.subscribe("/odom", 2, callBackOdom);
    ros::Subscriber Radar_sub = nh.subscribe("/radar/current_point", 2, callBackRadar);
    ros::Subscriber Cmd_sub = nh.subscribe("/mobile_base/commands/velocity", 2, callBackCmdMobileBase);
    ros::Subscriber CmdSmoother_sub = nh.subscribe("/teleop_velocity_smoother/raw_cmd_vel", 2, callBackTeleop);
    ros::Subscriber TargetPos_sub = nh.subscribe("/radar/target_point", 2, callBackTargetPos);
    ros::Subscriber TargetYaw_sub = nh.subscribe("/radar/target_yaw", 2, callBackTargetYaw);
    ros::Subscriber CurrentYaw_sub = nh.subscribe("/radar/current_yaw", 2, callBackCurrentYaw);
    ros::Subscriber DeltYaw_sub = nh.subscribe("/radar/delt_yaw", 2, callBackDeltYaw);
    ros::Subscriber rgbImage_sub = nh.subscribe("/camera/rgb/image_raw", 2, callbackRGB); //rgb images
    ros::Subscriber depthImage_sub = nh.subscribe("/camera/depth/image_raw", 1, imageCb_d); //depth images
    ros::Subscriber detection_sub = nh.subscribe("/darknet_ros/bounding_boxes", 2, objectsCallback); //semantics 
    ros::spin();
    outFile_labels.close();
    outFile_rgbdata.close();
    outFIle_depthdata.close();
    return 0;
}