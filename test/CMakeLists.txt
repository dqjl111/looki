cmake_minimum_required(VERSION 3.5)


add_executable(lktestsignal ${CMAKE_CURRENT_SOURCE_DIR}/testsignal.cpp)
target_include_directories(lktestsignal PRIVATE ${looki_SOURCE_DIR})
target_link_libraries(lktestsignal lkentrypoint lkcore)


list(APPEND TESTWIN_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/edeviceuser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/edeviceuser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testwin.cpp
)

add_executable(lktestwin ${WINENTRY} ${TESTWIN_FILES})
target_include_directories(lktestwin PRIVATE ${looki_SOURCE_DIR})
target_link_libraries(lktestwin lkentrypoint lkcore lkgui lkwidgets lkmedia lknetwork lksql lksvg)
lrcc_add_resources(lktestwin ${CMAKE_CURRENT_SOURCE_DIR}/res.lrc)


# lkmedia 测试程序：独立 GUI 窗口，验证 LVideoWidget + 当前平台的视频后端。
# 保留 console 子系统，便于直接看到 lWarn() 的日志输出。
# 用法：lktestvideo [视频路径]
if(looki_BUILD_VIDEO)
    add_executable(lktestvideo ${CMAKE_CURRENT_SOURCE_DIR}/testvideo.cpp)
    target_include_directories(lktestvideo PRIVATE ${looki_SOURCE_DIR})
    target_link_libraries(lktestvideo lkentrypoint lkcore lkgui lkwidgets lkmedia)
endif()

# 独立 D3D12 RHI 测试程序：直接创建 LWindow + SwapChain 并持续绘制三角形。
if(WIN32 AND looki_BUILD_3D)
    add_executable(lktestd3d12 ${CMAKE_CURRENT_SOURCE_DIR}/testd3d12.cpp)
    target_include_directories(lktestd3d12 PRIVATE ${looki_SOURCE_DIR})
    target_link_libraries(lktestd3d12 lkentrypoint lkcore lkgui)

    # 独立 D3D11 RHI 测试程序，覆盖 swapchain/PSO/SRB/纹理上传/深度绘制。
    add_executable(lktestd3d11 ${CMAKE_CURRENT_SOURCE_DIR}/testd3d11.cpp)
    target_include_directories(lktestd3d11 PRIVATE ${looki_SOURCE_DIR})
    target_link_libraries(lktestd3d11 lkentrypoint lkcore lkgui)
endif()
