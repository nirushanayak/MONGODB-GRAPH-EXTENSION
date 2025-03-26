# MongoDB Graph Extension

A C++ extension for MongoDB that adds powerful graph traversal and analysis capabilities while preserving MongoDB's flexible document model.

## Overview

MongoDB Graph Extension enhances MongoDB with native graph capabilities, eliminating the need for separate graph databases or complex application-level graph processing. This extension is designed for applications that need both rich document storage and efficient graph traversal.

## Features

### Current Capabilities
- **Path Finding**: Find paths between nodes in a MongoDB collection using breadth-first search
- **Complete Path Information**: Return all node documents along the discovered path
- **Flexible Connection Structure**: Support for both direct ObjectID references and embedded document connections
- **Cycle Detection**: Built-in handling for cyclic graph structures
- **Depth Limiting**: Control maximum traversal depth

### Planned Features
- **Weighted Path Finding**: Implement Dijkstra's algorithm for edge-weighted graphs
- **Bidirectional Search**: Optimize path finding by searching from both ends simultaneously
- **Multiple Path Results**: Return the top-k shortest paths between nodes
- **Graph Analytics**: Add functions for centrality, connected components, and community detection

## Use Cases

This extension is ideal for:
- Social networks and community platforms
- Knowledge management systems
- E-commerce and recommendation engines
- Healthcare information systems
- Fraud detection systems
- Enterprise knowledge graphs
- Content management systems

## Installation

### Prerequisites
- MongoDB (4.4+)
- MongoDB C++ Driver (4.0+)
- CMake (3.10+)
- C++17 compatible compiler

### Building from Source

1. Clone the repository:
```bash
git clone https://github.com/yourusername/mongodb-graph-extension.git
cd mongodb-graph-extension
```

2. Create a build directory:
```bash
mkdir build && cd build
```

3. Configure with CMake:
```bash
cmake ..
```

4. Build the library and examples:
```bash
make
```

## Usage

### Basic Path Finding

```cpp
#include <mongocxx/instance.hpp>
#include <mongocxx/client.hpp>
#include "mongo/graph_extension.h"

int main() {
    // Initialize MongoDB driver
    mongocxx::instance instance{};
    mongocxx::client client{mongocxx::uri{"mongodb://localhost:27017"}};
    
    // Create the graph extension
    mongo::graph_extension::GraphExtension graphExt(client);
    
    // Find a path from one node to another
    auto pathResult = graphExt.findPath(
        "myDatabase",        // Database name
        "nodes",             // Collection name
        startNodeId,         // Starting node ObjectID
        endNodeId,           // Target node ObjectID
        "connections",       // Field containing connections
        "_id",               // Field identifying nodes
        5                    // Maximum depth
    );
    
    // Process the result (BSON document)
    // The path result includes:
    // - Whether a path was found
    // - The path depth
    // - Complete documents for all nodes in the path
    // - The node count
    
    return 0;
}
```

### Example Result Format

```json
{
  "pathFound": true,
  "depth": 2,
  "nodes": [
    {
      "_id": {"$oid": "6098f69e1f4eae2b1c5e3fa1"},
      "name": "Node A",
      "connections": [...]
    },
    {
      "_id": {"$oid": "6098f69e1f4eae2b1c5e3fa2"},
      "name": "Node B",
      "connections": [...]
    },
    {
      "_id": {"$oid": "6098f69e1f4eae2b1c5e3fa3"},
      "name": "Node C",
      "connections": [...]
    }
  ],
  "nodeCount": 3
}
```

## Schema Design

The extension works with MongoDB collections that follow these graph data modeling patterns:

### Referencing Pattern

```json
{
  "_id": {"$oid": "6098f69e1f4eae2b1c5e3fa1"},
  "name": "Node A",
  "connections": [
    {"$oid": "6098f69e1f4eae2b1c5e3fa2"},
    {"$oid": "6098f69e1f4eae2b1c5e3fa3"}
  ]
}
```

### Embedding Pattern

```json
{
  "_id": {"$oid": "6098f69e1f4eae2b1c5e3fa1"},
  "name": "Node A",
  "connections": [
    {
      "nodeId": {"$oid": "6098f69e1f4eae2b1c5e3fa2"},
      "weight": 1.5
    },
    {
      "nodeId": {"$oid": "6098f69e1f4eae2b1c5e3fa3"},
      "weight": 2.3
    }
  ]
}
```

## Performance Considerations

The extension is optimized for:
- Document caching to minimize database lookups
- Early termination when targets are found
- Memory-efficient path tracking

For best performance:
- Keep connection arrays to a reasonable size
- Consider indexing the fields used in `connectFromField`
- For large graphs, limit the maximum depth appropriately

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
