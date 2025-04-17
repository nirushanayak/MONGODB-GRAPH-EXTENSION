// #include <iostream>
// #include <mongocxx/instance.hpp>
// #include <mongocxx/client.hpp>
// #include <mongocxx/uri.hpp>
// #include <bsoncxx/json.hpp>
// #include <bsoncxx/builder/stream/document.hpp>
// #include <bsoncxx/builder/stream/array.hpp>
// #include "mongo/graph_extension.h"

// void setupSampleData(mongocxx::client& client) {
//     // Get the test collection
//     auto collection = client["test"]["nodes"];
    
//     // Clear existing data
//     collection.delete_many({});
    
//     // Create sample data - a simple graph
//     using namespace bsoncxx::builder::stream;
    
//     // Create node IDs first
//     bsoncxx::oid nodeA_id = bsoncxx::oid();
//     bsoncxx::oid nodeB_id = bsoncxx::oid();
//     bsoncxx::oid nodeC_id = bsoncxx::oid();
//     bsoncxx::oid nodeD_id = bsoncxx::oid();
    
//     // Create nodes with connections using stream syntax
//     collection.insert_one(
//         document{} << "_id" << nodeA_id
//                   << "name" << "Node A"
//                   << "connections" << open_array
//                       << nodeB_id
//                       << nodeC_id
//                   << close_array
//                   << finalize
//     );
    
//     collection.insert_one(
//         document{} << "_id" << nodeB_id
//                   << "name" << "Node B"
//                   << "connections" << open_array
//                       << nodeD_id
//                   << close_array
//                   << finalize
//     );
    
//     collection.insert_one(
//         document{} << "_id" << nodeC_id
//                   << "name" << "Node C"
//                   << "connections" << open_array
//                       << nodeD_id
//                   << close_array
//                   << finalize
//     );
    
//     collection.insert_one(
//         document{} << "_id" << nodeD_id
//                   << "name" << "Node D"
//                   << "connections" << open_array
//                   << close_array  // Empty array
//                   << finalize
//     );
    
//     std::cout << "Sample data created with nodes:" << std::endl;
//     std::cout << "Node A: " << nodeA_id.to_string() << std::endl;
//     std::cout << "Node B: " << nodeB_id.to_string() << std::endl;
//     std::cout << "Node C: " << nodeC_id.to_string() << std::endl;
//     std::cout << "Node D: " << nodeD_id.to_string() << std::endl;
// }

// int main() {
//     // Initialize MongoDB driver
//     mongocxx::instance instance{};
//     mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};
    
//     // Set up sample data
//     setupSampleData(client);
    
//     // Get node IDs from the database for our test
//     auto collection = client["test"]["nodes"];
//     bsoncxx::oid startNodeId;
//     bsoncxx::oid endNodeId;
    
//     // Find Node A
//     {
//         auto filter = bsoncxx::builder::stream::document{} 
//                     << "name" << "Node A" 
//                     << bsoncxx::builder::stream::finalize;
//         auto result = collection.find_one(filter.view());
//         if (result) {
//             startNodeId = result->view()["_id"].get_oid().value;
//         }
//     }
    
//     // Find Node D
//     {
//         auto filter = bsoncxx::builder::stream::document{} 
//                     << "name" << "Node D" 
//                     << bsoncxx::builder::stream::finalize;
//         auto result = collection.find_one(filter.view());
//         if (result) {
//             endNodeId = result->view()["_id"].get_oid().value;
//         }
//     }
    
//     // Create the graph extension
//     mongo::graph_extension::GraphExtension graphExt(client);
    
//     // Find a path from Node A to Node D
//     auto pathResult = graphExt.findPath(
//         "test",
//         "nodes",
//         startNodeId,
//         endNodeId,
//         "connections",
//         "_id",
//         5  // max depth
//     );
    
//     // Print the result
//     std::cout << "\nPath finding result:" << std::endl;
//     std::cout << bsoncxx::to_json(pathResult) << std::endl;
    
//     return 0;
// }

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>

#include <iostream>
#include <unordered_map>
#include <vector>
#include <queue>
#include <limits>
#include <string>

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

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: ./path_example <start> <end>" << std::endl;
        return 1;
    }

    std::string start = argv[1];
    std::string end = argv[2];

    mongocxx::instance inst{};
    mongocxx::client conn{mongocxx::uri{}};
    auto db = conn["graph"];
    auto edges = db["edges"];

    std::unordered_map<std::string, std::vector<Edge>> graph;

    for (auto&& doc : edges.find({})) {
        std::string from = std::string(doc["from"].get_string().value);
        std::string to   = std::string(doc["to"].get_string().value);
        int weight = doc["weight"].get_int32();
        graph[from].push_back({to, weight});
    }

    std::priority_queue<PathStep, std::vector<PathStep>, std::greater<>> pq;
    std::unordered_map<std::string, int> visited;

    pq.push({start, 0, {start}});

    while (!pq.empty()) {
        auto current = pq.top(); pq.pop();

        if (visited.count(current.node)) continue;
        visited[current.node] = current.cost;

        if (current.node == end) {
            std::cout << "Shortest path from " << start << " to " << end << ":\n";
            for (const auto& node : current.path) {
                std::cout << node << (node == end ? "" : " -> ");
            }
            std::cout << "\nTotal cost: " << current.cost << std::endl;
            return 0;
        }

        for (const auto& edge : graph[current.node]) {
            if (!visited.count(edge.to)) {
                auto newPath = current.path;
                newPath.push_back(edge.to);
                pq.push({edge.to, current.cost + edge.weight, newPath});
            }
        }
    }

    std::cout << "No path found from " << start << " to " << end << std::endl;
    return 1;
}
