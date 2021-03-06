//
// Created by jakub on 28.2.2020.
//

#include <ros/ros.h>
#include <amr_graph_generator/PathDxfParser.h>
#include <amr_msgs/Graph.h>
#include <amr_graph_generator/GraphVisualizer.h>

int main(int argc, char **argv) {

    ros::init(argc, argv, "graph_generator"); /// initializes the ros node with default name
    ros::NodeHandle n("~");

    double maxEdgeLength;
    std::string dxfFilepath;
    int graphVisualizationRate;

    n.getParam("maxEdgeLength", maxEdgeLength);
    n.getParam("dxfFilepath", dxfFilepath);
    std::map<std::string, double> graphOffset;
    n.getParam("graphOffset", graphOffset);

    ROS_INFO_STREAM("Dxf input file: " + dxfFilepath);

    ros::Publisher graphPublisher = n.advertise<amr_msgs::Graph>("graph", 1, true);

    PathDxfParser pathParser(dxfFilepath, maxEdgeLength);
    auto graph = pathParser.generateGraph(graphOffset["x"], graphOffset["y"]);

    amr_msgs::Graph graphMsg;
    graphMsg.header.stamp = ros::Time();
    graphMsg.header.frame_id = "map";

    for (const auto& node: graph) {
        amr_msgs::Node nMsg;
        nMsg.point.uuid = node.uuid;
        nMsg.point.pose.x = node.posX;
        nMsg.point.pose.y = node.posY;
        nMsg.successors = node.successors;
        nMsg.isBidirectional = node.isBidirectional;
        graphMsg.nodes.push_back(nMsg);
    }

    graphPublisher.publish(graphMsg);
    ROS_INFO("Graph has been generated and published.");

    GraphVisualizer visualizer(graph);
    visualizer.publish();

    ros::spin();

    return 0;
}
