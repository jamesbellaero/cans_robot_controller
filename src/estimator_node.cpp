/* TODO: Also publish the state scaled to orbit. */
#include <ros/ros.h>
#include <time.h>
#include <geometry_msgs/Point.h>
#include <vicon_bridge/Markers.h>
#include <vicon_bridge/Marker.h>
#include <Eigen/Dense> 

#include "robot_controller/State.h"
#include "robot_controller/Markers.h"
#include "xsens_bridge/Imu.h"
#include "estimator.h"

#include <string>
#include <cstring>
#include <array>

Eigen::VectorXd zMarkers(15), zImu(6), state(12);
Eigen::MatrixXd P(12,12);
Estimator estimator;

// bool editing,operating;
// ros::Rate waitRate(10000);

ros::Publisher pub;
ros::Subscriber subMarkers, subImu;

ros::Time tsMarkers, tsImu;

void markersCallback(const robot_controller::Markers msg){
  tsMarkers = ros::Time(msg.tStamp);
  for(int i=0;i<15;i++){
    zMarkers(i) = msg.markers[i];
  }
}   

void imuCallback(const xsens_bridge::Imu msg){
    zImu << msg.acc[0],msg.acc[1],msg.acc[2],msg.gyr[0],msg.gyr[1],msg.gyr[2];
    tsImu = ros::Time(msg.tStamp);
}

int main(int argc, char** argv){
  ros::init(argc,argv,"estimator");
  std::string robotName;
  ros::NodeHandle nh;
  nh.getParam("RobotName", robotName);
  tsImu = ros::Time(0);
  tsMarkers = ros::Time(0);

  subImu = nh.subscribe(robotName+std::string("/imu"),1000,imuCallback);
  subMarkers = nh.subscribe(robotName+std::string("/markers"),1000,markersCallback);
  pub = nh.advertise<robot_controller::State>(robotName+std::string("/state"),1000);
  ros::Rate loop_rate(100);
  ros::Time tsImuOld,tsMarkersOld;
  bool first = true;
  ROS_INFO("Estimator Node Initialized");

  while(ros::ok()){
    if(first){
      //Do things
      if(tsImu.toSec() == 0 || tsMarkers.toSec() == 0){
        ros::spinOnce();
        loop_rate.sleep();
        continue;
      }
      estimator.estimateStateFromMarkers(zMarkers);
      ROS_INFO("Estimator Initialized");
      first=false;
    }else{
      double dtImu = ((ros::Duration)(tsImu - tsImuOld)).toSec();
      double dtMarkers = ((ros::Duration)(tsMarkers - tsMarkersOld)).toSec();
      ROS_INFO("GOT MEASUREMENT");
      estimator.predict(zImu,dtImu);
      // operating = true;
      // while(editing){
      //   waitRate.sleep();
      // }
      ROS_INFO("Predicted");
      estimator.correct(zMarkers,dtMarkers);
      ROS_INFO("CORRECTED");
      // operating = false;
    }
    tsImuOld = ros::Time().fromNSec(tsImu.toNSec());
    tsMarkersOld = ros::Time().fromNSec(tsMarkers.toNSec());
    robot_controller::State toPub;
    state = estimator.getState();
    for(int i=0;i<3;i++){//There are better ways
      toPub.r[i] = state(i);
      toPub.q[i] = state(i+3);
      toPub.v[i] = state(i+7);
      toPub.ba[i] = state(i+10);
    }
    toPub.q[3] = state(6);
    ros::Time tsNow = ros::Time::now();
    toPub.tStamp = tsNow.toSec();
    pub.publish(toPub);
    ros::spinOnce();
    loop_rate.sleep();
	}
}
