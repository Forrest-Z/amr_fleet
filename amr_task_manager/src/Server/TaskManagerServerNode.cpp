//
// Created by jakub on 8.3.2020.
//


#include <ros/ros.h>
#include <amr_task_manager/Server/TaskManagerServer.h>

int main(int argc, char **argv) {

    ros::init(argc, argv, "task_manager_server");
    TaskManagerServer manager;
    return 0;
}

