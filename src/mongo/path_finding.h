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

} // namespace graph_extension
} // namespace mongo