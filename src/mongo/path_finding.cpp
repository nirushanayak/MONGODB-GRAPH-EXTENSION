#include "path_finding.h"
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>

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
    
    // Build the document
    document doc;
    doc << "pathFound" << !nodes.empty()
        << "depth" << depth
        << "nodes" << nodeArray
        << "nodeCount" << static_cast<int32_t>(nodes.size());
    
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

} // namespace graph_extension
} // namespace mongo