#include <iostream>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include "mongo/graph_extension.h"

void setupWeightedSampleData(mongocxx::client& client) {
    // Get the test collection
    auto collection = client["test"]["weighted_nodes"];
    
    // Clear existing data
    collection.delete_many({});
    
    // Create sample data - a simple weighted graph
    using namespace bsoncxx::builder::stream;
    
    // Create node IDs first
    bsoncxx::oid nodeA_id = bsoncxx::oid();
    bsoncxx::oid nodeB_id = bsoncxx::oid();
    bsoncxx::oid nodeC_id = bsoncxx::oid();
    bsoncxx::oid nodeD_id = bsoncxx::oid();
    bsoncxx::oid nodeE_id = bsoncxx::oid();
    
    // Create nodes with weighted connections
    collection.insert_one(
        document{} << "_id" << nodeA_id
                  << "name" << "Node A"
                  << "connections" << open_array
                      << open_document
                          << "nodeId" << nodeB_id
                          << "weight" << 5
                      << close_document
                      << open_document
                          << "nodeId" << nodeC_id
                          << "weight" << 10
                      << close_document
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeB_id
                  << "name" << "Node B"
                  << "connections" << open_array
                      << open_document
                          << "nodeId" << nodeD_id
                          << "weight" << 3
                      << close_document
                      << open_document
                          << "nodeId" << nodeE_id
                          << "weight" << 8
                      << close_document
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeC_id
                  << "name" << "Node C"
                  << "connections" << open_array
                      << open_document
                          << "nodeId" << nodeD_id
                          << "weight" << 2
                      << close_document
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeD_id
                  << "name" << "Node D"
                  << "connections" << open_array
                      << open_document
                          << "nodeId" << nodeE_id
                          << "weight" << 1
                      << close_document
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeE_id
                  << "name" << "Node E"
                  << "connections" << open_array
                  << close_array  // Empty array
                  << finalize
    );
    
    std::cout << "Weighted sample data created with nodes:" << std::endl;
    std::cout << "Node A: " << nodeA_id.to_string() << std::endl;
    std::cout << "Node B: " << nodeB_id.to_string() << std::endl;
    std::cout << "Node C: " << nodeC_id.to_string() << std::endl;
    std::cout << "Node D: " << nodeD_id.to_string() << std::endl;
    std::cout << "Node E: " << nodeE_id.to_string() << std::endl;
}

void setupSimpleWeightedData(mongocxx::client& client) {
    // Get the test collection
    auto collection = client["test"]["simple_nodes"];
    
    // Clear existing data
    collection.delete_many({});
    
    // Create node IDs
    bsoncxx::oid nodeA_id = bsoncxx::oid();
    bsoncxx::oid nodeB_id = bsoncxx::oid();
    bsoncxx::oid nodeC_id = bsoncxx::oid();
    
    // Create nodes with direct OID connections (no embedded documents)
    using namespace bsoncxx::builder::stream;
    
    collection.insert_one(
        document{} << "_id" << nodeA_id
                  << "name" << "Node A"
                  << "connections" << open_array
                      << nodeB_id
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeB_id
                  << "name" << "Node B"
                  << "connections" << open_array
                      << nodeC_id
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeC_id
                  << "name" << "Node C"
                  << "connections" << open_array
                  << close_array
                  << finalize
    );
    
    std::cout << "Simple data created with nodes:" << std::endl;
    std::cout << "Node A: " << nodeA_id.to_string() << std::endl;
    std::cout << "Node B: " << nodeB_id.to_string() << std::endl;
    std::cout << "Node C: " << nodeC_id.to_string() << std::endl;
    
    // Test with simple unweighted path
    mongo::graph_extension::GraphExtension graphExt(client);
    
    auto basicPathResult = graphExt.findPath(
        "test",
        "simple_nodes",
        nodeA_id,
        nodeC_id,
        "connections",
        "_id",
        5
    );
    
    std::cout << "\nTest simple path: " << std::endl;
    std::cout << bsoncxx::to_json(basicPathResult) << std::endl;
}
int main() {
    // Initialize MongoDB driver
    mongocxx::instance instance{};
    mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};
    
    // Set up weighted sample data
    // setupWeightedSampleData(client);
    setupSimpleWeightedData(client);
    
    // Get node IDs from the database for our test
    auto collection = client["test"]["weighted_nodes"];
    bsoncxx::oid startNodeId;
    bsoncxx::oid endNodeId;
    
    // Find Node A
    {
        auto filter = bsoncxx::builder::stream::document{} 
                    << "name" << "Node A" 
                    << bsoncxx::builder::stream::finalize;
        auto result = collection.find_one(filter.view());
        if (result) {
            startNodeId = result->view()["_id"].get_oid().value;
        }
    }
    
    // Find Node E
    {
        auto filter = bsoncxx::builder::stream::document{} 
                    << "name" << "Node E" 
                    << bsoncxx::builder::stream::finalize;
        auto result = collection.find_one(filter.view());
        if (result) {
            endNodeId = result->view()["_id"].get_oid().value;
        }
    }
    
    // Create the graph extension
    mongo::graph_extension::GraphExtension graphExt(client);
    
    // First, test regular unweighted path finding
    std::cout << "\nFinding regular (unweighted) path:" << std::endl;
    auto basicPathResult = graphExt.findPath(
        "test",
        "weighted_nodes",
        startNodeId,
        endNodeId,
        "connections",
        "nodeId",
        5  // max depth
    );
    std::cout << bsoncxx::to_json(basicPathResult) << std::endl;
    
    // Now test weighted path finding
    std::cout << "\nFinding weighted path:" << std::endl;
    auto weightedPathResult = graphExt.findWeightedPath(
        "test",
        "weighted_nodes",
        startNodeId,
        endNodeId,
        "connections",
        "nodeId",
        "weight",
        5  // max depth
    );
    std::cout << bsoncxx::to_json(weightedPathResult) << std::endl;
    
    // Print a comparison of results
    auto basicPathDoc = basicPathResult.view();
    auto weightedPathDoc = weightedPathResult.view();
    
    int basicDepth = basicPathDoc["depth"].get_int32().value;
    int weightedDepth = weightedPathDoc["depth"].get_int32().value;
    int basicNodeCount = basicPathDoc["nodeCount"].get_int32().value;
    int weightedNodeCount = weightedPathDoc["nodeCount"].get_int32().value;
    double totalWeight = 0.0;
    
    if (weightedPathDoc["totalWeight"]) {
        totalWeight = weightedPathDoc["totalWeight"].get_double().value;
    }
    
    std::cout << "\nComparison of results:" << std::endl;
    std::cout << "Basic path depth: " << basicDepth << std::endl;
    std::cout << "Weighted path depth: " << weightedDepth << std::endl;
    std::cout << "Basic path node count: " << basicNodeCount << std::endl;
    std::cout << "Weighted path node count: " << weightedNodeCount << std::endl;
    std::cout << "Weighted path total weight: " << totalWeight << std::endl;
    
    // Check if paths differ
    if (basicNodeCount != weightedNodeCount) {
        std::cout << "\nThe unweighted and weighted paths are different!" << std::endl;
        std::cout << "This is expected, as weighted path finding optimizes for weight, not hop count." << std::endl;
    }
    
    return 0;
}