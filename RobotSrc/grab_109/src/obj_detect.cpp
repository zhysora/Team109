#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>

#include <visualization_msgs/Marker.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_ros/point_cloud.h>
#include <pcl_ros/transforms.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <iostream>

//#define min(x, y) (x)>(y)?(y):(x)
//#define max(x, y) (x)>(y)?(x):(y)

using namespace cv;
using namespace std;

typedef struct stBoxMarker
{
    float xMax;
    float xMin;
    float yMax;
    float yMin;
    float zMax;
    float zMin;
}stBoxMarker;

static stBoxMarker boxMarker;

static string rgb_topic;
static string pc_topic;

static const std::string WIN_TITLE = "LABEL";
static Mat frame_gray; // 保存传感器灰度图
static Mat templ; // 保存模板图像
static Mat objects; // 存储匹配的区域
static Rect matchObj; 
//static vector<Rect>::const_iterator obj_iter; // 迭代所有的标签
static int match_method = TM_CCOEFF_NORMED; // 匹配方法
static float thresholdVal = 0.95; //设定匹配阈值

static ros::Publisher image_pub; // 发布RGB
static ros::Publisher marker_pub; // 发布标记主题
static ros::Publisher pc_pub;  // 发布点云
//static visualization_msgs::Marker pos_obj; //标记物体在点云的位置
static visualization_msgs::Marker line_box; // 划线
static visualization_msgs::Marker line_follow;
static visualization_msgs::Marker text_marker;


static tf::TransformListener *tf_listener; // 坐标系变换

//static visualization_msgs::Marker pos_obj;

void DrawBox(float inMinX, float inMaxX, float inMinY, float inMaxY, float inMinZ, float inMaxZ, float inR, float inG, float inB)
{
    line_box.header.frame_id = "base_footprint";
    line_box.ns = "line_box";
    line_box.action = visualization_msgs::Marker::ADD;
    line_box.id = 0;
    line_box.type = visualization_msgs::Marker::LINE_LIST;
    line_box.scale.x = 0.005;
    line_box.color.r = inR;
    line_box.color.g = inG;
    line_box.color.b = inB;
    line_box.color.a = 1.0;

    geometry_msgs::Point p;
    p.z = inMinZ;
    p.x = inMinX; p.y = inMinY; line_box.points.push_back(p);
    p.x = inMinX; p.y = inMaxY; line_box.points.push_back(p);

    p.x = inMinX; p.y = inMaxY; line_box.points.push_back(p);
    p.x = inMaxX; p.y = inMaxY; line_box.points.push_back(p);

    p.x = inMaxX; p.y = inMaxY; line_box.points.push_back(p);
    p.x = inMaxX; p.y = inMinY; line_box.points.push_back(p);

    p.x = inMaxX; p.y = inMinY; line_box.points.push_back(p);
    p.x = inMinX; p.y = inMinY; line_box.points.push_back(p);

    p.z = inMaxZ;
    p.x = inMinX; p.y = inMinY; line_box.points.push_back(p);
    p.x = inMinX; p.y = inMaxY; line_box.points.push_back(p);

    p.x = inMinX; p.y = inMaxY; line_box.points.push_back(p);
    p.x = inMaxX; p.y = inMaxY; line_box.points.push_back(p);

    p.x = inMaxX; p.y = inMaxY; line_box.points.push_back(p);
    p.x = inMaxX; p.y = inMinY; line_box.points.push_back(p);

    p.x = inMaxX; p.y = inMinY; line_box.points.push_back(p);
    p.x = inMinX; p.y = inMinY; line_box.points.push_back(p);

    p.x = inMinX; p.y = inMinY; p.z = inMinZ; line_box.points.push_back(p);
    p.x = inMinX; p.y = inMinY; p.z = inMaxZ; line_box.points.push_back(p);

    p.x = inMinX; p.y = inMaxY; p.z = inMinZ; line_box.points.push_back(p);
    p.x = inMinX; p.y = inMaxY; p.z = inMaxZ; line_box.points.push_back(p);

    p.x = inMaxX; p.y = inMaxY; p.z = inMinZ; line_box.points.push_back(p);
    p.x = inMaxX; p.y = inMaxY; p.z = inMaxZ; line_box.points.push_back(p);

    p.x = inMaxX; p.y = inMinY; p.z = inMinZ; line_box.points.push_back(p);
    p.x = inMaxX; p.y = inMinY; p.z = inMaxZ; line_box.points.push_back(p);
    marker_pub.publish(line_box);
}

static int nTextNum = 2;
void 
DrawText(std::string inText, float inScale, float inX, float inY, float inZ, float inR, float inG, float inB)
{
    text_marker.header.frame_id = "base_footprint";
    text_marker.ns = "line_obj";
    text_marker.action = visualization_msgs::Marker::ADD;
    text_marker.id = nTextNum;
    nTextNum ++;
    text_marker.type = visualization_msgs::Marker::TEXT_VIEW_FACING;
    text_marker.scale.z = inScale;
    text_marker.color.r = inR;
    text_marker.color.g = inG;
    text_marker.color.b = inB;
    text_marker.color.a = 1.0;

    text_marker.pose.position.x = inX;
    text_marker.pose.position.y = inY;
    text_marker.pose.position.z = inZ;
    
    text_marker.pose.orientation=tf::createQuaternionMsgFromYaw(1.0);

    text_marker.text = inText;

    marker_pub.publish(text_marker);
}

void RemoveBoxes()
{
    line_box.action = 3;
    line_box.points.clear();
    marker_pub.publish(line_box);
    line_follow.action = 3;
    line_follow.points.clear();
    marker_pub.publish(line_follow);
    text_marker.action = 3;
    marker_pub.publish(text_marker);
}

static int nFrameNum=0;

void 
callbackRGB(const sensor_msgs::ImageConstPtr& msg)
{
	//将传感器图片转为Mat
	cv_bridge::CvImagePtr cv_ptr;
	try{
		cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
	}catch(cv_bridge::Exception& e){
		ROS_ERROR("cv_bridge exception: %s", e.what());
		return;
	}
	//cv_ptr->image.convertTo(frame_gray);
	//cvtColor(cv_ptr->image, frame_gray, CV_BGR2GRAY);
	
	//保存图像
	std::ostringstream stringStream;
	stringStream << "/home/robot/catkin_ws/src/team_109/grab_109/src/pic/pic_" << nFrameNum++;
	std::string frame_id = stringStream.str();
	imwrite(frame_id+".jpg", cv_ptr->image);
	ROS_INFO("(w, h)=(%d, %d)", frame_gray.cols, frame_gray.rows);

	matchTemplate(cv_ptr->image, templ, objects, match_method);
	normalize(objects, objects, 0, 1, NORM_MINMAX, -1, Mat() );
	double minVal, maxVal, val;
	Point minLoc, maxLoc, matchLoc;
	
	minMaxLoc(objects, &minVal, &maxVal, &minLoc, &maxLoc, Mat());
	if( match_method  == TM_SQDIFF || match_method == TM_SQDIFF_NORMED ){
		matchLoc = minLoc; 
		val = minVal;
	}
	else{ 
		matchLoc = maxLoc; 
		val = maxVal;
	}
	
	matchObj.x = matchLoc.x; matchObj.y = matchLoc.y;
	matchObj.width = templ.cols; matchObj.height = templ.rows;
	
	ROS_INFO("[callbackRGB] (x,y,width,height)=(%d,%d,%d,%d) maxVal=%.2f", matchObj.x,matchObj.y,matchObj.width,matchObj.height,val);

	//if(fabs(val - thresholdVal)<0.001){ // 检测到标签
		rectangle(cv_ptr->image,
		matchLoc,
		Point( matchLoc.x + templ.cols , matchLoc.y + templ.rows ), 
		CV_RGB(255, 0 , 255),
		10);
	//}
	
	image_pub.publish(cv_ptr->toImageMsg());
}

void 
callbackPointCloud(const sensor_msgs::PointCloud2 &input)
{
	//将传感器类型转为点云类型
	sensor_msgs::PointCloud2 pc_footprint;
	bool res = tf_listener->waitForTransform("/base_footprint", input.header.frame_id, input.header.stamp, ros::Duration(5.0)); 
	if(res == false)
		return ;
	pcl_ros::transformPointCloud("/base_footprint", input, pc_footprint, *tf_listener);
	//source cloud
	pcl::PointCloud<pcl::PointXYZRGB> cloud_src;
	pcl::fromROSMsg(pc_footprint , cloud_src);
	//ROS_WARN("cloud_src size = %d	 width = %d",cloud_src.size(),input.width); 
	
	ROS_INFO("(input.width, input.height)=(%d,%d)",input.width, input.height);
	ROS_INFO("[callbackPointCloud] (x,y,width,height)=(%d,%d,%d,%d)", matchObj.x,matchObj.y,matchObj.width,matchObj.height);

	Point tl = matchObj.tl(), br = matchObj.br();

ROS_INFO("%d, %d", tl.x, tl.y);

	int index_tl = tl.y * input.width + tl.x;
	int index_br = br.y * input.width + br.x;
	if(index_tl >= cloud_src.size() || index_br>=cloud_src.size()){
		ROS_WARN("no label find");
		return ;
	}
	pcl::PointXYZRGB ptl = cloud_src.points[index_tl];
	pcl::PointXYZRGB pbr = cloud_src.points[index_br];

ROS_INFO("(%f, %f, %f)", ptl.x, ptl.y, ptl.z);

ROS_INFO("%f", min(ptl.x, pbr.x));
	
	//RemoveBoxes();
	boxMarker.xMin = (float)min(ptl.x, pbr.x);
	boxMarker.xMax = max(ptl.x, pbr.x);
	boxMarker.yMin = min(ptl.y, pbr.y);
	boxMarker.yMax = max(ptl.y, pbr.y);
	boxMarker.zMin = min(ptl.z, pbr.z);
	boxMarker.zMax = max(ptl.z, pbr.z);

/*
	bool bFirstPoint = true;
	for(int y=0; y<matchObj.height; y++){
		for (int x=0; x<matchObj.width; x++){
			int rgb_obj_x = matchObj.x + x;
			int rgb_obj_y = matchObj.y + y;
			int index_pc = rgb_obj_y * input.width + rgb_obj_x;
			//ROS_INFO("(index, size)=(%d,%d)", index_pc, cloud_src.size());

			if(index_pc>=cloud_src.size()){
				ROS_WARN("no label find");
				return;
			}
			pcl::PointXYZRGB p = cloud_src.points[index_pc];
			if(bFirstPoint==true){
				boxMarker.xMax = boxMarker.xMin = p.x;
                		boxMarker.yMax = boxMarker.yMin = p.y;
                		boxMarker.zMax = boxMarker.zMin = p.z;
                		bFirstPoint = false;
			}
			if(p.x < boxMarker.xMin) { boxMarker.xMin = p.x;}
			if(p.x > boxMarker.xMax) { boxMarker.xMax = p.x;}
			if(p.y < boxMarker.yMin) { boxMarker.yMin = p.y;}
			if(p.y > boxMarker.yMax) { boxMarker.yMax = p.y;}
			if(p.z < boxMarker.zMin) { boxMarker.zMin = p.z;}
			if(p.z > boxMarker.zMax) { boxMarker.zMax = p.z;}
		}
	}*/


	ROS_INFO("[callbackPointCloud] boxMarker=(%.2f,%.2f,%.2f,%.2f, %.2f, %.2f)", boxMarker.xMin,boxMarker.xMax,boxMarker.yMin,boxMarker.yMax,boxMarker.zMin,boxMarker.zMax);

	if(boxMarker.xMin < 1.5 && boxMarker.yMin > -0.5 && boxMarker.yMax < 0.5)
	{
		RemoveBoxes();
		DrawBox(boxMarker.xMin, boxMarker.xMax, boxMarker.yMin, boxMarker.yMax, boxMarker.zMin, boxMarker.zMax, 0, 1, 0);

		DrawText("object x",0.08, boxMarker.xMax,(boxMarker.yMin+boxMarker.yMax)/2,boxMarker.zMax + 0.04, 1,0,1);
		
		ROS_WARN("[obj] xMin= %.2f yMin = %.2f yMax = %.2f",boxMarker.xMin, boxMarker.yMin, boxMarker.yMax);
	} 
	
}

int main(int argc, char **argv)
{
	//cv::nameWindow(WIN_TITLE);
	
	//templ = imread("label.jpg", 0);
	//templ = imread("label.jpg");

	ros::init(argc, argv, "grab_server");
	ros::NodeHandle nh_param("~");
	nh_param.param<std::string>("rgb_topic", rgb_topic, "/kinect2/qhd/image_color");
	nh_param.param<std::string>("topic", pc_topic, "/kinect2/qhd/points");
	//rgb_topic = "/kinect2/hd/image_color";
	//pc_topic = "/kinect2/hd/points";
	
	
	ROS_INFO("object_detect");
	
	templ = imread("/home/robot/catkin_ws/src/team_109/grab_109/src/label_qhd_color.jpg");
	ROS_INFO("(w, h)=(%d, %d)", templ.cols, templ.rows);

	tf_listener = new tf::TransformListener();
	ros::NodeHandle nh;
	ros::Subscriber rgb_sub = nh.subscribe(rgb_topic, 1, callbackRGB);
	ros::Subscriber pc_sub = nh.subscribe(pc_topic, 1, callbackPointCloud);
	
	image_pub = nh.advertise<sensor_msgs::Image>("/object/image", 2);
	marker_pub = nh.advertise<visualization_msgs::Marker>("obj_marker", 2);
	pc_pub = nh.advertise<sensor_msgs::PointCloud2>("obj_pointcloud", 1);
	
	ros::Rate loop_rate(30);
	while(ros::ok()){
		ros::spinOnce();
		loop_rate.sleep();
	}
	
	//cv::destroyWindow(WIN_TITLE);
	delete tf_listener;
	
	return 0;
	


}

