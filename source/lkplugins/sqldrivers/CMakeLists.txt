cmake_minimum_required(VERSION 3.5)

if(looki_USE_SQLITE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sqlite)
endif()