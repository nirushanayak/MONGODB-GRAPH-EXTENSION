// // weighted_path_benchmark.cpp

// #include <iostream>
// #include <chrono>
// #include <mongocxx/instance.hpp>
// #include <mongocxx/client.hpp>
// #include <mongocxx/uri.hpp>
// #include <mongocxx/pipeline.hpp>
// #include <bsoncxx/json.hpp>
// #include <bsoncxx/builder/stream/document.hpp>
// #include "mongo/graph_extension.h"

// // Timing helper
// template<typename Func>
// double measure_time(Func func) {
//     auto start = std::chrono::high_resolution_clock::now();
//     func();
//     auto end = std::chrono::high_resolution_clock::now();
//     return std::chrono::duration<double, std::milli>(end - start).count();
// }

// int main() {
//     using namespace bsoncxx::builder::stream;

//     mongocxx::instance instance{};
//     mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};
//     auto db = client["graph"];
//     auto edges = db["edges"];

//     // Hardcoded Start and End Nodes
//     std::string start_node = "A";
//     std::string end_node = "D";

//     mongo::graph_extension::GraphExtension graphExt(client);

//     std::cout << "✅ Start Node: " << start_node << std::endl;
//     std::cout << "✅ End Node: " << end_node << std::endl;

//     // --- MongoDB $graphLookup Benchmark ---
//     std::cout << "\n=== Benchmark: MongoDB $graphLookup ===" << std::endl;
//     double mongo_time = measure_time([&]() {
//         mongocxx::pipeline p;
//         p.match(document{} << "from" << start_node << finalize);
//         p.graph_lookup(document{}
//             << "from" << "edges"
//             << "startWith" << "$to"
//             << "connectFromField" << "to"
//             << "connectToField" << "from"
//             << "as" << "path"
//             << "maxDepth" << 10
//             << finalize);

//         auto cursor = edges.aggregate(p);
//         for (auto&& doc : cursor) {
//             (void)doc; // Just consume results
//         }
//     });
//     std::cout << "MongoDB $graphLookup time: " << mongo_time << " ms" << std::endl;

//     // --- Our Weighted Path Benchmark ---
//     std::cout << "\n=== Benchmark: Custom Weighted Path ===" << std::endl;
//     double weighted_time = measure_time([&]() {
//         graphExt.findWeightedPath(
//             "graph",    // Database
//             "edges",    // Collection
//             start_node,
//             end_node,
//             "from",
//             "_id",      // Ignored inside
//             "weight",
//             10          // Max depth
//         );
//     });
//     std::cout << "Custom Weighted Path time: " << weighted_time << " ms" << std::endl;

//     // --- Performance Comparison ---
//     std::cout << "\n=== Performance Comparison ===" << std::endl;
//     if (weighted_time > 0.0) {
//         double speedup = mongo_time / weighted_time;
//         std::cout << "Custom Weighted Path is " << speedup << "x faster than MongoDB $graphLookup." << std::endl;
//     } else {
//         std::cout << "⚠️ Weighted path time too small to compute speedup!" << std::endl;
//     }

//     return 0;
// }

// weighted_path_benchmark.cpp

#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/pipeline.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include "mongo/graph_extension.h"

// Timing helper
template<typename Func>
double measure_time(Func func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

int main() {
    using namespace bsoncxx::builder::stream;

    mongocxx::instance instance{};
    mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};
    auto db = client["graph"];
    auto edges = db["edges"];

    // Hardcoded Start and End Nodes
    // std::string start_node = "A";
    // std::string end_node = "D";
    std::string start_node = "N6236";
std::string end_node = "N1283";

    mongo::graph_extension::GraphExtension graphExt(client);

    std::cout << "✅ Start Node: " << start_node << std::endl;
    std::cout << "✅ End Node: " << end_node << std::endl;

    // --- MongoDB $graphLookup Benchmark ---
    std::cout << "\n=== Benchmark: MongoDB $graphLookup ===" << std::endl;
    double mongo_time = measure_time([&]() {
        mongocxx::pipeline p;
        p.match(document{} << "from" << start_node << finalize);
        p.graph_lookup(document{}
            << "from" << "edges"
            << "startWith" << "$to"
            << "connectFromField" << "to"
            << "connectToField" << "from"
            << "as" << "path"
            << "maxDepth" << 10
            << finalize);

        auto cursor = edges.aggregate(p);
        for (auto&& doc : cursor) {
            (void)doc; // Just consume results
        }
    });
    std::cout << "MongoDB $graphLookup time: " << mongo_time << " ms" << std::endl;

    // --- Our Weighted Path Benchmark ---
    std::cout << "\n=== Benchmark: Custom Weighted Path ===" << std::endl;
    bsoncxx::document::value path_result = bsoncxx::builder::stream::document{} << finalize;

    double weighted_time = measure_time([&]() {
        path_result = graphExt.findWeightedPath(
            "graph",    // Database
            "edges",    // Collection
            start_node,
            end_node,
            "from",
            "_id",      // Ignored internally
            "weight",
            10          // Max depth
        );
    });
    std::cout << "Custom Weighted Path time: " << weighted_time << " ms" << std::endl;

    // --- Show Path Found ---
    std::cout << "\n=== Path Found ===" << std::endl;
    auto view = path_result.view();
    if (view["pathFound"] && view["pathFound"].get_bool()) {
        std::cout << "✅ Path found with depth: " << view["depth"].get_int32() << std::endl;
        if (view["nodes"] && view["nodes"].type() == bsoncxx::type::k_array) {
            std::cout << "Path: ";
            for (auto&& elem : view["nodes"].get_array().value) {
                auto node_doc = elem.get_document().view();
                if (node_doc["nodeId"]) {
    std::cout << std::string(node_doc["nodeId"].get_string().value) << " ";

}
            }
            std::cout << std::endl;
        }
        if (view["totalWeight"]) {
            std::cout << "Total Weight: " << view["totalWeight"].get_double() << std::endl;
        }
    } else {
        std::cout << "❌ No path found." << std::endl;
    }

    // --- Performance Comparison ---
    std::cout << "\n=== Performance Comparison ===" << std::endl;
    if (weighted_time > 0.0) {
        double speedup = mongo_time / weighted_time;
        std::cout << "Custom Weighted Path is " << speedup << "x faster than MongoDB $graphLookup." << std::endl;
    } else {
        std::cout << "⚠️ Weighted path time too small to compute speedup!" << std::endl;
    }

    return 0;
}
