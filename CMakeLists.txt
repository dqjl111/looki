cmake_minimum_required(VERSION 3.5)

set(LOOKI_VERSION 0.0.1)
project(looki VERSION ${LOOKI_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mwindows")

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_definitions(_GLIBCXX_USE_WCHAR_T)

option(looki_CMAKE_ENABLE_TEST "enable test" OFF)
option(looki_BUILD_NAMESPACE "build lk namespace" OFF)
option(looki_BUILD_SHARED "build lk shared" OFF)
option(looki_BUILD_3D "build lk 3D" OFF)

option(looki_USE_SSL "build with ssl" OFF)
option(looki_USE_SQLITE "build with sqlite" OFF)

# lkmedia 是音频(LSound/miniaudio) + 视频(LVideoWidget/LVideoPlayer) 的单一库，
# 由 looki_BUILD_SHARED 决定编成静态库还是 DLL。
option(looki_BUILD_VIDEO "build the video part of lkmedia (LVideoWidget + backend)" ON)
option(looki_VIDEO_AUDIO_DUCKING "duck lkmedia's miniaudio master volume while a video plays" ON)

# lkmedia 的视频后端：静态编译期选择，不生成插件。
# Windows 默认 Media Foundation；其它平台默认 Null（保证可编译，播放返回失败）。
# 日后接 Linux：写好 lvideobackendgstreamer.cpp/.h，把这里设为 GStreamer 即可。
if(WIN32)
    set(looki_VIDEO_BACKEND "MF" CACHE STRING "lkmedia backend: MF or Null")
else()
    set(looki_VIDEO_BACKEND "Null" CACHE STRING "lkmedia backend: Null or GStreamer")
endif()
set_property(CACHE looki_VIDEO_BACKEND PROPERTY STRINGS MF GStreamer Null)

set(LOOKI_BUILD_VIDEO 0)
set(LOOKI_VIDEO_BACKEND_MF 0)
set(LOOKI_VIDEO_BACKEND_GSTREAMER 0)
set(LOOKI_VIDEO_BACKEND_NULL 0)
set(LOOKI_VIDEO_AUDIO_DUCKING 0)

if(looki_BUILD_VIDEO)
    set(LOOKI_BUILD_VIDEO 1)
    if(looki_VIDEO_AUDIO_DUCKING)
        set(LOOKI_VIDEO_AUDIO_DUCKING 1)
    endif()
    if(looki_VIDEO_BACKEND STREQUAL "MF")
        if(NOT WIN32)
            message(FATAL_ERROR "looki_VIDEO_BACKEND=MF is only available on Windows")
        endif()
        set(LOOKI_VIDEO_BACKEND_MF 1)
    elseif(looki_VIDEO_BACKEND STREQUAL "GStreamer")
        set(LOOKI_VIDEO_BACKEND_GSTREAMER 1)
    elseif(looki_VIDEO_BACKEND STREQUAL "Null")
        set(LOOKI_VIDEO_BACKEND_NULL 1)
    else()
        message(FATAL_ERROR "Unknown looki_VIDEO_BACKEND=${looki_VIDEO_BACKEND}; use MF, GStreamer or Null")
    endif()
endif()


set(looki_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source)
if(WIN32)
set(VCPKG_ROOT "D:/vcpkg/vcpkg-2026.07.29"  CACHE STRING "vcpkg root")
set(CMAKE_TOOLCHAIN_FILE ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
# set(VCPKG_INCLUDE_DIR ${VCPKG_ROOT}/installed/x64-windows/include)
# include_directories(${VCPKG_INCLUDE_DIR})
endif()


if(looki_BUILD_NAMESPACE)
  set(LOOKI_NAMESPACE 1)
else()
  set(LOOKI_NAMESPACE 0)
endIf()

if(looki_BUILD_SHARED)
  set(LOOKI_BUILD_SHARED 1)
else()
  set(LOOKI_BUILD_SHARED 0)
endIf()

set(looki_DEPEND_LIBRARIES "")
set(looki_SSL_LIBRARIES "")
set(looki_VIDEO_LIBRARIES "")

if(looki_USE_SSL)
  find_package(OpenSSL REQUIRED)
  list(APPEND looki_SSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
endIf()

if(looki_USE_SQLITE)
  find_package(unofficial-sqlite3 CONFIG REQUIRED)
endif()

if(CMAKE_CXX_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
  set(LOOKI_LITTLE_ENDIAN 1)
else()
  set(LOOKI_LITTLE_ENDIAN 0)
endif()


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lookiconfig.h.in ${looki_SOURCE_DIR}/lkcore/lookiconfig.h)

# COMMON LIBRARIES
if (UNIX)
    list(APPEND looki_DEPEND_LIBRARIES pthread)
endif()

set(WINENTRY "")
if (MINGW)
  #link_directories(${looki_SOURCE_DIR}/lk3rd/libs-mingw/x86/)
  set(WINENTRY WIN32)
  list(APPEND looki_DEPEND_LIBRARIES pthread)
endif()


if(WIN32)
    #link_directories(${looki_SOURCE_DIR}/lk3rd/libs-msvc/x86/)
  set(WINENTRY WIN32)
  list(APPEND looki_DEPEND_LIBRARIES imm32 dwmapi d3d11 d3dcompiler dxgi d2d1 dwrite)
  if(LOOKI_VIDEO_BACKEND_MF)
    # mfplat/mfuuid: Media Foundation + GUID 符号；ole32: CoCreateInstance；
    # oleaut32: SysAllocString (IMFMediaEngine::SetSource 需要 BSTR)。
    list(APPEND looki_VIDEO_LIBRARIES mfplat mfuuid ole32 oleaut32)
  endif()
endif()


set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(${looki_SOURCE_DIR})

set(LRCC_GENER ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lrcc${CMAKE_EXECUTABLE_SUFFIX})
include(CMake/LRcc.cmake)

if(looki_CMAKE_ENABLE_TEST AND (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR))
  enable_testing()
  if(NOT MSVC)
      add_compile_options(-Wall -Wextra)
  endif()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()

# INSTALL DIRECTIVES
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/source/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test/ DESTINATION test)
install(DIRECTORY ${CMAKE_BINARY_DIR}/lib/ DESTINATION lib PATTERN "*.exp" EXCLUDE)
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/ DESTINATION bin PATTERN "*.ilk" EXCLUDE PATTERN "*.pdb" EXCLUDE PATTERN "*.db" EXCLUDE)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/CMake/ DESTINATION CMake)
