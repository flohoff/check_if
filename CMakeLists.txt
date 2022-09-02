cmake_minimum_required (VERSION 2.6)
project (checkif)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

find_package(Boost 1.55.0 REQUIRED COMPONENTS program_options filesystem system)
include_directories(${Boost_INCLUDE_DIRS})

find_package(Protobuf REQUIRED)
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS IfState.proto)

find_package(PkgConfig)
pkg_check_modules(SNMPXX REQUIRED snmp++-3)

add_executable(checkif checkif.cpp ${PROTO_SRCS} ${PROTO_HDRS})

# Needed for protobuf generated files
target_include_directories(checkif PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(checkif ${Boost_LIBRARIES})
target_link_libraries(checkif ${SNMPXX_LIBRARIES})
target_link_libraries(checkif pthread)
target_link_libraries(checkif crypto)
target_link_libraries(checkif ${PROTOBUF_LIBRARIES})
