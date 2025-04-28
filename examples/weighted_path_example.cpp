#include <iostream>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include "mongo/graph_extension.h"

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <start_node> <end_node>" << std::endl;
        return 1;
    }

    std::string start_node = argv[1];
    std::string end_node = argv[2];

    mongocxx::instance instance{};
    mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};

    mongo::graph_extension::GraphExtension graphExt(client);

    auto result = graphExt.findWeightedPath(
        "graph",    // database name
        "edges",    // collection name
        start_node, // start node (string)
        end_node,   // end node (string)
        "from",     // connect field
        "_id",      // id field (ignored internally)
        "weight",   // weight field
        10          // max depth
    );

    auto resultView = result.view();

    std::cout << "\n=== Weighted Path Finding Result ===" << std::endl;
    std::cout << bsoncxx::to_json(resultView) << std::endl;

    return 0;
}
