include_directories(${PROJECT_SOURCE_DIR}/deps/mdp)
include_directories(${PROJECT_SOURCE_DIR}/deps/sandbox)

link_libraries(zmq)
add_executable(broker broker.cpp)
add_executable(client client.cpp)
add_executable(compiler_g++ compiler_g++.cpp)
add_executable(tester tester.cpp)

find_library(SANDBOX_LIB sandbox HINTS ${PROJECT_SOURCE_DIR}/deps/sandbox)
target_link_libraries(tester ${SANDBOX_LIB} pthread seccomp)

configure_file(${PROJECT_SOURCE_DIR}/tmp_files/echo.cpp ${CMAKE_CURRENT_BINARY_DIR}/echo.cpp COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/tmp_files/illed_echo.cpp ${CMAKE_CURRENT_BINARY_DIR}/illed_echo.cpp COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/tmp_files/wrong_echo.cpp ${CMAKE_CURRENT_BINARY_DIR}/wrong_echo.cpp COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/tmp_files/1.in ${CMAKE_CURRENT_BINARY_DIR}/1.in COPYONLY)
