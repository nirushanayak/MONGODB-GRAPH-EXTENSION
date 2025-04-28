#include "path_finding.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>

using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

namespace mongo {
namespace graph_extension {

// Custom hasher for bsoncxx::oid
struct OidHasher {
    std::size_t operator()(const bsoncxx::oid& oid) const {
        return std::hash<std::string>{}(oid.to_string());
    }
};

// Custom equality comparator for bsoncxx::oid
struct OidEqual {
    bool operator()(const bsoncxx::oid& lhs, const bsoncxx::oid& rhs) const {
        return lhs == rhs;
    }
};

bsoncxx::document::value Path::toBSON() const {
        using namespace bsoncxx::builder::stream;
        
        // Create an array of nodes
        array nodeArray;
        for (const auto& node : nodes) {
            nodeArray << bsoncxx::types::b_document{node};
        }
        
        // Create an array of edge weights if available
        array weightArray;
        for (const auto& weight : edgeWeights) {
            weightArray << weight;
        }
        
        // Build the document
        document doc;
        doc << "pathFound" << !nodes.empty()
            << "depth" << depth
            << "nodes" << nodeArray
            << "nodeCount" << static_cast<int32_t>(nodes.size());
        
        // Add weight information if it exists
        if (!edgeWeights.empty()) {
            doc << "edgeWeights" << weightArray
                << "totalWeight" << totalWeight;
        }
        
        return doc << finalize;
}

Path findBasicPath(
    mongocxx::collection& collection,
    const bsoncxx::oid& startNodeId,
    const bsoncxx::oid& endNodeId,
    const std::string& connectToField,
    const std::string& connectFromField,
    int maxDepth) {
    
    Path resultPath;
    
    // Track visited nodes to avoid cycles
    std::unordered_set<bsoncxx::oid, OidHasher, OidEqual> visited;
    
    // Queue for BFS: (nodeId, depth)
    std::queue<std::pair<bsoncxx::oid, int>> queue;
    
    // Track parent nodes to reconstruct the path
    std::unordered_map<bsoncxx::oid, bsoncxx::oid, OidHasher, OidEqual> parent;
    
    // Track node documents for path reconstruction
    std::unordered_map<bsoncxx::oid, bsoncxx::document::value, OidHasher, OidEqual> nodeDocuments;
    
    // Start BFS from the start node
    queue.push(std::make_pair(startNodeId, 0));
    visited.insert(startNodeId);
    
    // Find the start node document
    {
        using namespace bsoncxx::builder::stream;
        auto filter = document{} << connectFromField << startNodeId << finalize;
        auto startNodeDoc = collection.find_one(filter.view());
        if (startNodeDoc) {
            nodeDocuments.insert(std::make_pair(
                startNodeId, 
                std::move(startNodeDoc.value())
            ));
        } else {
            // Start node not found
            return resultPath;
        }
    }
    
    bool pathFound = false;
    
    // BFS traversal
    while (!queue.empty()) {
        auto current = queue.front();
        auto currentId = current.first;
        auto depth = current.second;
        queue.pop();
        
        // Check if we've reached the target
        if (currentId == endNodeId) {
            resultPath.depth = depth;
            pathFound = true;
            break;
        }
        
        // Skip if we've reached maximum depth
        if (depth >= maxDepth) {
            continue;
        }
        
        // Get current node document
        auto currentDocIt = nodeDocuments.find(currentId);
        if (currentDocIt == nodeDocuments.end()) {
            continue; // Skip if document not found
        }
        
        bsoncxx::document::view currentDoc = currentDocIt->second.view();
        
        // Check if the node has connections
        if (currentDoc[connectToField] && currentDoc[connectToField].type() == bsoncxx::type::k_array) {
            // Iterate through connections
            for (auto&& connValue : currentDoc[connectToField].get_array().value) {
                bsoncxx::oid neighborId;
                
                // Handle different connection formats (direct OID or embedded document)
                if (connValue.type() == bsoncxx::type::k_oid) {
                    neighborId = connValue.get_oid().value;
                } else if (connValue.type() == bsoncxx::type::k_document && 
                           connValue.get_document().view()[connectFromField]) {
                    neighborId = connValue.get_document().view()[connectFromField].get_oid().value;
                } else {
                    continue; // Skip invalid connections
                }
                
                // Skip if already visited
                if (visited.find(neighborId) != visited.end()) {
                    continue;
                }
                
                // Mark as visited
                visited.insert(neighborId);
                
                // Record parent for path reconstruction
                parent[neighborId] = currentId;
                
                // Fetch the neighbor document
                using namespace bsoncxx::builder::stream;
                auto filter = document{} << connectFromField << neighborId << finalize;
                auto neighborDoc = collection.find_one(filter.view());
                
                if (neighborDoc) {
                    // Store the document for path reconstruction
                    nodeDocuments.insert(std::make_pair(
                        neighborId, 
                        std::move(neighborDoc.value())
                    ));
                    
                    // Add to the queue
                    queue.push(std::make_pair(neighborId, depth + 1));
                }
            }
        }
    }
    
    // If path found, reconstruct it
    if (pathFound) {
        // Start from end node and work backwards
        std::vector<bsoncxx::oid> path;
        bsoncxx::oid current = endNodeId;
        
        while (true) {
            path.push_back(current);
            if (current == startNodeId) {
                break;
            }
            
            auto parentIt = parent.find(current);
            if (parentIt == parent.end()) {
                // Something went wrong with our path tracking
                return resultPath;
            }
            current = parentIt->second;
        }
        
        // Reverse the path (start to end)
        std::reverse(path.begin(), path.end());
        
        // Add node documents to the result path
        for (const auto& nodeId : path) {
            auto docIt = nodeDocuments.find(nodeId);
            if (docIt != nodeDocuments.end()) {
                resultPath.nodes.push_back(docIt->second);
            }
        }
    }
    
    return resultPath;
}


struct Edge {
    std::string to;
    int weight;
};

struct PathStep {
    std::string node;
    int cost;
    std::vector<std::string> path;

    bool operator>(const PathStep& other) const {
        return cost > other.cost;
    }
};


Path findWeightedPathImpl(
    mongocxx::collection& collection,
    const std::string& start,
    const std::string& end,
    const std::string& connect_field,
    const std::string& id_field,
    const std::string& weight_field,
    int max_depth
) {
    Path result;

    // --- Build graph in memory ---
    std::unordered_map<std::string, std::vector<Edge>> graph;
    for (auto&& doc : collection.find({})) {
        std::string from = std::string(doc["from"].get_string().value);
        std::string to = std::string(doc["to"].get_string().value);
        int weight = doc["weight"].get_int32();

        graph[from].push_back({to, weight});
    }

    // --- Dijkstra-like search ---
    std::priority_queue<PathStep, std::vector<PathStep>, std::greater<>> pq;
    std::unordered_map<std::string, int> visited;

    pq.push({start, 0, {start}});

    while (!pq.empty()) {
        auto current = pq.top(); pq.pop();

        if (visited.count(current.node)) continue;
        visited[current.node] = current.cost;

        if (current.node == end) {
            result.found = true;
            for (const auto& node : current.path) {
                auto node_doc = document{} << "nodeId" << node << finalize;
                result.nodes.push_back(bsoncxx::document::value(node_doc));
            }
            result.depth = current.path.size() - 1;
            result.cost = current.cost;
            return result;
        }

        if (current.path.size() > static_cast<size_t>(max_depth)) continue;

        for (const auto& edge : graph[current.node]) {
            if (!visited.count(edge.to)) {
                auto newPath = current.path;
                newPath.push_back(edge.to);
                pq.push({edge.to, current.cost + edge.weight, newPath});
            }
        }
    }

    // No path found
    result.found = false;
    return result;
}

/**
 * Bidirectional Search Implementation
 */
Path mongo::graph_extension::findBidirectionalPathImpl(
    mongocxx::collection& collection,
    const bsoncxx::oid& startNodeId,
    const bsoncxx::oid& endNodeId,
    const std::string& connectToField,
    const std::string& connectFromField,
    int maxDepth) {
    
    Path resultPath;
    
    // Early exit check - if start and end are the same
    if (startNodeId == endNodeId) {
        // Fetch the single node and return
        using namespace bsoncxx::builder::stream;
        auto filter = document{} << connectFromField << startNodeId << finalize;
        auto nodeDoc = collection.find_one(filter.view());
        if (nodeDoc) {
            resultPath.nodes.push_back(std::move(nodeDoc.value()));
            resultPath.depth = 0;
        }
        return resultPath;
    }
    
    // Track visited nodes and their depths
    // Using unordered_map instead of unordered_set to also track depth and direction
    // Value: pair of (depth, direction) where direction is 1 for forward, -1 for backward
    std::unordered_map<bsoncxx::oid, std::pair<int, int>, OidHasher, OidEqual> visited;
    
    // Track parent nodes to reconstruct the path
    std::unordered_map<bsoncxx::oid, bsoncxx::oid, OidHasher, OidEqual> forwardParent;
    std::unordered_map<bsoncxx::oid, bsoncxx::oid, OidHasher, OidEqual> backwardParent;
    
    // Track node documents for path reconstruction
    std::unordered_map<bsoncxx::oid, bsoncxx::document::value, OidHasher, OidEqual> nodeDocuments;
    
    // Queues for bidirectional BFS: (nodeId, depth)
    std::queue<std::pair<bsoncxx::oid, int>> forwardQueue;
    std::queue<std::pair<bsoncxx::oid, int>> backwardQueue;
    
    // Initialize forward search (from start node)
    forwardQueue.push(std::make_pair(startNodeId, 0));
    visited[startNodeId] = std::make_pair(0, 1); // depth 0, forward direction
    
    // Initialize backward search (from end node)
    backwardQueue.push(std::make_pair(endNodeId, 0));
    visited[endNodeId] = std::make_pair(0, -1); // depth 0, backward direction
    
    // Fetch and store the start and end node documents
    {
        using namespace bsoncxx::builder::stream;
        auto filter = document{} << connectFromField << startNodeId << finalize;
        auto startNodeDoc = collection.find_one(filter.view());
        if (startNodeDoc) {
            nodeDocuments.insert(std::make_pair(
                startNodeId, 
                std::move(startNodeDoc.value())
            ));
        } else {
            // Start node not found
            return resultPath;
        }
        
        filter = document{} << connectFromField << endNodeId << finalize;
        auto endNodeDoc = collection.find_one(filter.view());
        if (endNodeDoc) {
            nodeDocuments.insert(std::make_pair(
                endNodeId, 
                std::move(endNodeDoc.value())
            ));
        } else {
            // End node not found
            return resultPath;
        }
    }
    
    // Variables to track meeting point
    bool pathFound = false;
    bsoncxx::oid meetingNode;
    int totalPathLength = -1;
    
    // Continue until either both queues are empty or max depth is reached from both directions
    while (!forwardQueue.empty() || !backwardQueue.empty()) {
        // Process one level at a time for better balance
        
        // Process forward queue (if not empty and within depth limit)
        if (!forwardQueue.empty()) {
            auto current = forwardQueue.front();
            auto currentId = current.first;
            auto depth = current.second;
            forwardQueue.pop();
            
            // Skip if we've reached maximum depth from forward direction
            if (depth >= maxDepth / 2) {
                continue;
            }
            
            // Get current node document
            auto currentDocIt = nodeDocuments.find(currentId);
            if (currentDocIt == nodeDocuments.end()) {
                continue; // Skip if document not found
            }
            
            bsoncxx::document::view currentDoc = currentDocIt->second.view();
            
            // Check for connections
            if (currentDoc[connectToField] && currentDoc[connectToField].type() == bsoncxx::type::k_array) {
                // Process all connections
                for (auto&& connValue : currentDoc[connectToField].get_array().value) {
                    bsoncxx::oid neighborId;
                    
                    // Handle different connection formats
                    if (connValue.type() == bsoncxx::type::k_oid) {
                        neighborId = connValue.get_oid().value;
                    } else if (connValue.type() == bsoncxx::type::k_document && 
                              connValue.get_document().view()[connectFromField]) {
                        neighborId = connValue.get_document().view()[connectFromField].get_oid().value;
                    } else {
                        continue; // Skip invalid connections
                    }
                    
                    // Check if this node has been visited
                    auto visitedIt = visited.find(neighborId);
                    
                    if (visitedIt == visited.end()) {
                        // Not visited - add to forward queue
                        forwardParent[neighborId] = currentId;
                        visited[neighborId] = std::make_pair(depth + 1, 1); // Forward direction
                        
                        // Fetch neighbor document
                        using namespace bsoncxx::builder::stream;
                        auto filter = document{} << connectFromField << neighborId << finalize;
                        auto neighborDoc = collection.find_one(filter.view());
                        
                        if (neighborDoc) {
                            nodeDocuments.insert(std::make_pair(
                                neighborId, 
                                std::move(neighborDoc.value())
                            ));
                            forwardQueue.push(std::make_pair(neighborId, depth + 1));
                        }
                    } 
                    else if (visitedIt->second.second == -1) {
                        // Visited from backward direction - the search fronts have met!
                        int forwardDepth = depth + 1;
                        int backwardDepth = visitedIt->second.first;
                        int pathLength = forwardDepth + backwardDepth;
                        
                        // Check if this is the first meeting or a shorter path
                        if (totalPathLength == -1 || pathLength < totalPathLength) {
                            meetingNode = neighborId;
                            totalPathLength = pathLength;
                            pathFound = true;
                            
                            // Record parent for the forward direction
                            forwardParent[neighborId] = currentId;
                        }
                    }
                }
            }
        }
        
        // If we found a path, we can optionally continue to look for shorter paths
        // Or we can break here for the first path found:
        if (pathFound) {
            break;
        }
        
        // Process backward queue (if not empty and within depth limit)
        if (!backwardQueue.empty()) {
            auto current = backwardQueue.front();
            auto currentId = current.first;
            auto depth = current.second;
            backwardQueue.pop();
            
            // Skip if we've reached maximum depth from backward direction
            if (depth >= maxDepth / 2) {
                continue;
            }
            
            // Get current node document
            auto currentDocIt = nodeDocuments.find(currentId);
            if (currentDocIt == nodeDocuments.end()) {
                continue; // Skip if document not found
            }
            
            // For backward search, we need to find nodes that have the current node in their connections
            // This requires a database query
            using namespace bsoncxx::builder::stream;
            
            // Query to find nodes that connect to the current node
            // Using $elemMatch for array fields, handling both direct OIDs and embedded documents
            auto forwardMatchFilter = document{} 
                << connectToField << open_document
                    << "$elemMatch" << open_document
                        << "$eq" << currentId
                    << close_document
                << close_document
                << finalize;
                
            // Alternative query for embedded documents if the connection is stored that way
            auto embeddedMatchFilter = document{} 
                << connectToField << open_document
                    << "$elemMatch" << open_document
                        << connectFromField << currentId
                    << close_document
                << close_document
                << finalize;
                
            // Combine queries with $or
            auto filter = document{} 
                << "$or" << open_array
                    << forwardMatchFilter.view()
                    << embeddedMatchFilter.view()
                << close_array
                << finalize;
                
            // Find all incoming connections
            auto cursor = collection.find(filter.view());
            
            for (auto&& doc : cursor) {
                bsoncxx::oid neighborId = doc[connectFromField].get_oid().value;
                
                // Check if this node has been visited
                auto visitedIt = visited.find(neighborId);
                
                if (visitedIt == visited.end()) {
                    // Not visited - add to backward queue
                    backwardParent[neighborId] = currentId;
                    visited[neighborId] = std::make_pair(depth + 1, -1); // Backward direction
                    nodeDocuments.insert(std::make_pair(neighborId, doc));
                    backwardQueue.push(std::make_pair(neighborId, depth + 1));
                } 
                else if (visitedIt->second.second == 1) {
                    // Visited from forward direction - the search fronts have met!
                    int forwardDepth = visitedIt->second.first;
                    int backwardDepth = depth + 1;
                    int pathLength = forwardDepth + backwardDepth;
                    
                    // Check if this is the first meeting or a shorter path
                    if (totalPathLength == -1 || pathLength < totalPathLength) {
                        meetingNode = neighborId;
                        totalPathLength = pathLength;
                        pathFound = true;
                        
                        // Record parent for the backward direction
                        backwardParent[neighborId] = currentId;
                    }
                }
            }
        }
        
        // If we found a path, we can optionally continue to look for shorter paths
        // Or we can break here for the first path found:
        if (pathFound) {
            break;
        }
    }
    
    // If path found, reconstruct it from both ends
    if (pathFound) {
        resultPath.depth = totalPathLength;
        
        // Reconstruct forward path (from start to meeting point)
        std::vector<bsoncxx::oid> forwardPath;
        bsoncxx::oid current = meetingNode;
        
        while (current != startNodeId) {
            forwardPath.push_back(current);
            auto parentIt = forwardParent.find(current);
            if (parentIt == forwardParent.end()) {
                break; // Something went wrong
            }
            current = parentIt->second;
        }
        forwardPath.push_back(startNodeId);
        std::reverse(forwardPath.begin(), forwardPath.end());
        
        // Reconstruct backward path (from meeting point to end)
        std::vector<bsoncxx::oid> backwardPath;
        current = meetingNode;
        
        while (current != endNodeId) {
            auto parentIt = backwardParent.find(current);
            if (parentIt == backwardParent.end()) {
                break; // Something went wrong
            }
            current = parentIt->second;
            backwardPath.push_back(current);
        }
        
        // Combine paths (note: meeting node appears only once)
        std::vector<bsoncxx::oid> completePath = forwardPath;
        completePath.insert(completePath.end(), backwardPath.begin(), backwardPath.end());
        
        // Add node documents to result path
        for (const auto& nodeId : completePath) {
            auto docIt = nodeDocuments.find(nodeId);
            if (docIt != nodeDocuments.end()) {
                resultPath.nodes.push_back(docIt->second);
            }
        }
    }
    
    return resultPath;
}

} // namespace graph_extension
} // namespace mongo