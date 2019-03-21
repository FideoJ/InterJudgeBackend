include_directories(${PROJECT_SOURCE_DIR}/deps/mdp)
include_directories(${PROJECT_SOURCE_DIR}/deps/sandbox)

link_libraries(zmq)
add_executable(broker broker.cpp)
add_executable(client client.cpp)
add_executable(compiler_gpp compiler_gpp.cpp)
add_executable(tester tester.cpp)

find_library(SANDBOX_LIB sandbox HINTS ${PROJECT_SOURCE_DIR}/deps/sandbox)
target_link_libraries(tester ${SANDBOX_LIB} pthread seccomp)
