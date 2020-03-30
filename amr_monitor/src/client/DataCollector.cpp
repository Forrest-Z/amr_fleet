//
// Created by jakub on 21. 3. 2020.
//

#include <amr_monitor/client/DataCollector.h>

DataCollector::DataCollector() {

    ros::NodeHandle nh;
    robotPoseSub = nh.subscribe("amcl_pose", 5, &DataCollector::robotPoseCb, this);
    robotCurrentGoalSub = nh.subscribe("pose_controller/current_goal", 5, &DataCollector::robotCurrentGoalCb, this);
    clientStatusPub = nh.advertise<amr_msgs::ClientInfo>("/client_info", 5, true);

    clientInfo.clientId = ros::this_node::getNamespace();

    ros::Rate r(1);
    while (ros::ok()) {
        ros::spinOnce();
        clientStatusPub.publish(clientInfo);
        r.sleep();
    }
}

void DataCollector::robotPoseCb(const geometry_msgs::PoseWithCovarianceStampedConstPtr &pose) {
    clientInfo.poseWithCovariance = *pose;
}

void DataCollector::robotCurrentGoalCb(const amr_msgs::PointConstPtr &point) {
    clientInfo.robotCurrentGoal = *point;
}






//void DataCollector::updateRobotPose() {
//    tf::StampedTransform transform;
//    try{
//        poseTfListener.waitForTransform("/map", tf::resolve(tfPrefix, "base_link"), ros::Time::now(), ros::Duration(5.0));
//        poseTfListener.lookupTransform("/map", tf::resolve(tfPrefix, "base_link"), ros::Time(0), transform);
//        geometry_msgs::Pose2D currentRobotPose;
//        currentRobotPose.x = transform.getOrigin().x();
//        currentRobotPose.y = transform.getOrigin().y();
//        currentRobotPose.theta = tf::getYaw(transform.getRotation());
//
//        controller->setCurrentPose(currentRobotPose);
//        robotPoseReceived = true;
////        ROS_INFO_STREAM("X: " << currentRobotPose.x << " Y: " << currentRobotPose.y << " Yaw: " << currentRobotPose.theta);
//    } catch (const tf::TransformException& ex) {
//        ROS_ERROR("%s", ex.what());
//        ros::Duration(1.0).sleep();
//    }
//}