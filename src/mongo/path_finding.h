#pragma once

#include <mongocxx/collection.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/document/value.hpp>
#include <string>
#include <vector>

namespace mongo {
namespace graph_extension {

/**
 * Represents a path through the graph
 */
struct Path {
    std::vector<bsoncxx::document::value> nodes;
    std::vector<double> edgeWeights;  // Store weights of each edge
    double totalWeight;  // Total path weight
    int depth;
    bool found;      // <--- Add this line
    int cost; 
    
    Path() : depth(0), totalWeight(0), found(false), cost(0) {}
    
    // Convert path to BSON document
    bsoncxx::document::value toBSON() const;
};

/**
 * Find a path between nodes in a MongoDB collection
 */
Path findBasicPath(
    mongocxx::collection& collection,
    const bsoncxx::oid& startNodeId,
    const bsoncxx::oid& endNodeId,
    const std::string& connectToField,
    const std::string& connectFromField,
    int maxDepth);

Path findWeightedPathImpl(
    mongocxx::collection& collection,
    const std::string& start,
    const std::string& end,
    const std::string& connect_field,
    const std::string& id_field,
    const std::string& weight_field,
    int max_depth);

/**
 * Find a path between nodes using bidirectional search to improve performance
 * on large graphs. Searches simultaneously from start node and end node.
 */
Path findBidirectionalPathImpl(
    mongocxx::collection& collection,
    const bsoncxx::oid& startNodeId,
    const bsoncxx::oid& endNodeId,
    const std::string& connectToField,
    const std::string& connectFromField,
    int maxDepth);

} // namespace graph_extension
} // namespace mongo