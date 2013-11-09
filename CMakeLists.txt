# 
# hCraft's CMakeLists file.
# 

cmake_minimum_required(VERSION 2.8)
project(hCraft)

file(GLOB_RECURSE hCraft_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE hCraft_HEADERS ${CMAKE_SOURCE_DIR}/include/*.hpp)

add_subdirectory(cmake)
set(CMAKE_CXX_FLAGS "-g -O0 -std=c++11")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

include_directories(src/ include/)
add_executable(hCraft ${hCraft_SOURCES} ${hCraft_HEADERS})


#
# Dependencies:
#

find_package(Pthread)
find_package(CryptoPP)
#CryptoPP Find package is broken doesn't find correct LIB paths on Arch
#HACK
set(CRYPTOPP_LIB_HACK /usr/lib/libcryptopp.so)

find_package(CURL)
find_package(LibEvent)
#Another hack to also link aginst libevent_pthreads to resolve evthread_use_pthreads undefined
set(pthreadEVENT_LIB /usr/lib/libevent_pthreads-2.0.so.5)

find_package(LibNoise)
find_package(mysql)
find_package(Soci)
find_package(TBB)
find_package(ZLIB)

include_directories(${CRYPTOPP_INCLUDE_DIR} ${CURL_INCLUDE_DIRS} ${LIBEVENT_INCLUDE_DIR}
${LIBNOISE_INCLUDE_DIR} ${MYSQL_INCLUDE_DIR} ${SOCI_INCLUDE_DIRS} ${TBB_INCLUDE_DIRS})

message("${include_directories}")

target_link_libraries(hCraft ${PTHREAD_LIBRARIES} ${SOCI_LIBRARY} ${SOCI_mysql_PLUGIN} ${pthreadEVENT_LIB} ${CRYPTOPP_LIB_HACK} ${CURL_LIBRARIES} ${MYSQL_LIBRARIES} 
${LIBEVENT_LIB} ${LIBNOISE_LIBRARY} ${TBB_LIBRARIES} ${ZLIB_LIBRARIES})

install(TARGETS hCraft RUNTIME DESTINATION bin)

if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "-O3 -std=c++11") ## Optimize
    set(CMAKE_EXE_LINKER_FLAGS "-s")      ## Strip binary
endif(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_BUILD_TYPE MATCHES Release)

