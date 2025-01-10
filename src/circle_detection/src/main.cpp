#include "utils_logger.hpp"
#include <circle_detector.h>
#include <image_transport/image_transport.h>
#include <message_filters/cache.h>
#include <message_filters/subscriber.h>
#include <message_filters/sync_policies/approximate_time.h>
#include <message_filters/synchronizer.h>

void imagePub(const std::vector<sensor_msgs::ImageConstPtr> &color_msg,
              const std::vector<sensor_msgs::PointCloud2ConstPtr> &cloud_msg,
              image_transport::Publisher &processed_image_pub)
{
    circle_detector cd;
    cv::Mat processed_image;
    Eigen::Matrix4f poseMatrix;
    sensor_msgs::PointCloud2 ros_cloud;
    std::string error;

    cd.imageCallback(color_msg, cloud_msg, processed_image, poseMatrix, ros_cloud,
                     circle_detector::DetectionType::Circle, error);

    // cd.imageCallback(color_msg,cloud_msg,processed_image,poseMatrix);
    // ROS_WARN("cols:%d",processed_image.cols);
    std::cout << "Estimated Pose Matrix :\n" << poseMatrix << std::endl;

    sensor_msgs::ImagePtr output_msg = cv_bridge::CvImage(color_msg[0]->header, "bgr8", processed_image).toImageMsg();
    processed_image_pub.publish(output_msg);
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "object_detection");

    ros::NodeHandle nh;
    image_transport::ImageTransport it(nh);

    printf_program("Circle_Detection: Circle Detection algorithm for RD100");
    common_tools::printf_software_version("LibCircle_Detection");

    message_filters::Subscriber<sensor_msgs::Image> color_sub(nh, "/global_cam/rgb/rgb_raw", 1);
    message_filters::Subscriber<sensor_msgs::PointCloud2> cloud_sub(nh, "/global_cam/depth/berxel_cloudpoint", 1);

    message_filters::Cache<sensor_msgs::Image> color_cache(color_sub, 5);
    message_filters::Cache<sensor_msgs::PointCloud2> cloud_cache(cloud_sub, 5);

    color_sub.registerCallback([&color_cache](const sensor_msgs::ImageConstPtr &msg) {
        color_cache.add(msg); // 将新消息添加到缓存
    });

    cloud_sub.registerCallback([&cloud_cache](const sensor_msgs::PointCloud2ConstPtr &msg) {
        cloud_cache.add(msg); // 将新消息添加到缓存
    });
    image_transport::Publisher processed_image_pub = it.advertise("/camera/processed_image", 1);
    ros::Timer timer = nh.createTimer(ros::Duration(0.1), [&](const ros::TimerEvent &) {
        // 获取缓存中的最近 5 条消息
        std::vector<sensor_msgs::ImageConstPtr> color_msgs
            = color_cache.getInterval(ros::Time::now() - ros::Duration(1.0), ros::Time::now());
        std::vector<sensor_msgs::PointCloud2ConstPtr> cloud_msgs
            = cloud_cache.getInterval(ros::Time::now() - ros::Duration(1.0), ros::Time::now());

        // 确保有足够的消息
        if(color_msgs.size() < 5 || cloud_msgs.size() < 5)
        {
            ROS_WARN("Not enough messages in cache");
            return;
        }

        // 调用 imagePub
        imagePub(color_msgs, cloud_msgs, processed_image_pub);
    });


    ros::spin();
    return 0;
}
