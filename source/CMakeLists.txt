cmake_minimum_required(VERSION 3.5)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkentrypoint)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkcore)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lktool)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lknetwork)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lksql)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkaudio)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkgui)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkwidgets)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkplugins)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkgraph)


if(looki_BUILD_3D)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lk3D)
endif()
