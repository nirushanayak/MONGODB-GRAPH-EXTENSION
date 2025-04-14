#include "graph_extension.h"
#include "path_finding.h"

namespace mongo {
namespace graph_extension {

GraphExtension::GraphExtension(mongocxx::client& client) 
    : _client(client) {}

bsoncxx::document::value GraphExtension::findPath(
    const std::string& dbName,
    const std::string& collectionName,
    const bsoncxx::oid& startNodeId,
    const bsoncxx::oid& endNodeId,
    const std::string& connectToField,
    const std::string& connectFromField,
    int maxDepth) {
    
    // Get the collection
    auto collection = _client[dbName][collectionName];
    
    // Use path finding implementation
    Path path = findBasicPath(
        collection,
        startNodeId,
        endNodeId,
        connectToField,
        connectFromField,
        maxDepth);
    
    // Convert path to BSON
    return path.toBSON();
}

bsoncxx::document::value GraphExtension::findBidirectionalPath(
    const std::string& dbName,
    const std::string& collectionName,
    const bsoncxx::oid& startNodeId,
    const bsoncxx::oid& endNodeId,
    const std::string& connectToField,
    const std::string& connectFromField,
    int maxDepth) {
    
    // Get the collection
    auto collection = _client[dbName][collectionName];
    
    // Use bidirectional path finding implementation
    Path path = findBidirectionalPathImpl(
        collection,
        startNodeId,
        endNodeId,
        connectToField,
        connectFromField,
        maxDepth);
    
    // Convert path to BSON
    return path.toBSON();
}

} // namespace graph_extension
} // namespace mongo