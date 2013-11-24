# 
# hCraft's CMakeLists file.
# 

cmake_minimum_required(VERSION 2.8)
project(hCraft)

file(GLOB_RECURSE hCraft_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE hCraft_HEADERS ${CMAKE_SOURCE_DIR}/include/*.hpp)

add_subdirectory(cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

include_directories(src/ include/)
add_executable(hCraft ${hCraft_SOURCES} ${hCraft_HEADERS})

#
# Dependencies:
#

set(MISSING_LIB 0)
set(MISSING_LIB_OVERRIDE 1)

find_package(Pthread)
if(${PTHREAD_FOUND})
  target_link_libraries(hCraft ${PTHREAD_LIBRARIES})
else()
  message(WARNING "PTHREAD NOT FOUND")
  set(MISSING_LIB 1)
endif()

find_package(CryptoPP)
if(${CRYPTOPP_FOUND})
  target_link_libraries(hCraft ${CRYPTOPP_LIBRARIES})
else()
  message(WARNING "Crypto++ NOT FOUND")
  set(MISSING_LIB 1)
endif()

find_package(CURL)
if(${CURL_FOUND})
  target_link_libraries(hCraft ${CURL_LIBRARIES})
else()
  message(WARNING "CURL NOT FOUND")
  set(MISSING_LIB 1)
endif()

find_package(LibEvent)
if(${LibEvent_FOUND})
  target_link_libraries(hCraft ${LIBEVENT_LIB})
else()
  message(WARNING "LibEvent NOT FOUND")
  set(MISSING_LIB 1)
endif()

find_package(LibNoise)
if(${LIBNOISE_FOUND})
  target_link_libraries(hCraft ${LIBNOISE_LIBRARY})
else()
  message(WARNING "LibNoise NOT FOUND")
  set(MISSING_LIB 1)
endif()

find_package(mysql)
if(${MYSQL_FOUND})
  target_link_libraries(hCraft ${MYSQL_LIBRARIES})
else()
  message(WARNING "MySQL NOT FOUND")
  set(MISSING_LIB 1)
endif()

find_package(Soci)
if(${SOCI_LIBRARY})
  target_link_libraries(hCraft ${SOCI_LIBRARY} ${SOCI_mysql_PLUGIN})
else()
  message(WARNING "SOCI NOT FOUND")
  set(MISSING_LIB 1)
endif()

find_package(TBB)
if(${TBB_FOUND})
  target_link_libraries(hCraft ${TBB_LIBRARIES})
else()
  message(WARNING "TBB NOT FOUND")
  set(MISSING_LIB 1)
endif()

find_package(ZLIB)
if(${ZLIB_FOUND})
  target_link_libraries(hCraft ${ZLIB_LIBRARIES})
else()
  message(WARNING "ZLIB NOT FOUND")
  set(MISSING_LIB 1)
endif()

if(MISSING_LIB_OVERRIDE)
  message(STATUS "MISSING LIBRARY OVERRIDE IS ENABLED PLEASE MAKE SURE MISSING LIBRARIES ARE LOCATED MANUALY")
  message(STATIS "This warning is also enabled because of issues with CryptoPPfind.cmake"
else()
  if(MISSING_LIB)
    message(FATAL_ERROR "There are libraries required for hCraft missing or not found on your system EXITING")
  endif(MISSING_LIB)
endif(MISSING_LIB_OVERRIDE)

#HACKS

#CryptoPP Find package is broken doesn't find correct LIB paths on Arch
set(CRYPTOPP_LIB_HACK /usr/lib/libcryptopp.so)

#Another hack to also link aginst libevent_pthreads to resolve evthread_use_pthreads undefined
set(pthreadEVENT_LIB /usr/lib/libevent_pthreads-2.0.so.5)

target_link_libraries(hCraft ${CRYPTOPP_LIB_HACK} ${pthreadEVENT_LIB})

#HACKS_END

include_directories(${CRYPTOPP_INCLUDE_DIR} ${CURL_INCLUDE_DIRS} ${LIBEVENT_INCLUDE_DIR}
${LIBNOISE_INCLUDE_DIR} ${MYSQL_INCLUDE_DIR} ${SOCI_INCLUDE_DIRS} ${TBB_INCLUDE_DIRS})


install(TARGETS hCraft RUNTIME DESTINATION bin)

if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "-O3 -std=c++11") ## Optimize
    set(CMAKE_EXE_LINKER_FLAGS "-s")      ## Strip binary
elseif(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "-g -O0 -std=c++11")
endif(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_BUILD_TYPE MATCHES Release)
