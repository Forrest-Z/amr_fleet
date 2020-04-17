//
// Created by jakub on 15.3.2020.
//

#include <amr_semaphore/server/SemaphoreServer.h>
#include <algorithm>
#include <future>

NodesOccupancyContainer::NodesOccupancyContainer(const unsigned int occupancyLength) {
    bufferMaxLength = occupancyLength;
}

std::map<std::string, std::list<Node>> NodesOccupancyContainer::getOccupancyData() {
    return data;
}

bool NodesOccupancyContainer::lockNode(const std::string &ownerId, const Node &node, bool lockVirtually) {
    if (!lockVirtually && !isNodeReallyLocked(node).empty()) {
        return false;
    } else if (ownerId == isNodeVirtuallyLocked(node)) {
        // node is already owned by this owner, but only virtually, next lock is not needed
        return true;
    } else {
        auto targetOwnerIt = data.find(ownerId);
        if (targetOwnerIt == data.end()) {
            // ownerId has not exist already, add it
            std::pair<std::string, std::list<Node>> ownerArray(ownerId, {});
            auto insertResult = data.insert(ownerArray);
            targetOwnerIt = insertResult.first;
        }

        auto& nodesArray = targetOwnerIt->second;
        nodesArray.push_back(node);
    }
}

bool NodesOccupancyContainer::unlockNode(const std::string &ownerId, const Node &node) {
    auto targetOwnerIt = data.find(ownerId);
    if (targetOwnerIt == data.end()) {
        return false; // owner does not exist
    }

    auto& nodesArray = targetOwnerIt->second;
    for (auto lockedNodeIt = nodesArray.begin(); lockedNodeIt != nodesArray.end(); lockedNodeIt++) {
        if (lockedNodeIt->uuid == node.uuid) {
            nodesArray.erase(lockedNodeIt);
            return true;
        }
    }

    return false;   // node is not locked
}

bool NodesOccupancyContainer::isNodeAlreadyLocked(const Node& node) {
    for (const auto& ownerNodes : data) {
        for (const auto& nodesIt : ownerNodes.second) {
            if (nodesIt.uuid == node.uuid) {
                return true;
            }
        }
    }

    return false;
}

bool NodesOccupancyContainer::isNodeAlreadyLockedBy(const std::string& ownerId, const Node &node) {
    auto ownerNodesIt = data.find(ownerId);
    if (ownerNodesIt == data.end()) {
        // owner does not exist
        return false;
    }

    for (const auto& n: ownerNodesIt->second) {
        if (n.uuid == node.uuid) {
            return true;
        }
    }
    return false;
}

void NodesOccupancyContainer::unlockAllNodes(const std::string &ownerId) {
    data[ownerId] = std::list<Node>();
}

void NodesOccupancyContainer::checkMaxNodesAndRemove(const std::string& ownerId, const Node& referencedNode) {
    auto targetOwnerIt = data.find(ownerId);
    if (targetOwnerIt == data.end()) {
        // owner does not exist, nothing to check
        return;
    }

    auto& nodesArray = targetOwnerIt->second;
    if (nodesArray.size() > bufferMaxLength) {
        auto refNodeIt = std::find(nodesArray.begin(), nodesArray.end(), referencedNode);

        for (int i = 0; i < bufferMaxLength; i++) {
            if (refNodeIt == nodesArray.begin()) {
                // max number of nodes has not been achieved
                return;
            }
            refNodeIt--;
        }

        nodesArray.erase(nodesArray.begin(), ++refNodeIt);
    }
}

std::string NodesOccupancyContainer::isNodeReallyLocked(const Node &node) {
    for (const auto& ownerNodes : data) {
        auto targetNodeIt = std::find(ownerNodes.second.begin(), ownerNodes.second.end(), node);

        if (targetNodeIt == ownerNodes.second.end()) {
            // node is not in this owner array
            continue;
        }

        // check if node is in critical buffer range
        if (std::distance(ownerNodes.second.begin(), targetNodeIt) < bufferMaxLength) {
            return ownerNodes.first;
        }
    }
    return {};
}

std::string NodesOccupancyContainer::isNodeVirtuallyLocked(const Node& node) {
    for (const auto& ownerNodes : data) {
        auto targetNodeIt = std::find(ownerNodes.second.begin(), ownerNodes.second.end(), node);

        if (targetNodeIt == ownerNodes.second.end()) {
            // node is not in this owner array
            continue;
        }

        // check if node is in critical buffer range
        if (std::distance(ownerNodes.second.begin(), targetNodeIt) >= bufferMaxLength) {
            return ownerNodes.first;
        }
    }
    return {};
}


SemaphoreServer::SemaphoreServer(ros::NodeHandle& nh) {

    //todo config
    nodesOccupancy = std::make_shared<NodesOccupancyContainer>(3);

    graphSub = nh.subscribe("/graph_generator/graph", 5, &SemaphoreServer::graphCb, this);
    clientsPathsSub = nh.subscribe("/task_manager_server/client_paths", 10, &SemaphoreServer::clientPathsCb, this);
    lockNodeSrv = nh.advertiseService("lock_node", &SemaphoreServer::lockNodeCb, this);

    // prepare visualizer
    visual_tools.reset(new rvt::RvizVisualTools("map", "/amr_occupied_nodes"));
    visual_tools->loadMarkerPub(false, true);  // create publisher before waiting
    // clear messages
    visual_tools->deleteAllMarkers();
    visual_tools->enableBatchPublishing();
    visual_tools->trigger();
}

bool SemaphoreServer::lockNodeCb(amr_msgs::LockPoint::Request& req, amr_msgs::LockPoint::Response& res) {
    if (req.unlockAll) {
        nodesOccupancy->unlockAllNodes(req.clientId);
        res.success = true;
        return true;
    }

    auto node = graph.getNode(req.point.uuid);

    // unlock node
    if (!req.lock) {
        if (nodesOccupancy->unlockNode(req.clientId, node)) {
            // successfully unlocked
            res.success = true;
        } else {
            // node has not been unlocked
            res.message = "Target node has not been locked yet or client ID is wrong.";
            res.success = true;
        }
    }

    std::string realNodeOwner = nodesOccupancy->isNodeReallyLocked(node);
    if (realNodeOwner.empty()) {
        // node is not really locked
        std::string bidNodeOwner = nodesOccupancy->isNodeVirtuallyLocked(node);

        if (bidNodeOwner.empty()) {
            // node is not not virtually locked and also not really locked
            // we can lock the node
            nodesOccupancy->lockNode(req.clientId, node);
            nodesOccupancy->checkMaxNodesAndRemove(req.clientId, node);

            auto clientNextNode = getClientNextWaypoint(req.clientId, node);
            if (node.isBidirectional && clientNextNode.isValid() && clientNextNode.isBidirectional) {
                lockAllBidirectionalNeighbours(req.clientId, node);
            }
            res.message = "Node successfully locked";
            res.success = true;
        } else {
            // node is virtually locked
            if (bidNodeOwner == req.clientId) {
                // owner of virtual node is requested client, we can lock it
                nodesOccupancy->lockNode(req.clientId, node);
                nodesOccupancy->checkMaxNodesAndRemove(req.clientId, node);
            } else {
                // this virtual node is owned by other client
                // check if client path continue wit bidirectional paths
                auto clientNextNode = getClientNextWaypoint(req.clientId, node);
                if (clientNextNode.isValid() && clientNextNode.isBidirectional) {
                    res.message = "Node is already locked";
                    res.success = false;
                } else {
                    nodesOccupancy->lockNode(req.clientId, node);
                    nodesOccupancy->checkMaxNodesAndRemove(req.clientId, node);
                    res.message = "Node successfully locked";
                    res.success = true;
                }
            }
        }
    } else {
        // node is really locked
        if (realNodeOwner == req.clientId) {
            // by requested client
            res.message = "Node is already locked by you";
            res.success = true;
        } else {
            // by other owner
            res.message = "Node is already locked";
            res.success = false;
        }
    }

    // update visualization
    std::async(std::launch::async, &SemaphoreServer::visualizeNodesOccupancy, this);

    return true;
}

rvt::colors getColorHash(const std::string& ownerId) {
    return static_cast<rvt::colors>(std::hash<std::string>{}(ownerId) % 15);
}

void SemaphoreServer::visualizeNodesOccupancy() {
    visual_tools->deleteAllMarkers();

//    srand( time(NULL) );

    for (const auto& ownerNodes: nodesOccupancy->getOccupancyData()) {
        for (const auto& nodes: ownerNodes.second) {
            geometry_msgs::Point point;
            point.x = nodes.posX;
            point.y = nodes.posY;

            if (nodes.isBidirectional) {
                double randNum = -1 + (std::rand() % ( 1 - (-1) + 1 ));
                point.x += randNum/100;
            }

            visual_tools->publishSphere(point, getColorHash(ownerNodes.first), rvt::scales::XLARGE);
            geometry_msgs::Pose pose;
            pose.position = point;
            pose.position.x += 0.1;
            visual_tools->publishText(pose, ownerNodes.first, rvt::WHITE, rvt::XXLARGE, false);
        }
    }

    visual_tools->trigger();
}

void SemaphoreServer::graphCb(const amr_msgs::GraphPtr &msg) {
    graph.readNewGraph(*msg);
}

void SemaphoreServer::clientPathsCb(const amr_msgs::ClientPathConstPtr &msg) {
    clientsPaths[msg->clientId] = {};
    for (const auto& wp: msg->waypoints) {
        auto node = graph.getNode(wp.uuid);
        clientsPaths[msg->clientId].push_back(node);
    }
}

void SemaphoreServer::lockAllBidirectionalNeighbours(const std::string& clientId, const Node& node) {
    auto neighbors = graph.getNeighbors(node).first;
    std::list<Node> bidNodes(neighbors.begin(), neighbors.end());
    while (!bidNodes.empty()) {
        // todo check isNodeAlreadyLocked only for target owner, it would be better
        if (!bidNodes.front().isBidirectional || nodesOccupancy->isNodeAlreadyLocked(bidNodes.front())) {
            bidNodes.pop_front();
            continue;
        }

        // this is bidirectional node
        nodesOccupancy->lockNode(clientId, bidNodes.front(), true);
        neighbors = graph.getNeighbors(bidNodes.front()).first;
        bidNodes.pop_front();
        bidNodes.insert(bidNodes.end(), neighbors.begin(), neighbors.end());
    }
}

Node SemaphoreServer::getClientNextWaypoint(const std::string& clientId, const Node& node) {
    auto clientIt = clientsPaths.find(clientId);
    if (clientIt == clientsPaths.end()) {
        // client waypoints has not been received
        return {};
    }
    auto waypointNodes = clientIt->second;
    auto currentNodeIt = std::find(waypointNodes.begin(), waypointNodes.end(), node);

    if (currentNodeIt == clientIt->second.end()) {
        // node is not between waypoints
        return {};
    } else {
        currentNodeIt++;
        if (currentNodeIt == clientIt->second.end()) {
            // this is last node, it have not successors
            return {};
        }

        return *(currentNodeIt);  // return next node
    }
}

