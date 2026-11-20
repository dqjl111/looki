cmake_minimum_required(VERSION 3.5)

# lkmedia：音频(LSound/miniaudio) + 视频(LVideoPlayer/LVideoWidget) 合并后的单一库。
# 支持静态与动态(SHARED)两种构建；平台后端由顶层 looki_VIDEO_BACKEND
# 生成的 LOOKI_VIDEO_BACKEND_* 宏 + 下面的选择性源文件共同决定。
if(looki_BUILD_SHARED)
    add_library(lkmedia SHARED)
else()
    add_library(lkmedia)
endif()

target_compile_definitions(lkmedia PRIVATE LKBUILD_MEDIA)
target_compile_features(lkmedia PUBLIC cxx_std_23)
target_include_directories(lkmedia PUBLIC ${looki_SOURCE_DIR})

# miniaudio / stb_vorbis 是单编译单元实现，直接编进 lkmedia。
# 合并前它们是独立的 miniaudio 静态库；合并后不再单独输出一个 .lib，
# 动态构建时也保证实现与使用方在同一个 DLL 里。
set(LKMEDIA_THIRDPARTY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd/miniaudio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd/stb_vorbis.cpp
)
set(LKMEDIA_THIRDPARTY_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd/miniaudio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd/stb_vorbis.h
)

# ---- 音频部分：始终编入 ----
set(LKMEDIA_SOURCES
    ${LKMEDIA_THIRDPARTY_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/laudiodata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lsound.cpp
)
set(LKMEDIA_HEADERS
    ${LKMEDIA_THIRDPARTY_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/laudiodata.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lsound.h
)

# ---- 视频部分：由 looki_BUILD_VIDEO 选择性编入 ----
if(looki_BUILD_VIDEO)
    list(APPEND LKMEDIA_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideoaudiobridge.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideowindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideowidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackendnull.cpp
    )
    list(APPEND LKMEDIA_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideo.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackend.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideoaudiobridge.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideowindow.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideowidget.h
        ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackendnull.h
    )

    if(LOOKI_VIDEO_BACKEND_MF)
        list(APPEND LKMEDIA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackendmf.cpp)
        list(APPEND LKMEDIA_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackendmf.h)
    endif()

    if(LOOKI_VIDEO_BACKEND_GSTREAMER)
        if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackendgstreamer.cpp)
            message(FATAL_ERROR
                "looki_VIDEO_BACKEND=GStreamer but lvideobackendgstreamer.cpp is missing. "
                "Implement LVideoBackendGStreamer (see lvideobackend.h) and drop the files "
                "into source/lkmedia/ to enable it.")
        endif()
        list(APPEND LKMEDIA_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackendgstreamer.cpp)
        list(APPEND LKMEDIA_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/lvideobackendgstreamer.h)
    endif()
endif()

target_sources(lkmedia PRIVATE ${LKMEDIA_SOURCES} ${LKMEDIA_HEADERS})

# PUBLIC：静态构建时依赖必须传递给最终链接的 exe；动态构建时无害。
target_link_libraries(lkmedia PUBLIC
    lkcore
    ${looki_DEPEND_LIBRARIES})

if(looki_BUILD_VIDEO)
    # LVideoWidget 继承 LWidget，帧适配层要把 D3D11 纹理包成 LPixmap。
    # MF 后端还需要 mfplat/mfuuid/ole32/oleaut32。
    target_link_libraries(lkmedia PUBLIC
        lkwidgets lkgui
        ${looki_VIDEO_LIBRARIES})
endif()

list(APPEND INSTALL_LIBS lkmedia)
if(looki_BUILD_SHARED)
    list(APPEND INSTALL_BINS lkmedia)
endif()
