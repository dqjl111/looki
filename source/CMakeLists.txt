cmake_minimum_required(VERSION 3.5)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkentrypoint)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkcore)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lktool)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lknetwork)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lksql)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkgui)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkwidgets)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkplugins)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkgraph)

# lkmedia 合并了音频与视频：视频控件依赖 lkwidgets / lkgui，
# 所以放在它们之后。库本身始终编译，视频部分由 looki_BUILD_VIDEO 控制。
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lkmedia)


if(looki_BUILD_3D)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lk3D)
endif()
