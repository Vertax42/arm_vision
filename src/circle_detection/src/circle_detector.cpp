#include "circle_detector.h"
#include "utils_logger.hpp"

// caculate average Pose matrix
Eigen::Matrix4f circle_detector::averagePoseMatrices(const std::deque<Eigen::Matrix4f> &matrices)
{
    Eigen::Matrix4f sum = Eigen::Matrix4f::Zero();
    for(const auto &mat : matrices)
    {
        sum += mat;
    }
    return sum / static_cast<float>(matrices.size());
}

// use Ransac to seg
pcl::PointCloud<pcl::PointXYZRGB>::Ptr
circle_detector::segmentPlane(const pcl::PointCloud<pcl::PointXYZRGB>::Ptr &cloud,
                              pcl::ModelCoefficients::Ptr coefficients)
{
    // 创建一个新的 XYZ 点云用于分割
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::copyPointCloud(*cloud, *cloud_xyz);

    pcl::PointCloud<pcl::PointXYZ>::Ptr plane(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::SACSegmentation<pcl::PointXYZ> seg;
    pcl::PointIndices::Ptr inliers(new pcl::PointIndices);
    // pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);

    seg.setOptimizeCoefficients(true);
    // seg.setModelType(pcl::SACMODEL_PLANE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(0.01);

    seg.setInputCloud(cloud_xyz);
    seg.segment(*inliers, *coefficients);

    if(inliers->indices.empty())
    {
        ROS_INFO("Could not estimate a planar model for the given dataset.");
        return cloud; // 返回原始点云，如果无法找到平面
    }

    // 提取平面内的点云
    pcl::ExtractIndices<pcl::PointXYZ> extract;
    extract.setInputCloud(cloud_xyz);
    extract.setIndices(inliers);
    extract.setNegative(false);

    pcl::PointCloud<pcl::PointXYZ>::Ptr plane_xyz(new pcl::PointCloud<pcl::PointXYZ>);
    extract.filter(*plane_xyz);

    // 将结果转换回 XYZRGB
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr plane_rgb(new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::copyPointCloud(*plane_xyz, *plane_rgb);

    return plane_rgb;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr
circle_detector::completePlane(const pcl::PointCloud<pcl::PointXYZRGB>::Ptr &plane)
{
    pcl::PointXYZRGB minPt, maxPt;
    pcl::getMinMax3D(*plane, minPt, maxPt);

    float sum_z = 0.0;
    for(const auto &point : plane->points)
    {
        sum_z += point.z;
    }
    float mean_z = sum_z / plane->points.size();

    float width = maxPt.x - minPt.x;
    float height = maxPt.y - minPt.y;

    float centerX = (minPt.x + maxPt.x) / 2.0;
    float centerY = (minPt.y + maxPt.y) / 2.0;
    // float centerZ = (minPt.z + maxPt.z) / 2.0;

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr completed_plane(new pcl::PointCloud<pcl::PointXYZRGB>);
    // every 1cm add a point, z
    for(float x = centerX - width / 2; x <= centerX + width / 2; x += 0.01)
    {
        for(float y = centerY - height / 2; y <= centerY + height / 2; y += 0.01)
        {
            pcl::PointXYZRGB point;
            point.x = x;
            point.y = y;
            point.z = mean_z;
            point.r = 0;
            point.g = 0;
            point.b = 255;
            point.a = 255;
            completed_plane->points.push_back(point);
        }
    }
    std::cout << "CompletedPlane cloud's size: " << completed_plane->points.size() << "." << std::endl;
    return completed_plane;
}

Eigen::Matrix4f circle_detector::estimatePoseMatrixUsingPCA(const pcl::PointCloud<pcl::PointXYZRGB>::Ptr &cloud)
{
    // 创建一个新的 XYZ 点云用于 PCA
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::copyPointCloud(*cloud, *cloud_xyz);

    Eigen::Vector4f centroid;
    pcl::compute3DCentroid(*cloud_xyz, centroid);

    pcl::PCA<pcl::PointXYZ> pca;
    pca.setInputCloud(cloud_xyz);
    Eigen::Matrix3f eigenVectors = pca.getEigenVectors();
    Eigen::Matrix4f poseMatrix = Eigen::Matrix4f::Identity();
    poseMatrix.block<3, 3>(0, 0) = eigenVectors;
    poseMatrix.block<3, 1>(0, 3) = centroid.head<3>();

    return poseMatrix;
}

Eigen::Matrix4f circle_detector::adjustPoseMatrixZAxis(Eigen::Matrix4f poseMatrix)
{
    Eigen::Matrix4f adjustedPoseMatrix = poseMatrix;

    Eigen::Vector3f poseZAxis = poseMatrix.block<3, 1>(0, 2);

    // 检查 Z 轴的 Z 分量是否大于0
    if(poseZAxis.z() < 0)
    {
        adjustedPoseMatrix.block<3, 1>(0, 2) = -poseZAxis;
    }

    return adjustedPoseMatrix;
}

void circle_detector::enhanceGrayImage(cv::Mat &grayImage)
{
    double minVal, maxVal;
    cv::minMaxLoc(grayImage, &minVal, &maxVal); // 找到图像中的最小和最大像素值

    if(maxVal > 0)
    { // 防止除以零
        grayImage.convertTo(grayImage, CV_32F,
                            255.0 / maxVal);   // 转换数据类型并等比例放大
        grayImage.convertTo(grayImage, CV_8U); // 转换回原始数据类型
    }
}

cv::Point2f circle_detector::calculateCircleCenter(cv::Point2f a, cv::Point2f b, cv::Point2f c)
{
    double ax = a.x, ay = a.y;
    double bx = b.x, by = b.y;
    double cx = c.x, cy = c.y;

    double D = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    double Ux = (pow(ax, 2) + pow(ay, 2)) * (by - cy) + (pow(bx, 2) + pow(by, 2)) * (cy - ay)
                + (pow(cx, 2) + pow(cy, 2)) * (ay - by);
    double Uy = (pow(ax, 2) + pow(ay, 2)) * (cx - bx) + (pow(bx, 2) + pow(by, 2)) * (ax - cx)
                + (pow(cx, 2) + pow(cy, 2)) * (bx - ax);

    double centerX = Ux / D;
    double centerY = Uy / D;
    return cv::Point2f(centerX, centerY);
}

bool circle_detector::is_same_point(const pcl::PointXYZRGB &point1, const pcl::PointXYZRGB &point2)
{
    if(point1.x == point2.x && point1.y == point2.y && point1.z == point2.z)
    {
        return true;
    } else
    {
        return false;
    }
}

bool circle_detector::is_valid_point(const pcl::PointXYZRGB &point)
{
    float range = sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
    if(range < 0.15 || range > 10)
        return false;
    else
        return true;
}

bool circle_detector::cloud_is_vaild(const pcl::PointCloud<pcl::PointXYZRGB>::Ptr &cloud, float threhold)
{
    int count = 0;
    for(const auto &point : cloud->points)
    {
        if(point.x == 0 && point.y == 0 && point.z == 0)
        {
            count++;
            if(count >= cloud->points.size() * threhold)
            {
                ROS_INFO("Error: Input cloud have %d zero points, the cloud is not valid!", count);
                // std::cout << "Error: Input cloud have " << count << " zero points!"
                // << std::endl;
                return false;
            }
        }
    }
    ROS_INFO("The contour cloud have %d zero points! Still use it!", count);
    // std::cout << "The contour cloud have " << count << " zero points! Still use
    // it!" << std::endl; std::cout << "Still use it." << std::endl;
    return true;
}

bool circle_detector::isDarkScene(const cv::Mat &grayImage, double threshold)
{
    cv::Scalar meanBrightness = cv::mean(grayImage);
    ROS_INFO("The input grayImage's mean brightness: %f", meanBrightness[0]);
    return meanBrightness[0] < threshold;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr
circle_detector::removeZeroPoints(const pcl::PointCloud<pcl::PointXYZRGB>::Ptr &cloud)
{
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr filtered_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    // std::cout << "Input cloud width, height: " << cloud->width << ", " <<
    // cloud->height << std::endl;

    filtered_cloud->points.reserve(cloud->points.size());

    for(const auto &point : cloud->points)
    {
        if(!(point.x == 0 && point.y == 0 && point.z == 0))
        {
            filtered_cloud->push_back(point);
        }
    }

    // filtered_cloud->width = filtered_cloud->points.size();
    // filtered_cloud->height = 1;
    // filtered_cloud->is_dense = cloud->is_dense;
    ROS_INFO("removeZeroPoints! Now output cloud's width, height: %d, %d", filtered_cloud->width,
             filtered_cloud->height);
    // std::cout << "Output cloud width, height: " << filtered_cloud->width << ",
    // " << filtered_cloud->height << std::endl;
    return filtered_cloud;
}

pcl::PointXYZRGB circle_detector::projectPointOntoPlane(const pcl::PointXYZRGB &point,
                                                        const pcl::ModelCoefficients::Ptr coefficients)
{
    if(coefficients->values.size() != 4)
    {
        ROS_ERROR("Fatal Error: The coefficients size is not equal to 4!");
        return point;
        // throw std::runtime_error("Failed to estimate the plane equation.");
    }

    float a = coefficients->values[0];
    float b = coefficients->values[1];
    float c = coefficients->values[2];
    float d = coefficients->values[3];

    float x0 = point.x;
    float y0 = point.y;
    float z0 = point.z;

    float t = -(a * x0 + b * y0 + c * z0 + d) / (a * a + b * b + c * c);
    float x_proj = x0 + a * t;
    float y_proj = y0 + b * t;
    float z_proj = z0 + c * t;

    pcl::PointXYZRGB projected_point;
    projected_point.x = x_proj;
    projected_point.y = y_proj;
    projected_point.z = z_proj;
    projected_point.r = point.r;
    projected_point.g = point.g;
    projected_point.b = point.b;
    projected_point.a = point.a;

    return projected_point;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr circle_detector::recoverPointcloudsfrommultiframes(
    std::vector<pcl::PointCloud<pcl::PointXYZRGB>::Ptr> &pointcloudVector)
{
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr mergedCloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    std::cout << "The input pointcloudVector's size: " << pointcloudVector.size() << "." << std::endl;
    if(pointcloudVector[0]->points.size() != pointcloudVector[pointcloudVector.size() - 1]->points.size())
    {
        ROS_ERROR("Error! The num_points of vector elements are not equal!");
        return mergedCloud;
    }
    size_t num_points = pointcloudVector[0]->points.size();
    mergedCloud->resize(num_points);

    // std::swap(pointcloudVector_temp[0], pointcloudVector_temp[1]);
    // pcl::PointCloud<pcl::PointXYZRGB>::Ptr tempCloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    // *tempCloud = *pointcloudVector[0];
    // *pointcloudVector[0] = *pointcloudVector[1];
    // *pointcloudVector[1] = *tempCloud;

    pointcloudVector.erase(pointcloudVector.begin());
    ROS_WARN("Remove the first pointcloud, now the pointcloudVector's size: %ld", pointcloudVector.size());
    
    for(size_t i = 0; i < num_points; ++i)
    {
        // 检查第一个点云的点
        if(pointcloudVector[0]->points[i].x == 0 && pointcloudVector[0]->points[i].y == 0
           && pointcloudVector[0]->points[i].z == 0)
        {
            // 如果是无效点，查找其余点云中的非无效点
            bool found = false;
            for(size_t j = 1; j < pointcloudVector.size(); ++j)
            {
                if(!(pointcloudVector[j]->points[i].x == 0 && pointcloudVector[j]->points[i].y == 0
                     && pointcloudVector[j]->points[i].z == 0))
                {
                    mergedCloud->points[i] = pointcloudVector[j]->points[i];
                    found = true;
                    break;
                }
            }
            // 如果在其他点云中都没有找到非无效点，则保留为(0, 0, 0)
            if(!found)
            {
                mergedCloud->points[i].x = 0;
                mergedCloud->points[i].y = 0;
                mergedCloud->points[i].z = 0;
                mergedCloud->points[i].r = pointcloudVector[0]->points[i].r;
                mergedCloud->points[i].g = pointcloudVector[0]->points[i].g;
                mergedCloud->points[i].b = pointcloudVector[0]->points[i].b;
                mergedCloud->points[i].a = pointcloudVector[0]->points[i].a;
            }
        } else
        {
            // 如果不是无效点，直接赋值
            mergedCloud->points[i] = pointcloudVector[0]->points[i];
        }
    }
    ROS_INFO("Merge clouds finish! New point cloud mergedCloud's size: %ld!", mergedCloud->points.size());
    return mergedCloud;
}

cv_bridge::CvImagePtr circle_detector::chooseBestImage(const std::vector<cv_bridge::CvImagePtr> &colormsgVector)
{
    // 选择亮度值中位数对应的图像
    std::map<double, int> brightnessMap;

    // cv_bridge::CvImagePtr bestImage;

    // std::vector<cv_bridge::CvImagePtr> validImages;

    for(int index = 0; index < colormsgVector.size(); ++index)
    {
        const auto &cv_color = colormsgVector[index];
        if(cv_color && !cv_color->image.empty())
        {
            cv::Mat gray_image;
            cv::cvtColor(cv_color->image, gray_image, cv::COLOR_BGR2GRAY);
            cv::Scalar meanBrightness = cv::mean(gray_image);

            double brightnessValue = meanBrightness[0];
            brightnessMap[brightnessValue] = index;
        }
    }

    if(brightnessMap.empty())
    {
        ROS_WARN("No valid images to process.");
        return nullptr;
    }

    auto midIt = brightnessMap.begin();
    std::advance(midIt, brightnessMap.size() / 2);
    // int bestImageIndex = midIt->second;

    // 打印出选择图像在vector中的位置
    ROS_INFO("The chosen image's index in vector is: %d", midIt->second);
    // 输出最合适图像的平均亮度
    ROS_INFO("The chosecdn image's mean brightness is: %f", midIt->first);

    return colormsgVector[midIt->second];
}

bool circle_detector::imageCallback(const std::vector<sensor_msgs::ImageConstPtr> &color_msg,
                                    const std::vector<sensor_msgs::PointCloud2ConstPtr> &cloud_msg,
                                    cv::Mat &processed_image, Eigen::Matrix4f &poseMatrix,
                                    sensor_msgs::PointCloud2 &ros_cloud, DetectionType type)
{

    static bool printed_info = false;

    if(!printed_info)
    {
        printf_program("Circle_Detection: Circle Detection algorithm for RD100");
        common_tools::printf_software_version("LibCircle_Detection");
        printed_info = true;
    }

    std::string version = get_version_string(); // get lib version
    ROS_INFO("Now using libcircle_detection Version: %s", version.c_str());

    auto start_time = std::chrono::high_resolution_clock::now();

    // verify input data
    if(color_msg.size() == 0 || cloud_msg.size() == 0)
    {
        ROS_ERROR("Fatal Error: color_msg or cloud_msg empty!");
        return false;
    }

    if(color_msg.size() != cloud_msg.size())
    {
        ROS_ERROR("Fatal Error: color_msg and cloud_msg size not equal!");
        return false;
    }

    // ROS_INFO("color_msg size: %ld, cloud_msg size: %ld", color_msg.size(), cloud_msg.size());

#if 0
  // for debug
  for (size_t i = 0; i < color_msg.size(); ++i) {
    try {
      cv_bridge::CvImagePtr cv_Ptr =
          cv_bridge::toCvCopy(color_msg[i], sensor_msgs::image_encodings::BGR8);
      std::string image_filename = "image_" + std::to_string(i) + ".png";
      cv::imwrite(image_filename, cv_Ptr->image);
      ROS_INFO("Saved image in callback function!");
    } catch (cv_bridge::Exception &e) {
      std::cerr << "cv_bridge exception: " << e.what() << std::endl;
      return false;
    }
  }
#endif

    // init vectors
    std::vector<cv_bridge::CvImagePtr> cv_images;
    cv_images.reserve(color_msg.size());

    std::vector<pcl::PointCloud<pcl::PointXYZRGB>::Ptr> cloud_vector;
    cloud_vector.reserve(cloud_msg.size());
    ROS_INFO("After input check, cv_images size: %ld, cloud_vector size: %ld", color_msg.size(), cloud_msg.size());

    if(cv_images.size() != 5)
    {
        ROS_WARN("Please check the input images and clouds, expected to receive %d "
                 "messages, but got %ld messages!",
                 5, cv_images.size());
    }

    for(const auto &img_msgptr : color_msg)
    {
        ROS_INFO("color_msg timestamp: %f", img_msgptr->header.stamp.toSec());
        try
        {
            cv_bridge::CvImagePtr cv_color = cv_bridge::toCvCopy(img_msgptr, sensor_msgs::image_encodings::BGR8);
            cv_images.emplace_back(cv_color);
        } catch(cv_bridge::Exception &e)
        {
            ROS_ERROR("Fatal Error: cv_bridge exception: %s", e.what());
            return false;
        }
    }
    ROS_INFO("Transform color_msg to cv_images done, cv_images size: %ld", cv_images.size());

    // 将ROS消息vector转换为PCL点云
    for(const auto &cloud_msgptr : cloud_msg)
    {
        ROS_INFO("cloud_msg timestamp: %f", cloud_msgptr->header.stamp.toSec());
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
        pcl::fromROSMsg(*cloud_msgptr, *cloud);
        cloud_vector.emplace_back(cloud);
    }
    ROS_INFO("Transform cloud_msg to cloud_vector done, cloud_vector size: %ld", cloud_vector.size());

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    cloud = recoverPointcloudsfrommultiframes(cloud_vector);
    // 判断cloud是否为空点云
    if(cloud->points.empty())
    {
        ROS_ERROR("Fatal Error: The input clouds have different size!");
        return false;
    }
    // pcl::fromROSMsg(*cloud_msg, *cloud);
    // choose the best image from vector
    cv_bridge::CvImagePtr cv_color = chooseBestImage(cv_images);

    // cv_bridge::CvImagePtr cv_color = cv_images[2];
    if(!cv_color)
    {
        ROS_ERROR("Fatal Error: Failed to choose the best image!");
        return false;
    }
    ROS_INFO("Input image size: [%d, %d]", cv_color->image.cols, cv_color->image.rows);
    ROS_INFO("Input cloud size: %ld", cloud->points.size());
    // std::cout << "cloud.size():" << cloud->points.size() << std::endl;
    // std::cout << "image.size():" << cv_color->image.cols * cv_color->image.rows
    // << std::endl;
    if(cloud->points.size() != cv_color->image.cols * cv_color->image.rows)
    {
        ROS_WARN("点云数量和图片像素数不等");
    }

    // 创建一个用于处理轮廓的点云
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr contour_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    // 创建中心点云 (r / 4)
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr contour_cloud_center(new pcl::PointCloud<pcl::PointXYZRGB>);

    float target_circle_radius;
    cv::Point contourCenter;
    cv::Point2f target_circle_center;
    int circle_count = 0;
    bool contour_cloud_center_valid;

    std::vector<std::vector<float> > detected_results(2, std::vector<float>(2, 0));

    if(type == Circle)
    {
        // ROS_WARN("有圆形贴纸测点");
        cv::Mat gray_image;
        cv::cvtColor(cv_color->image, gray_image, cv::COLOR_BGR2GRAY);
        // 极端黑暗场景判断
        if(isDarkScene(gray_image, 15))
        {
            ROS_INFO("Image circle detection: Dark Scene Detected! Adjust input "
                     "image by clahe equalization!");
            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
            clahe->setClipLimit(4);
            clahe->setTilesGridSize(cv::Size(7, 7));
            clahe->apply(gray_image, gray_image);
            cv::Scalar meanBrightness = cv::mean(gray_image);
            ROS_INFO("After CLAHE equalization, the image's mean brightness now is: %f", meanBrightness[0]);
        }
        // Gaussian + Median Blur
        cv::GaussianBlur(gray_image, gray_image, cv::Size(9, 9), 0, 0);
        cv::medianBlur(gray_image, gray_image, 5);
        ROS_INFO("Gaussian & Median Blur Done.");
        ROS_INFO("Input gray image width, height: [%d, %d]", gray_image.cols, gray_image.rows);
        // std::cout << "gray_image.size():"<< gray_image.size() << std::endl;
        // std::cout << "Gaussian & Median Blur Done." << std::endl;
        int param1 = 100;               // Canny边缘检测的高阈值
        int param2 = 50;                // 累加器的阈值
        int param3 = 50;                // 圆最小半径
        std::vector<cv::Vec3f> circles; // 用于存储检测到的圆的参数
        cv::HoughCircles(gray_image, circles, cv::HOUGH_GRADIENT, 1, gray_image.rows / 8, param1, param2, param3,
                         150); // 霍夫变换检测圆
        ROS_INFO("Try to apply Hough Circle Detection with Canny high threshold "
                 "param1: %d, Adder counter param2: %d, Min radius param3: %d",
                 param1, param2, param3);
        // std::cout << "Hough Circle Detection Done." << std::endl;
        while(circles.empty())
        {
            ROS_INFO("No circle detected, try to adjust the parameters.");
            param1 *= 0.8;
            param2 *= 0.8;
            param3 *= 0.8;
            if(param3 <= 20)
            {
                param3 = 20; // 圆最小半径
            }
            ROS_INFO("Now using Canny high threshold param1: %d, Adder counter "
                     "param2: %d, Min radius param3: %d",
                     param1, param2, param3);
            if(param1 < 30 || param2 < 20)
            {
                ROS_ERROR("Fatal Error: Taking wrong pictures, no Hough circle "
                          "detected! Please check the input image!");
                // std::cout << "No Hough circle detected." << std::endl;
                return false;
            }
            cv::HoughCircles(gray_image, circles, cv::HOUGH_GRADIENT, 1, gray_image.rows / 8, param1, param2, param3,
                             150);
        }

        if(circles.size() > 2)
        {
            ROS_ERROR("Fatal Error: Taking wrong pictures, too many circles "
                      "detected! Please check the input image!");
            // std::cout << "Error: Too many circles detected!" << std::endl;
            return false;
        }

        ROS_INFO("Hough Detected results: %ld", circles.size());
        ROS_INFO("---------------------------------------------");
        // std::cout << "Hough Detected results: " << circles.size() << std::endl;
        // std::cout << "-----------------------------" << std::endl;
        cv::Point center_now;
        float radius_now;
        processed_image = cv_color->image.clone();
        for(size_t i = 0; i < circles.size(); i++)
        {
            cv::Vec3f c = circles[i];
            cv::Point center = cv::Point2f(c[0], c[1]);
            float radius = c[2];
            std::cout << "Center: " << center << ", Radius: " << radius << std::endl;
            int rectX = (center.x - radius) > 0 ? (center.x - radius) : 0;
            int rectY = (center.y - radius) > 0 ? (center.y - radius) : 0;
            int rectWidth = (gray_image.cols - rectX) > (2 * radius) ? (2 * radius) : (gray_image.cols - rectX);
            int rectHeight = (gray_image.rows - rectY) > (2 * radius) ? (2 * radius) : (gray_image.rows - rectY);

            cv::Rect roi_rect = cv::Rect(rectX, rectY, rectWidth, rectHeight);
            cv::Mat roi = gray_image(roi_rect);
            cv::Mat mask = cv::Mat::zeros(roi.size(), CV_8UC1);
            cv::circle(mask, cv::Point(radius, radius), radius, cv::Scalar(255), -1);
            cv::Scalar mean_roi = cv::mean(roi, mask);

            cv::Rect roi_rect_half
                = cv::Rect(rectX + (radius / 2), rectY + (radius / 2), rectWidth / 2, rectHeight / 2);
            cv::Mat roi_half = gray_image(roi_rect_half);
            cv::Mat mask_half = cv::Mat::zeros(roi_half.size(), CV_8UC1);
            cv::circle(mask_half, cv::Point(radius / 2, radius / 2), radius / 2, cv::Scalar(255), -1);
            cv::Scalar mean_roi_half = cv::mean(roi_half, mask_half);

            std::string label;
            cv::Scalar circlecolor;

            std::cout << "mean_roi, mean_roi_half: " << mean_roi[0] << ", " << mean_roi_half[0] << std::endl;
            int centergray = gray_image.at<uchar>(center.y, center.x);
            std::cout << "center gray value: " << centergray << std::endl;
            if(mean_roi[0] > 192 && mean_roi_half[0] > 192 && abs(mean_roi[0] - mean_roi_half[0]) < 10)
            {
                label = "Circle";
                circlecolor = cv::Scalar(0, 255, 0);
                ROS_INFO("Circle Detected!");
                // std:cout << "Circle Detected" << std::endl;
                circle_count++;
                center_now = center;
                radius_now = radius;
                detected_results[0][i] = abs(mean_roi[0] - mean_roi_half[0]);
                detected_results[1][i] = centergray;
            } else if((abs(mean_roi[0] - mean_roi_half[0]) < 10) && (centergray > 96))
            {
                label = "Circle";
                circlecolor = cv::Scalar(0, 255, 0);
                ROS_INFO("Circle Detected!");
                // std::cout << "Circle Detected" << std::endl;
                circle_count++;
                center_now = center;
                radius_now = radius;
                detected_results[0][i] = abs(mean_roi[0] - mean_roi_half[0]);
                detected_results[1][i] = centergray;
            } else if(abs(mean_roi[0] - mean_roi_half[0]) > 10)
            {
                label = "Ring";
                circlecolor = cv::Scalar(255, 0, 0);
                std::cout << "Ring Detected" << std::endl;
                target_circle_radius = radius;
                target_circle_center = center;
                detected_results[0][i] = abs(mean_roi[0] - mean_roi_half[0]);
                detected_results[1][i] = centergray;
            } else
            {
                label = "Circle";
                circlecolor = cv::Scalar(0, 255, 0);
                ROS_INFO("Circle Detected!");
                circle_count++;
                center_now = center;
                radius_now = radius;
                detected_results[0][i] = abs(mean_roi[0] - mean_roi_half[0]);
                detected_results[1][i] = centergray;
            }

            cv::circle(processed_image, center, radius, circlecolor, 1, cv::LINE_AA);
            cv::circle(processed_image, center, 2, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
            cv::putText(processed_image, label, cv::Point(center.x + 10, center.y + 10), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        circlecolor, 1, cv::LINE_AA);
            std::cout << "-----------------------------" << std::endl;
        }

        if(circles.size() == 2 && circle_count == 0)
        {
            int chosen_index = 0;
            if(detected_results[0][0] < detected_results[0][1])
            {
                chosen_index = 1;
            }
            ROS_INFO("Detected two rings! Choose the ring %d as target!", chosen_index);

            target_circle_center = cv::Point2f(circles[chosen_index][0], circles[chosen_index][1]);
            target_circle_radius = circles[chosen_index][2];
            ROS_INFO("Target ring center: (%f, %f), radius: %f", target_circle_center.x, target_circle_center.y,
                     target_circle_radius);
        }

        if((circles.size() == 2) && (circle_count == 2))
        {
            int chosen_index = 0;
            if(detected_results[1][0] > detected_results[1][1])
            {
                chosen_index = 1;
            }
            ROS_INFO("Detected two circles! Choose the circle %d as target!", chosen_index);

            target_circle_center = cv::Point2f(circles[chosen_index][0], circles[chosen_index][1]);
            target_circle_radius = circles[chosen_index][2];
            ROS_INFO("Target ring center: (%f, %f), radius: %f", target_circle_center.x, target_circle_center.y,
                     target_circle_radius);
        }

        if(circles.size() == 1 && circle_count == 1)
        {
            ROS_INFO("Only one circle detected, using circle center!");
            // std::cout << "Only one circle detected, using circle center!" <<
            // std::endl;
            target_circle_radius = radius_now;
            target_circle_center = center_now;
        }

        contourCenter.x = static_cast<int>(target_circle_center.x);
        contourCenter.y = static_cast<int>(target_circle_center.y);

        // std::vector<cv::Point> target_points;
        // std::vector<cv::Point> target_points_center;
        double sum_z = 0.0;
        int count_n = 0;

        for(int i = 0; i < cv_color->image.cols; i++)
        {
            for(int j = 0; j < cv_color->image.rows; j++)
            {
                if(cv::norm(target_circle_center - cv::Point2f(i, j)) < target_circle_radius)
                {
                    int index1 = j * cv_color->image.cols + i;
                    if(index1 < cloud->points.size())
                    {
                        pcl::PointXYZRGB point = cloud->points[index1];
                        if(!std::isnan(point.z))
                        {
                            contour_cloud->points.push_back(point);
                            sum_z += point.z;
                            count_n++;
                        }
                    }
                }

                if(cv::norm(target_circle_center - cv::Point2f(i, j)) < (target_circle_radius / 4))
                {
                    int index2 = j * cv_color->image.cols + i;
                    if(index2 < cloud->points.size())
                    {
                        pcl::PointXYZRGB point = cloud->points[index2];
                        if(!std::isnan(point.z))
                        {
                            contour_cloud_center->points.push_back(point);
                        }
                    }
                }
            }
        }

        ROS_INFO("Contour_cloud mean_z: %f", sum_z / count_n);

        if((sum_z / count_n) < 0.15 || (sum_z / count_n) > 1.2)
        {
            ROS_ERROR("Fatal Error: The mean_z of contour_cloud is out of range!");
            return false;
        } else if((sum_z / count_n) >= 0.45)
        {
            ROS_INFO("Background points need to be removed!");
            pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_passthrough(new pcl::PointCloud<pcl::PointXYZRGB>);
            pcl::PassThrough<pcl::PointXYZRGB> pass;
            pass.setInputCloud(cloud);
            pass.setFilterFieldName("z");
            pass.setFilterLimits(0.15, 0.45);
            pass.setNegative(false);
            pass.filter(*cloud_passthrough);
            ROS_INFO("After passthrough filter, cloud_passthrough size: %ld", cloud_passthrough->points.size());
            cloud = cloud_passthrough;
        } else
        {
            ROS_INFO("Background points are valid, contour_cloud size: %ld", contour_cloud->points.size());
        }
        //     if (cv::norm(target_circle_center - cv::Point2f(i, j)) <
        //         (target_circle_radius / 4)) {
        //       target_points_center.push_back(cv::Point(i, j));
        //     }
        //   }
        // }

        // int width = cv_color->image.cols;
        // for (const auto &pt : target_points) {
        //   int index = pt.y * width + pt.x;
        //   if (index < cloud->points.size()) {
        //     contour_cloud->points.push_back(cloud->points[index]);
        //   }
        // }

        // for (const auto &pt : target_points_center) {
        //   int index = pt.y * width + pt.x;
        //   if (index < cloud->points.size()) {
        //     contour_cloud_center->points.push_back(cloud->points[index]);
        //   }
        // }

        // std::cout << "Contour_cloud size: " << contour_cloud->points.size() <<
        // "." << std::endl;

        if(!cloud_is_vaild(contour_cloud_center, 0.8))
        {
            ROS_ERROR("Fatal Error: Over 80%% points in contour_cloud_center are (0, "
                      "0, 0), cannot estimate contour_cloud_center! Please check the "
                      "input point clouds!!!!");
            return false;
        }

        if(!cloud_is_vaild(contour_cloud_center, 0.5))
        {
            ROS_INFO("Extracted contour_cloud_center isn't valid! Over 50%% points "
                     "in contour_cloud_center are (0, 0, 0), try to estimate "
                     "detection result from bad points!");
            // std::cout << "contour_cloud_center isn't valid! " << std::endl;
            contour_cloud_center_valid = false;
        } else
        {
            ROS_INFO("Extracted contour_cloud_center is valid! Less than 50%% points "
                     "in contour_cloud_center are (0, 0, 0), use "
                     "contour_cloud_center to construct kdtree for KnnSearch!");
            // std::cout << "contour_cloud_center is valid! " << std::endl;
            contour_cloud_center_valid = true;
        }

        if(!cloud_is_vaild(contour_cloud, 0.01))
        {
            ROS_INFO("Detected zero points! Remove all zero points! Try to estimate "
                     "contour_cloud!");
            // std::cout << "Too many zero points! Remove all zero points! Try to
            // estimate contour_cloud!" << std::endl;
            std::cout << "Before contour_cloud's size are: " << contour_cloud->points.size() << std::endl;
            contour_cloud = removeZeroPoints(contour_cloud);
            std::cout << "Now contour_cloud's size are: " << contour_cloud->points.size() << std::endl;
            // return false;
        }

        if(contour_cloud->points.size() < 30 || contour_cloud_center->points.size() < 10)
        {
            ROS_ERROR("Fatal Error: Too few points in contour_cloud or "
                      "contour_cloud_center! Please check the input point clouds!");
            return false;
        }
    }

    else if(type == NonCircle)
    {
        // ROS_WARN("无圆形贴纸测点");
        processed_image = cv_color->image.clone();
        contourCenter.x = cv_color->image.cols / 2;
        contourCenter.y = cv_color->image.rows / 2;
        int width = cv_color->image.cols;
        for(auto m = contourCenter.x - cv_color->image.cols / 8; m <= contourCenter.x + cv_color->image.cols / 8; m++)
        {
            for(auto n = contourCenter.y - cv_color->image.rows / 8; n <= contourCenter.y + cv_color->image.rows / 8;
                n++)
            {
                if(m == contourCenter.x - cv_color->image.cols / 8 || m == contourCenter.x + cv_color->image.cols / 8)
                {
                    cv::circle(processed_image, cv::Point(m, n), 5, cv::Scalar(0, 255, 0), -1);
                } else if(n == contourCenter.y - cv_color->image.rows / 8
                          || n == contourCenter.y + cv_color->image.rows / 8)
                {
                    cv::circle(processed_image, cv::Point(m, n), 5, cv::Scalar(0, 255, 0), -1);
                }

                if((m >= contourCenter.x - cv_color->image.cols / 8 && m <= contourCenter.x + cv_color->image.cols / 8)
                   && (n >= contourCenter.y - cv_color->image.rows / 8
                       && n <= contourCenter.y + cv_color->image.rows / 8))
                {
                    int width = cv_color->image.cols;
                    int index = n * width + m;
                    if(index < cloud->points.size())
                    {
                        contour_cloud->points.push_back(cloud->points[index]);
                    }
                }
            }
        }
    }

    // std::cout << "contour_cloud.size():"<<contour_cloud->size() << std::endl;
    pcl::toROSMsg(*contour_cloud, ros_cloud);
    ros_cloud.header.frame_id = "circle_cloud"; // 设置合适的帧ID
    ros_cloud.header.stamp = ros::Time::now();  // 设置时间戳

    // pcl::PointCloud<pcl::PointXYZRGB>::Ptr plane_cloud =
    // segmentPlane(contour_cloud); std::cout << "plane_cloud.size():"<<
    // plane_cloud->size() << std::endl;

    if(!contour_cloud->points.empty())
    {
        int width = cv_color->image.cols;
        int index = contourCenter.y * width + contourCenter.x;

        pcl::PointXYZRGB center_rgb = cloud->points[index];
        ROS_INFO("Image contourCenter: x, y = (%d, %d)", contourCenter.x, contourCenter.y);
        ROS_INFO("Center Point: x, y, z = (%f, %f, %f)", center_rgb.x, center_rgb.y, center_rgb.z);
        // std::cout << "center: x, y, z = (" << cloud->points[index].x << ", " <<
        // cloud->points[index].y << ", " << cloud->points[index].z << ")" <<
        // std::endl;

        // std::cout << "contourCenter.x" << contourCenter.x << "contourCenter.y" <<
        // contourCenter.y << std::endl;

        pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtree;
        kdtree.setInputCloud(contour_cloud);
        int K = 1;
        std::vector<int> pointIdxNKNSearch(K);
        std::vector<float> pointNKNSquareDistance(K);
        if(kdtree.nearestKSearch(center_rgb, K, pointIdxNKNSearch, pointNKNSquareDistance) > 0)
        {
            int nearest_point_idx = pointIdxNKNSearch[0];
            pcl::PointXYZRGB nearest_point = contour_cloud->points[nearest_point_idx];
            if(nearest_point.x == 0 && nearest_point.y == 0 && nearest_point.z == 0)
            {
                ROS_INFO("Center point missing! Start to complete missing points using "
                         "segmentPlane and completePlane!!!");
                // std::cout << "Center point missing! Start to complete missing
                // points!!!" << std::endl;
                pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);
                pcl::PointCloud<pcl::PointXYZRGB>::Ptr plane_cloud = segmentPlane(contour_cloud, coefficients);
                pcl::PointCloud<pcl::PointXYZRGB>::Ptr completed_plane_cloud = completePlane(plane_cloud);

                Eigen::Vector4f plane_cloud_center;
                pcl::compute3DCentroid(*completed_plane_cloud, plane_cloud_center);
                pcl::PointXYZRGB projectPointIn;
                pcl::PointXYZRGB projectPointOut;

                projectPointIn.x = plane_cloud_center.x();
                projectPointIn.y = plane_cloud_center.y();
                projectPointIn.z = plane_cloud_center.z();
                projectPointOut = projectPointOntoPlane(projectPointIn, coefficients);
                if(is_same_point(projectPointIn, projectPointOut))
                {
                    ROS_INFO("ProjectPointOntoPlane failed! Still use the "
                             "compute3DCentroid's output as center point!");
                }

                nearest_point.x = projectPointOut.x;
                nearest_point.y = projectPointOut.y;
                nearest_point.z = projectPointOut.z;

                ROS_INFO("Completed plane cloud center: x, y, z = (%f, %f, %f)", projectPointIn.x, projectPointIn.y,
                         projectPointIn.z);

                ROS_INFO("ProjectPointOut corrdinates: x, y, z = (%f, %f, %f)", projectPointOut.x, projectPointOut.y,
                         projectPointOut.z);

                // std::cout << "Completed plane cloud center: (" << projectPointIn.x <<
                // ", " << projectPointIn.y << ", " << projectPointIn.z << ")" <<
                // std::endl; std::cout << "projectPointOut corrdinates: (" <<
                // projectPointOut.x << ", " << projectPointOut.y << ", " <<
                // projectPointOut.z << ")" << std::endl;

                if(contour_cloud_center_valid)
                {
                    pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtree1;
                    kdtree1.setInputCloud(contour_cloud_center);
                    int K = 1;
                    std::vector<int> pointIdxNKNSearch1(K);
                    std::vector<float> pointNKNSquaredDistance1(K);
                    if(kdtree1.nearestKSearch(nearest_point, K, pointIdxNKNSearch1, pointNKNSquaredDistance1) > 0)
                    {
                        int nearest_point_idx1 = pointIdxNKNSearch1[0];
                        nearest_point = contour_cloud->points[nearest_point_idx1];
                        ROS_INFO("Center Point Detection Type: 1");
                        ROS_INFO("contour_cloud_center valid! Deploy KnnSearch in "
                                 "contour_cloud_center! KnnSearch center point: (x, y, z) "
                                 "= (%f, %f, %f)",
                                 nearest_point.x, nearest_point.y, nearest_point.z);
                    }
                } else
                {
                    ROS_INFO("Center Point Detection Type: 2");
                    ROS_INFO("Use plane cloud's projectPointOut as Nearest Point! "
                             "projectPointOut center point: (x, y, z) = (%f, %f, %f)",
                             projectPointOut.x, projectPointOut.y, projectPointOut.z);
                }
            } else
            {
                ROS_INFO("Center Point Detection Type: 0");
                ROS_INFO("Center point exist! Directly use Nearest Point! Existing "
                         "center point: (x, y, z) = (%f, %f, %f)",
                         nearest_point.x, nearest_point.y, nearest_point.z);
            }

            // ROS_INFO("Nearest Point: x, y, z = (%f, %f, %f)", nearest_point.x,
            // nearest_point.y, nearest_point.z);

            pcl::NormalEstimation<pcl::PointXYZRGB, pcl::Normal> ne;
            ne.setInputCloud(contour_cloud);
            pcl::search::KdTree<pcl::PointXYZRGB>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZRGB>);
            ne.setSearchMethod(tree);
            pcl::PointCloud<pcl::Normal>::Ptr cloud_normals(new pcl::PointCloud<pcl::Normal>);
            ne.setRadiusSearch(0.03);
            ne.compute(*cloud_normals);
            pcl::Normal normal = cloud_normals->points[nearest_point_idx];
            ROS_INFO("Compute Normal: (%f, %f, %f)", normal.normal_x, normal.normal_y, normal.normal_z);
            // std::cout << "Normal: (" << normal.normal_x << ", " << normal.normal_y
            // << ", " << normal.normal_z << ")" << std::endl;
            if(normal.normal_z < 0)
            {
                ROS_INFO("Adjust normal vector to reverse Z Axis!");
                // std::cout << "adjust normal!" << std::endl;
                normal.normal_x = -normal.normal_x;
                normal.normal_y = -normal.normal_y;
                normal.normal_z = -normal.normal_z;
                ROS_INFO("Reverse finished! Normal now is: (%f, %f, %f)", normal.normal_x, normal.normal_y,
                         normal.normal_z);
            }

            poseMatrix = Eigen::Matrix4f::Identity();
            poseMatrix.block<3, 3>(0, 0)
                = Eigen::Quaternionf()
                      .setFromTwoVectors(Eigen::Vector3f::UnitZ(),
                                         Eigen::Vector3f(normal.normal_x, normal.normal_y, normal.normal_z))
                      .toRotationMatrix();
            poseMatrix.block<3, 1>(0, 3) = Eigen::Vector3f(nearest_point.x, nearest_point.y, nearest_point.z);

            std::cout << "Estimated Pose Matrix :\n" << poseMatrix << std::endl;
            auto end_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = end_time - start_time;
            ROS_INFO("Circle Detection algorithm completed!");
            ROS_INFO("libCircledetection execution time : %f seconds!", duration.count());
            return true;
        }
        ROS_ERROR("Fatal Error: Nearest K search failed! ");
        // std::cout << "Nearest K search failed! " << std::endl;
        return false;
    } else
    {
        ROS_ERROR("Fatal Error: No contour points detected! ");
        // std::cout << "目标轮廓点云信息缺失" << std::endl;
        return false;
    }
}