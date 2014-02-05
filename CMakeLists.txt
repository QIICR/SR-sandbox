project(SRexperiments)
cmake_minimum_required(VERSION 2.8)
find_package(DCMTK REQUIRED)

include_directories(${DCMTK_INCLUDE_DIRS})
add_executable(tid1411test tid1411test.cxx)
target_link_libraries(tid1411test ${DCMTK_LIBRARIES} xml2 z)
