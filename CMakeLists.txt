cmake_minimum_required(VERSION 3.8)

project(sdbus-test C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDBUS REQUIRED libsystemd)


add_executable(sdbus-test main.c)


# SD-Bus
target_link_libraries(sdbus-test PRIVATE ${SDBUS_LIBRARIES})
target_include_directories(sdbus-test PUBLIC ${SDBUS_INCLUDE_DIRS})
target_compile_options(sdbus-test PUBLIC ${SDBUS_CFLAGS_OTHER})
# Additional linux libs
target_link_libraries(sdbus-test PUBLIC pthread util)