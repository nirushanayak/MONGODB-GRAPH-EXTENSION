// bidirectional_example.cpp

#include <iostream>
#include <chrono>
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

    // Hardcoded ObjectIds
    bsoncxx::oid startNodeId("6810209ab7518ea0b767ee42");
    bsoncxx::oid endNodeId("6810209ab7518ea0b767f229");

    std::cout << "✅ Start Node: " << startNodeId.to_string() << std::endl;
    std::cout << "✅ End Node: " << endNodeId.to_string() << std::endl;

    mongo::graph_extension::GraphExtension graphExt(client);

    std::cout << "\n=== Benchmark: MongoDB $graphLookup ===" << std::endl;
    double mongo_time = measure_time([&]() {
        mongocxx::pipeline p;
        p.match(document{} << "from" << startNodeId << finalize);
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
            (void)doc; // consume documents
        }
    });

    std::cout << "MongoDB $graphLookup time: " << mongo_time << " ms\n";

    std::cout << "\n=== Benchmark: Bidirectional Path Finding ===" << std::endl;
    double bidi_time = measure_time([&]() {
        graphExt.findBidirectionalPath(
            "graph",
            "edges",
            startNodeId,
            endNodeId,
            "to",
            "_id",
            10
        );
    });

    std::cout << "Bidirectional Path Finding time: " << bidi_time << " ms\n";

    std::cout << "\n=== Performance Comparison ===" << std::endl;
    double speedup = mongo_time / bidi_time;
    std::cout << "Bidirectional is " << speedup << "x faster than MongoDB $graphLookup." << std::endl;

    return 0;
}
