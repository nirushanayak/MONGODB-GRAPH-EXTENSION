#include <iostream>
#include <chrono>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include "mongo/graph_extension.h"

// Utility function for measuring execution time
template<typename Func>
double measure_execution_time(Func func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

// Setup a larger sample graph for demonstrating performance differences
void setupLargerSampleGraph(mongocxx::client& client, int size = 100) {
    // Get the test collection
    auto collection = client["test"]["larger_graph"];
    
    // Clear existing data
    collection.delete_many({});
    
    // Create a graph with 'size' nodes in a grid-like pattern
    // This creates a challenging graph for path finding
    using namespace bsoncxx::builder::stream;
    
    std::vector<bsoncxx::oid> node_ids;
    
    // First, create all node IDs
    for (int i = 0; i < size; i++) {
        node_ids.push_back(bsoncxx::oid());
    }
    
    // Now create the nodes with connections
    for (int i = 0; i < size; i++) {
        // Grid size - assuming a square grid
        int grid_size = static_cast<int>(std::sqrt(size));
        
        // Create connections based on grid position
        array connections_array;
        
        // Connect to right neighbor
        if ((i % grid_size) < (grid_size - 1)) {
            connections_array << node_ids[i + 1];
        }
        
        // Connect to left neighbor
        if ((i % grid_size) > 0) {
            connections_array << node_ids[i - 1];
        }
        
        // Connect to bottom neighbor
        if (i + grid_size < size) {
            connections_array << node_ids[i + grid_size];
        }
        
        // Connect to top neighbor
        if (i - grid_size >= 0) {
            connections_array << node_ids[i - grid_size];
        }
        
        // Add some random long-range connections to make path finding more interesting
        // This creates small-world network properties
        if (i % 10 == 0) {
            int random_target = (i + (grid_size * 2) + 3) % size;
            connections_array << node_ids[random_target];
        }
        
        // Create the node document
        document node_doc;
        node_doc << "_id" << node_ids[i]
                << "name" << ("Node " + std::to_string(i))
                << "x" << (i % grid_size)
                << "y" << (i / grid_size)
                << "connections" << connections_array;
        
        collection.insert_one(node_doc << finalize);
    }
    
    std::cout << "Created a graph with " << size << " nodes in a grid-like pattern" << std::endl;
    std::cout << "Start Node ID: " << node_ids[0].to_string() << std::endl;
    std::cout << "End Node ID: " << node_ids[size-1].to_string() << std::endl;
}

int main() {
    // Initialize MongoDB driver
    mongocxx::instance instance{};
    mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};
    
    // Setup larger test graph for benchmarking
    setupLargerSampleGraph(client, 100); // 100 node graph
    
    // Create the graph extension
    mongo::graph_extension::GraphExtension graphExt(client);
    
    // Get start and end node IDs
    auto collection = client["test"]["larger_graph"];
    bsoncxx::oid startNodeId;
    bsoncxx::oid endNodeId;
    
    // Find node 0 (start)
    {
        auto filter = bsoncxx::builder::stream::document{} 
                    << "name" << "Node 0" 
                    << bsoncxx::builder::stream::finalize;
        auto result = collection.find_one(filter.view());
        if (result) {
            startNodeId = result->view()["_id"].get_oid().value;
        }
    }
    
    // Find node 99 (end)
    {
        auto filter = bsoncxx::builder::stream::document{} 
                    << "name" << "Node 99" 
                    << bsoncxx::builder::stream::finalize;
        auto result = collection.find_one(filter.view());
        if (result) {
            endNodeId = result->view()["_id"].get_oid().value;
        }
    }
    
    // Test 1: Run basic path finding
    std::cout << "\n=== Running Basic Path Finding ===" << std::endl;
    
    // Initialize with an empty document
    bsoncxx::builder::stream::document empty_doc{};
    bsoncxx::document::value basicPathResult{empty_doc.view()};
    
    double basicTime = measure_execution_time([&]() {
        basicPathResult = graphExt.findPath(
            "test",
            "larger_graph",
            startNodeId,
            endNodeId,
            "connections",
            "_id",
            20  // max depth
        );
    });
    
    auto basicPathView = basicPathResult.view();
    bool basicPathFound = basicPathView["pathFound"].get_bool().value;
    int basicPathDepth = basicPathView["depth"].get_int32().value;
    int basicPathNodeCount = basicPathView["nodeCount"].get_int32().value;
    
    std::cout << "Basic Path Finding Results:" << std::endl;
    std::cout << "- Execution time: " << basicTime << " ms" << std::endl;
    std::cout << "- Path found: " << (basicPathFound ? "Yes" : "No") << std::endl;
    std::cout << "- Path depth: " << basicPathDepth << std::endl;
    std::cout << "- Node count: " << basicPathNodeCount << std::endl;
    
    // Test 2: Run bidirectional path finding
    std::cout << "\n=== Running Bidirectional Path Finding ===" << std::endl;
    
    // Initialize with an empty document
    bsoncxx::document::value biPathResult{empty_doc.view()};
    
    double biTime = measure_execution_time([&]() {
        biPathResult = graphExt.findBidirectionalPath(
            "test",
            "larger_graph",
            startNodeId,
            endNodeId,
            "connections",
            "_id",
            20  // max depth
        );
    });
    
    auto biPathView = biPathResult.view();
    bool biPathFound = biPathView["pathFound"].get_bool().value;
    int biPathDepth = biPathView["depth"].get_int32().value;
    int biPathNodeCount = biPathView["nodeCount"].get_int32().value;
    
    std::cout << "Bidirectional Path Finding Results:" << std::endl;
    std::cout << "- Execution time: " << biTime << " ms" << std::endl;
    std::cout << "- Path found: " << (biPathFound ? "Yes" : "No") << std::endl;
    std::cout << "- Path depth: " << biPathDepth << std::endl;
    std::cout << "- Node count: " << biPathNodeCount << std::endl;
    
    // Performance comparison
    std::cout << "\n=== Performance Comparison ===" << std::endl;
    double speedup = basicTime / biTime;
    std::cout << "Bidirectional search was " << speedup << "x faster than basic search" << std::endl;
    
    // Path comparison
    std::cout << "\n=== Path Comparison ===" << std::endl;
    std::cout << "Basic path length: " << basicPathNodeCount << " nodes, depth: " << basicPathDepth << std::endl;
    std::cout << "Bidirectional path length: " << biPathNodeCount << " nodes, depth: " << biPathDepth << std::endl;
    
    // Check if paths are equivalent in length
    if (basicPathDepth == biPathDepth) {
        std::cout << "Both algorithms found paths of equal depth" << std::endl;
    } else {
        std::cout << "Note: Different path depths found. This can happen in graphs with multiple equal-length paths." << std::endl;
    }
    
    return 0;
}