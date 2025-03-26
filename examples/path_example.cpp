#include <iostream>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include "mongo/graph_extension.h"

void setupSampleData(mongocxx::client& client) {
    // Get the test collection
    auto collection = client["test"]["nodes"];
    
    // Clear existing data
    collection.delete_many({});
    
    // Create sample data - a simple graph
    using namespace bsoncxx::builder::stream;
    
    // Create node IDs first
    bsoncxx::oid nodeA_id = bsoncxx::oid();
    bsoncxx::oid nodeB_id = bsoncxx::oid();
    bsoncxx::oid nodeC_id = bsoncxx::oid();
    bsoncxx::oid nodeD_id = bsoncxx::oid();
    
    // Create nodes with connections using stream syntax
    collection.insert_one(
        document{} << "_id" << nodeA_id
                  << "name" << "Node A"
                  << "connections" << open_array
                      << nodeB_id
                      << nodeC_id
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeB_id
                  << "name" << "Node B"
                  << "connections" << open_array
                      << nodeD_id
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeC_id
                  << "name" << "Node C"
                  << "connections" << open_array
                      << nodeD_id
                  << close_array
                  << finalize
    );
    
    collection.insert_one(
        document{} << "_id" << nodeD_id
                  << "name" << "Node D"
                  << "connections" << open_array
                  << close_array  // Empty array
                  << finalize
    );
    
    std::cout << "Sample data created with nodes:" << std::endl;
    std::cout << "Node A: " << nodeA_id.to_string() << std::endl;
    std::cout << "Node B: " << nodeB_id.to_string() << std::endl;
    std::cout << "Node C: " << nodeC_id.to_string() << std::endl;
    std::cout << "Node D: " << nodeD_id.to_string() << std::endl;
}

int main() {
    // Initialize MongoDB driver
    mongocxx::instance instance{};
    mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};
    
    // Set up sample data
    setupSampleData(client);
    
    // Get node IDs from the database for our test
    auto collection = client["test"]["nodes"];
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
    
    // Find Node D
    {
        auto filter = bsoncxx::builder::stream::document{} 
                    << "name" << "Node D" 
                    << bsoncxx::builder::stream::finalize;
        auto result = collection.find_one(filter.view());
        if (result) {
            endNodeId = result->view()["_id"].get_oid().value;
        }
    }
    
    // Create the graph extension
    mongo::graph_extension::GraphExtension graphExt(client);
    
    // Find a path from Node A to Node D
    auto pathResult = graphExt.findPath(
        "test",
        "nodes",
        startNodeId,
        endNodeId,
        "connections",
        "_id",
        5  // max depth
    );
    
    // Print the result
    std::cout << "\nPath finding result:" << std::endl;
    std::cout << bsoncxx::to_json(pathResult) << std::endl;
    
    return 0;
}