cmake_minimum_required(VERSION 3.10)
project(mongodb-graph-extension)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set MongoDB C++ driver paths more explicitly
set(MONGOCXX_INCLUDE_DIR "/opt/homebrew/Cellar/mongo-cxx-driver/4.0.0/include/mongocxx/v_noabi")
set(BSONCXX_INCLUDE_DIR "/opt/homebrew/Cellar/mongo-cxx-driver/4.0.0/include/bsoncxx/v_noabi")
set(MONGOCXX_LIB_DIR "/opt/homebrew/Cellar/mongo-cxx-driver/4.0.0/lib")

include_directories(
    ${MONGOCXX_INCLUDE_DIR}
    ${BSONCXX_INCLUDE_DIR}
    "/opt/homebrew/Cellar/mongo-cxx-driver/4.0.0/include"  # Base include directory
    ${CMAKE_SOURCE_DIR}/src
)

link_directories(
    ${MONGOCXX_LIB_DIR}
)

add_library(
    mongodb-graph-extension SHARED
    src/mongo/graph_extension.cpp
    src/mongo/path_finding.cpp
)

target_link_libraries(
    mongodb-graph-extension
    mongocxx
    bsoncxx
)

add_executable(
    path_example
    examples/path_example.cpp
)

target_link_libraries(
    path_example
    mongodb-graph-extension
    mongocxx
    bsoncxx
)