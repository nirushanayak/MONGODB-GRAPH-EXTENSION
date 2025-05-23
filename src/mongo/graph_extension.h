#pragma once

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <string>
#include <vector>

namespace mongo {
namespace graph_extension {

/**
 * Main interface for MongoDB Graph Extension
 */
class GraphExtension {
public:
    GraphExtension(mongocxx::client& client);

    /**
     * Find paths between nodes using enhanced algorithms
     */
    bsoncxx::document::value findPath(
        const std::string& dbName,
        const std::string& collectionName,
        const bsoncxx::oid& startNodeId,
        const bsoncxx::oid& endNodeId,
        const std::string& connectToField,
        const std::string& connectFromField,
        int maxDepth = 10);

    bsoncxx::document::value findWeightedPath(
        const std::string& db_name,
        const std::string& collection_name,
        const std::string& start,
        const std::string& end,
        const std::string& connect_field,
        const std::string& id_field,
        const std::string& weight_field,
        int max_depth
    );
    /**
     * Find paths between nodes using bidirectional search algorithm
     * This significantly improves performance for large graphs compared to the basic BFS approach
     */
    bsoncxx::document::value findBidirectionalPath(
        const std::string& dbName,
        const std::string& collectionName,
        const bsoncxx::oid& startNodeId,
        const bsoncxx::oid& endNodeId,
        const std::string& connectToField,
        const std::string& connectFromField,
        int maxDepth = 10);

private:
    mongocxx::client& _client;
};

} // namespace graph_extension
} // namespace mongo