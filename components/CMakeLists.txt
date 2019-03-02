include_directories(${PROJECT_SOURCE_DIR}/mdp)

add_executable(broker broker.cpp)
add_executable(client client.cpp)
add_executable(compiler_g++ compiler_g++.cpp)
add_executable(tester tester.cpp)

target_link_libraries(broker zmq)
target_link_libraries(client zmq)
target_link_libraries(compiler_g++ zmq)
target_link_libraries(tester zmq)
