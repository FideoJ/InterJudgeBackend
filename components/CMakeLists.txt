find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROJECT_SOURCE_DIR}/proto/file_provider.proto)
add_library(protocol ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(protocol protobuf)

include_directories(${PROJECT_SOURCE_DIR}/deps/mdp)
include_directories(${PROJECT_SOURCE_DIR}/deps/sandbox)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

link_libraries(zmq)
add_executable(broker broker.cpp)
add_executable(client client.cpp)
add_executable(compiler_gpp compiler_gpp.cpp)
add_executable(tester tester.cpp)
add_executable(file_provider file_provider.cpp)

find_library(SANDBOX_LIB sandbox HINTS ${PROJECT_SOURCE_DIR}/deps/sandbox)
target_link_libraries(tester ${SANDBOX_LIB} pthread seccomp)

target_link_libraries(file_provider protocol)
