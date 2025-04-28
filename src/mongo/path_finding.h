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
    
    Path() : depth(0) {}
    
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
    const mongocxx::collection& collection,
    const bsoncxx::oid& start,
    const bsoncxx::oid& end,
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
    const std::string& weightField,
    int maxDepth);

} // namespace graph_extension
} // namespace mongo