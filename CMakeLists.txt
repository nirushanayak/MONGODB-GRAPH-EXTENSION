cmake_minimum_required(VERSION 3.10)
project(mongodb-graph-extension)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === MongoDB C++ driver paths ===
set(MONGOCXX_ROOT "/opt/homebrew/Cellar/mongo-cxx-driver/4.0.0")
set(MONGOCXX_INCLUDE_DIR "${MONGOCXX_ROOT}/include/mongocxx/v_noabi")
set(BSONCXX_INCLUDE_DIR "${MONGOCXX_ROOT}/include/bsoncxx/v_noabi")
set(MONGOCXX_LIB_DIR "${MONGOCXX_ROOT}/lib")

# === Include headers ===
include_directories(
    ${MONGOCXX_INCLUDE_DIR}
    ${BSONCXX_INCLUDE_DIR}
    "${MONGOCXX_ROOT}/include"  # root include, helps resolve nested includes
    ${CMAKE_SOURCE_DIR}/src
)

# === Link driver libraries ===
link_directories(
    ${MONGOCXX_LIB_DIR}
)

# === Build shared library ===
add_library(
    mongodb-graph-extension SHARED
    src/mongo/graph_extension.cpp
    src/mongo/path_finding.cpp
)

# === Link with mongo drivers ===
target_link_libraries(
    mongodb-graph-extension
    mongocxx
    bsoncxx
)

# === Build path_example executable ===
add_executable(
    path_example
    examples/path_example.cpp
)

# === Link path_example with the shared lib and drivers ===
target_link_libraries(
    path_example
    mongodb-graph-extension
    mongocxx
    bsoncxx
)


# Build the bidirectional path example
add_executable(
    bidirectional_example
    examples/bidirectional_example.cpp
)

target_link_libraries(
    bidirectional_example
    mongodb-graph-extension
    mongocxx
    bsoncxx
)

